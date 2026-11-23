"""Regression tests for TTS speaker tuning; no ESP32 or Fish key required."""

from __future__ import annotations

import math
import wave
from array import array

from mazhost.audio_profiles import PROFILES, shape_speech_wav
from mazhost.config import Settings
from mazhost.tts import SpeechOut


def _tone(path, hz=440, rate=16000, amp=32000, length_s=1.0):
    values = array("h", (round(amp * math.sin(2 * math.pi * hz * i / rate))
                         for i in range(round(rate * length_s))))
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(values.tobytes())


def _values(path):
    with wave.open(str(path), "rb") as src:
        assert (src.getnchannels(), src.getsampwidth()) == (1, 2)
        values = array("h")
        values.frombytes(src.readframes(src.getnframes()))
        return src.getframerate(), values


def _rms(values):
    # Discard the attack/release edges when comparing signal levels.
    v = values[200:-200]
    return math.sqrt(sum(x * x for x in v) / max(1, len(v)))


def test_cardputer_adv_preserves_wav_format_with_safe_peak_and_fades(tmp_path):
    wav = tmp_path / "voice.wav"
    _tone(wav, hz=440, amp=32760)
    assert shape_speech_wav(wav, "cardputer_adv") is True
    rate, samples = _values(wav)
    assert rate == 16000 and len(samples) == 16000
    assert max(abs(x) for x in samples) <= round(PROFILES["cardputer_adv"].max_peak * 32767) + 1
    assert abs(samples[0]) < 1000
    assert abs(samples[-1]) < 1000


def test_bass_reduction_for_tiny_speaker(tmp_path):
    low, speech = tmp_path / "low.wav", tmp_path / "speech.wav"
    _tone(low, hz=70, amp=16000)
    _tone(speech, hz=1000, amp=16000)
    assert shape_speech_wav(low, "cardputer_adv")
    assert shape_speech_wav(speech, "cardputer_adv")
    _, lv = _values(low)
    _, sv = _values(speech)
    assert _rms(lv) < _rms(sv) * 0.5


def test_flat_is_byte_exact_and_malformed_audio_is_untouched(tmp_path):
    wav = tmp_path / "voice.wav"
    _tone(wav)
    original = wav.read_bytes()
    assert shape_speech_wav(wav, "flat") is False
    assert wav.read_bytes() == original
    wav.write_bytes(b"bad wav content")
    assert shape_speech_wav(wav, "cardputer_adv") is False
    assert wav.read_bytes() == b"bad wav content"


def test_profiles_are_distinct_and_reversible(tmp_path):
    values = {}
    for profile in ("cardputer_adv", "prototype_small", "prototype_large", "headphones"):
        wav = tmp_path / (profile + ".wav")
        _tone(wav, hz=200, amp=16000)
        assert shape_speech_wav(wav, profile) is True
        values[profile] = _rms(_values(wav)[1])
    assert len({round(x) for x in values.values()}) == 4


def test_cache_keys_change_with_profile(tmp_path):
    kw = {"_env_file": None, "token": "test-token", "tts_enabled": True,
          "fish_api_key": "test-key", "data_dir": str(tmp_path)}
    one = SpeechOut(Settings(**kw, audio_output_profile="cardputer_adv"))
    two = SpeechOut(Settings(**kw, audio_output_profile="flat"))
    assert one._cache_file("Done.", "voice") != two._cache_file("Done.", "voice")
