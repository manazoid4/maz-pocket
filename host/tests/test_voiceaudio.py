from __future__ import annotations

import wave

import numpy as np
import pytest

from mazhost import voiceaudio
from mazhost.config import Settings
from mazhost.speakplan import MAX_PARTS, SpeakPlanner, TimingLog, plan_parts
from mazhost.tts import SpeechOut


def _wav(path, x, rate):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes((x * 32767).astype("<i2").tobytes())


def _read(path):
    with wave.open(str(path), "rb") as w:
        assert (w.getnchannels(), w.getsampwidth(), w.getframerate()) == (1, 2, 16000)
        return np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float32) / 32768


def test_24k_float_becomes_16k_mono_normalised(tmp_path):
    t = np.arange(24000) / 24000
    voiceaudio.write_device_wav((0.05 * np.sin(2 * np.pi * 300 * t)).astype(np.float32), 24000, tmp_path / "a.wav")
    y = _read(tmp_path / "a.wav")
    assert abs(len(y) - 16000) < 5  # 1 s stays 1 s
    rms_db = 20 * np.log10(np.sqrt(np.mean(y * y)))
    assert abs(rms_db - voiceaudio.TARGET_RMS_DBFS) < 1.5 or np.abs(y).max() > 0.85
    assert np.abs(y).max() <= 0.9


def test_loud_input_is_limited_not_clipped(tmp_path):
    x = np.zeros(16000, dtype=np.float32); x[::50] = 1.0; x[1::50] = -1.0  # spiky, high crest
    voiceaudio.write_device_wav(x, 16000, tmp_path / "b.wav")
    assert np.abs(_read(tmp_path / "b.wav")).max() <= 0.9


def test_normalize_wav_in_place_and_ignores_garbage(tmp_path):
    _wav(tmp_path / "c.wav", 0.01 * np.sin(np.arange(8000) / 5.0), 16000)
    assert voiceaudio.normalize_wav(tmp_path / "c.wav")
    assert np.abs(_read(tmp_path / "c.wav")).max() > 0.1  # quiet clip was brought up
    bad = tmp_path / "d.wav"; bad.write_bytes(b"RIFF" + b"\x00" * 40)
    assert voiceaudio.normalize_wav(bad) is False and bad.read_bytes().startswith(b"RIFF")


def _settings(tmp_path, **kw):
    return Settings(_env_file=None, token="t" * 24, data_dir=str(tmp_path / "data"),
                    tts_enabled=True, **kw)


def test_default_provider_is_fish_never_kokoro(tmp_path):
    s = _settings(tmp_path)
    assert s.tts_provider == "fish" and SpeechOut(s).kokoro is None


def test_kokoro_provider_synthesises_without_cloud(tmp_path, monkeypatch):
    class FakeK:
        def create(self, text, voice, speed, lang):
            assert voice == "bf_emma" and lang == "en-gb"
            return (0.2 * np.sin(np.arange(24000) / 7.0)).astype(np.float32), 24000
    monkeypatch.setattr(voiceaudio.KokoroEngine, "_load", lambda self: FakeK())
    out = SpeechOut(_settings(tmp_path, tts_provider="kokoro"))
    path = out.synthesize("Hello there.")
    assert out.last_provider == "kokoro" and len(_read(path)) > 15000


def test_kokoro_missing_models_is_a_clear_error_not_a_silent_switch(tmp_path):
    out = SpeechOut(_settings(tmp_path, tts_provider="kokoro", kokoro_dir=str(tmp_path / "none")))
    with pytest.raises(RuntimeError, match="kokoro_"):
        out.synthesize("Hello there.")
    assert out.last_provider == ""  # did not fall back to fish or windows


def test_plan_parts_streams_per_sentence_chunks_capped():
    text = "First one is quick. " + " ".join(f"Sentence number {i} has a fair amount of words in it, honestly." for i in range(8))
    parts = plan_parts(text)
    assert parts[0] == "First one is quick."
    assert 2 <= len(parts) <= MAX_PARTS and " ".join(parts) == text


def test_planner_logs_ms_per_part():
    import tempfile
    from pathlib import Path

    def synth(t):
        f = tempfile.NamedTemporaryFile(delete=False, suffix=".wav"); f.write(b"x"); f.close()
        return Path(f.name)
    row = {"stt": 1, "llm": 1}
    p = SpeakPlanner(synth, TimingLog(5))
    pid, n = p.start("Do it now. " + "Then check the logs and report back to me when you finish all of it. " * 3, row)
    for i in range(n):
        p.part(pid, i)
    import time; time.sleep(0.1)
    assert len(row["tts_parts_ms"]) == n and all(isinstance(v, int) for v in row["tts_parts_ms"])
    assert "tts_parts_ms=" in TimingLog.line(row)
