"""MAZ Host defaults must be explicit, local-first and safe."""

from __future__ import annotations

import pytest
from pydantic import ValidationError

from mazhost.config import Settings


def test_defaults_are_local_first_and_conservative():
    s = Settings(_env_file=None)
    assert s.port == 8787
    assert s.default_route == "local"
    assert s.ollama_model == "lfm2.5-8b-a1b-gpu:latest"
    assert s.ai_profile == "smart"
    assert s.tts_enabled is False, "TTS must be opt-in so it cannot delay answers"
    assert s.max_audio_seconds == 900
    assert s.max_turns == 12, "Pocket history should stay bounded for prompt latency"
    assert s.mazlatest_url == "http://localhost:20128/v1"
    assert s.mazlatest_model == "MazLatest"


@pytest.mark.parametrize(
    "url",
    [
        "https://localhost:20128/v1",
        "http://192.168.1.20:20128/v1",
        "http://example.com:20128/v1",
        "http://user:secret@localhost:20128/v1",
        "http://localhost:20128/v1?token=secret",
    ],
)
def test_mazlatest_route_rejects_non_loopback_or_credential_bearing_urls(url):
    with pytest.raises(ValidationError, match="mazlatest_url must be"):
        Settings(mazlatest_url=url, _env_file=None)


def test_old_shipped_gemma_default_is_migrated_but_other_models_are_respected():
    old = Settings(ollama_model="gemma3:1b", _env_file=None)
    custom = Settings(ollama_model="qwen3.5:4b-q4_K_M", _env_file=None)
    assert old.ollama_model == "lfm2.5-8b-a1b-gpu:latest"
    assert custom.ollama_model == "qwen3.5:4b-q4_K_M"
