from __future__ import annotations

from mazhost.config import Settings
from mazhost.llm import Models


def settings(**values):
    return Settings(
        token="test-token-that-is-not-default",
        ollama_model="primary:test",
        ollama_backup_model="maz-pocket-lite:latest",
        _env_file=None,
        **values,
    )


def test_auto_policy_orders_primary_then_lite():
    models = Models(settings(local_model_policy="auto"))
    assert models._local_models() == ["primary:test", "maz-pocket-lite:latest"]


def test_backup_policy_can_force_low_vram_model():
    models = Models(settings(local_model_policy="backup"))
    assert models._local_models() == ["maz-pocket-lite:latest"]


def test_local_route_falls_back_to_lite_without_cloud(monkeypatch):
    models = Models(settings(local_model_policy="auto"))
    calls: list[str] = []

    def local_one(model, _messages):
        calls.append(model)
        if model == "primary:test":
            raise RuntimeError("gpu_busy")
        return "Pocket Lite ready", f"local:{model}"

    monkeypatch.setattr(models, "_local_one", local_one)
    monkeypatch.setattr(models, "_cloud", lambda _messages: (_ for _ in ()).throw(AssertionError("cloud used")))

    text, provider = models.chat([{"role": "user", "content": "hi"}], "local")
    assert text == "Pocket Lite ready"
    assert provider == "local:maz-pocket-lite:latest"
    assert calls == ["primary:test", "maz-pocket-lite:latest"]


def test_stream_failover_only_happens_before_first_delta(monkeypatch):
    models = Models(settings(local_model_policy="auto"))

    def stream_one(model, _messages):
        if model == "primary:test":
            raise RuntimeError("primary unavailable")
        yield "fast ", f"local:{model}"
        yield "assistant", f"local:{model}"

    monkeypatch.setattr(models, "_local_stream_one", stream_one)
    assert list(models.stream_chat([{"role": "user", "content": "hi"}], "local")) == [
        ("fast ", "local:maz-pocket-lite:latest"),
        ("assistant", "local:maz-pocket-lite:latest"),
    ]
