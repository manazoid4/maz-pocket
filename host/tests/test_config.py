"""MAZ Host defaults must be explicit, local-first and safe."""

from __future__ import annotations

from mazhost.config import Settings


def test_defaults_are_local_first_and_conservative():
    s = Settings(_env_file=None)
    assert s.port == 8787
    assert s.default_route == "local"
    assert s.ollama_model == "lfm2.5-8b-a1b-gpu:latest"
    assert s.tts_enabled is False, "TTS must be opt-in so it cannot delay answers"
    assert s.max_audio_seconds == 900
