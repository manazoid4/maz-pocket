from __future__ import annotations

import json

import httpx
import pytest

from mazhost.config import Settings
from mazhost.errors import ErrorCode, RouteError
from mazhost.llm import Models


def settings(**values):
    return Settings(
        token="test-token-that-is-not-default",
        ollama_model="primary:test",
        ollama_backup_model="backup:test",
        _env_file=None,
        **values,
    )


def test_auto_policy_orders_primary_then_backup():
    models = Models(settings(local_model_policy="auto"))
    assert models._local_models() == ["primary:test", "backup:test"]


def test_backup_policy_can_force_low_vram_model():
    models = Models(settings(local_model_policy="backup"))
    assert models._local_models() == ["backup:test"]


def test_interactive_profiles_keep_models_warm_and_bound_output():
    smart = Models(settings(ai_profile="smart"))
    fast = Models(settings(ai_profile="fast"))
    save = Models(settings(ai_profile="save"))
    messages = [{"role": "user", "content": "give me the next action"}]

    assert smart._keep_alive() == "15m"
    assert fast._keep_alive() == "60m"
    assert save._keep_alive() == 0
    assert smart._options("primary:test", messages)["num_predict"] == 160


def test_local_route_falls_back_without_cloud(monkeypatch):
    models = Models(settings(local_model_policy="auto"))
    calls: list[str] = []

    def local_one(model, _messages):
        calls.append(model)
        if model == "primary:test":
            raise RuntimeError("gpu_busy")
        return "backup ready", f"local:{model}"

    monkeypatch.setattr(models, "_local_one", local_one)
    monkeypatch.setattr(
        models,
        "_cloud",
        lambda _messages: (_ for _ in ()).throw(AssertionError("cloud used")),
    )

    text, provider = models.chat([{"role": "user", "content": "hi"}], "local")
    assert text == "backup ready"
    assert provider == "local:backup:test"
    assert calls == ["primary:test", "backup:test"]


def test_auto_uses_cloud_only_after_both_locals_fail(monkeypatch):
    models = Models(settings(local_model_policy="auto", cloud_key="test"))
    calls: list[str] = []

    def local_one(model, _messages):
        calls.append(model)
        raise RuntimeError("offline")

    monkeypatch.setattr(models, "_local_one", local_one)
    monkeypatch.setattr(models, "_cloud", lambda _messages: ("cloud ok", "cloud"))

    assert models.chat([{"role": "user", "content": "hi"}], "auto") == ("cloud ok", "cloud")
    assert calls == ["primary:test", "backup:test"]


def test_llamacpp_engine_uses_its_own_model_chain():
    models = Models(
        settings(
            local_engine="llamacpp",
            llamacpp_model="qwen3-4b",
            llamacpp_backup_model="gemma3-270m",
        )
    )
    assert models._local_models() == ["qwen3-4b", "gemma3-270m"]


def test_llamacpp_backup_model_can_live_on_a_second_server():
    models = Models(
        settings(
            local_engine="llamacpp",
            llamacpp_url="http://127.0.0.1:8080",
            llamacpp_model="qwen3-4b",
            llamacpp_backup_url="http://127.0.0.1:8081",
            llamacpp_backup_model="gemma3-270m",
        )
    )
    assert models._llamacpp_url("qwen3-4b") == "http://127.0.0.1:8080"
    assert models._llamacpp_url("gemma3-270m") == "http://127.0.0.1:8081"


def test_llamacpp_turn_is_non_streaming_and_bounded():
    seen: dict = {}

    def handler(request: httpx.Request) -> httpx.Response:
        assert request.url.path == "/v1/chat/completions"
        seen.update(json.loads(request.content.decode("utf-8")))
        return httpx.Response(
            200,
            json={
                "choices": [{"message": {"content": "local ready"}}],
                "usage": {"prompt_tokens": 41, "completion_tokens": 7},
                "timings": {"prompt_ms": 120.0, "predicted_ms": 380.0},
            },
        )

    client = httpx.Client(transport=httpx.MockTransport(handler))
    models = Models(
        settings(local_engine="llamacpp", llamacpp_model="qwen3-4b"),
        client=client,
    )

    text, provider = models.chat([{"role": "user", "content": "hi"}], "local")
    assert (text, provider) == ("local ready", "local:qwen3-4b")
    assert seen["stream"] is False
    assert seen["max_tokens"] == 160
    assert seen["chat_template_kwargs"] == {"enable_thinking": False}
    assert models._last_usage["engine"] == "llamacpp"
    assert models._last_usage["total_ms"] == 500


