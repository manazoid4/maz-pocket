"""Settings must read MAZ_* env vars and refuse to look secure when it is not."""

from __future__ import annotations

from mazhost.config import Settings


def test_defaults_are_conservative():
    s = Settings(_env_file=None)
    assert s.port == 8787
    assert s.default_route == "auto"
    assert s.tts_enabled is False, "TTS must be opt-in so it cannot delay answers"
    assert s.max_audio_seconds == 900
