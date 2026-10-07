from __future__ import annotations

from pathlib import Path

from mazhost.config import Settings
from mazhost.stt import SpeechToText


def test_turn_stt_prefers_groq_then_falls_back_local(tmp_path, monkeypatch):
    wav = tmp_path / "a.wav"
    wav.write_bytes(b"RIFF" + b"\0" * 60)
    stt = SpeechToText(Settings(groq_key="k"))
    seen = []
    monkeypatch.setattr(stt, "_groq", lambda p, prompt: seen.append("groq") or "from groq")
    monkeypatch.setattr(stt, "transcribe", lambda p: "from local")
    assert stt.transcribe_turn(Path(wav)) == "from groq"

    def boom(p, prompt):
        raise RuntimeError("down")
    monkeypatch.setattr(stt, "_groq", boom)
    assert stt.transcribe_turn(Path(wav)) == "from local"

    keyless = SpeechToText(Settings())
    monkeypatch.setattr(keyless, "transcribe", lambda p: "from local")
    assert keyless.transcribe_turn(Path(wav)) == "from local"