def test_llamacpp_thinking_only_reply_is_named_not_silently_empty():
    def handler(_request: httpx.Request) -> httpx.Response:
        return httpx.Response(
            200,
            json={
                "choices": [
                    {
                        "finish_reason": "length",
                        "message": {"content": "", "reasoning_content": "Okay, the user said hi"},
                    }
                ]
            },
        )

    models = Models(
        settings(local_engine="llamacpp", llamacpp_model="qwen3-4b"),
        client=httpx.Client(transport=httpx.MockTransport(handler)),
    )

    try:
        models._llamacpp_one("qwen3-4b", [{"role": "user", "content": "hi"}])
    except RuntimeError as error:
        assert str(error) == "local_model_spent_budget_thinking"
    else:
        raise AssertionError("empty content must not be returned as an answer")


def test_llamacpp_status_reports_engine_and_health():
    def handler(request: httpx.Request) -> httpx.Response:
        if request.url.path == "/health":
            return httpx.Response(200, json={"status": "ok"})
        return httpx.Response(404)

    models = Models(
        settings(local_engine="llamacpp", llamacpp_model="qwen3-4b"),
        client=httpx.Client(transport=httpx.MockTransport(handler)),
    )

    status = models.status()
    assert status["local"] is True
    assert status["local_engine"] == "llamacpp"
    assert status["local_model_installed"] is True
    assert status["backup_model_installed"] is False


def test_cloud_request_explicitly_disables_streaming_for_9router_compatibility():
    seen: dict = {}

    def handler(request: httpx.Request) -> httpx.Response:
        seen.update(json.loads(request.content.decode("utf-8")))
        return httpx.Response(
            200,
            json={"choices": [{"message": {"content": "cloud ready"}}]},
        )

    client = httpx.Client(transport=httpx.MockTransport(handler))
    models = Models(
        settings(
            cloud_key="test-cloud-key",
            cloud_url="http://9router.test/v1",
            cloud_model="cloud:test",
        ),
        client=client,
    )

    assert models.chat([{"role": "user", "content": "hi"}], "cloud") == ("cloud ready", "cloud")
    assert seen["stream"] is False


def test_mazlatest_uses_explicit_loopback_route_and_exact_model_without_credentials():
    seen: dict = {}

    def handler(request: httpx.Request) -> httpx.Response:
        assert str(request.url) == "http://localhost:20128/v1/chat/completions"
        assert "authorization" not in request.headers
        seen.update(json.loads(request.content.decode("utf-8")))
        return httpx.Response(
            200,
            json={"choices": [{"message": {"content": "MazLatest ready"}}]},
        )

    models = Models(
        settings(),
        client=httpx.Client(transport=httpx.MockTransport(handler)),
    )

    assert models.chat([{"role": "user", "content": "hi"}], "mazlatest") == (
        "MazLatest ready",
        "mazlatest:MazLatest",
    )
    assert seen["model"] == "MazLatest"
    assert seen["stream"] is False
    assert seen["max_tokens"] == 160


def test_explicit_mazlatest_failure_is_loud_and_never_falls_back(monkeypatch):
    models = Models(settings())
    monkeypatch.setattr(
        models,
        "_mazlatest",
        lambda _messages: (_ for _ in ()).throw(httpx.ConnectError("offline")),
    )
    monkeypatch.setattr(
        models,
        "_cloud",
        lambda _messages: (_ for _ in ()).throw(AssertionError("cloud fallback used")),
    )
    monkeypatch.setattr(
        models,
        "_local",
        lambda _messages: (_ for _ in ()).throw(AssertionError("local fallback used")),
    )

    with pytest.raises(RouteError) as raised:
        models.chat([{"role": "user", "content": "hi"}], "mazlatest")
    assert raised.value.code == ErrorCode.UPSTREAM_UNAVAILABLE
