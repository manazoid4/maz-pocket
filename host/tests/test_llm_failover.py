from __future__ import annotations

import json

import httpx

from mazhost.config import Settings
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
