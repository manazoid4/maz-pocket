"""Device-native audio helpers: the Pocket plays 16 kHz mono 16-bit PCM WAV (src/audio/voice.h SAMPLE_RATE).

Everything Core sends is resampled to that once here and loudness-normalised, so the
ES8311 path does no work and replies sound equally loud whichever TTS made them.
Also the optional local Kokoro engine (kokoro-onnx, free, offline).
"""

from __future__ import annotations

import logging
import threading
import wave
from pathlib import Path

import numpy as np

log = logging.getLogger("uvicorn.error")

DEVICE_RATE = 16000
TARGET_RMS_DBFS = -21.0  # speech loudness target (small speaker: loud but not clipping)
PEAK_LIMIT = 0.89  # about -1 dBFS


def _resample(x: np.ndarray, src: int, dst: int) -> np.ndarray:
    """Polyphase-style: zero-stuff up, windowed-sinc low-pass, take every nth."""
    if src == dst:
        return x
    g = int(np.gcd(src, dst))
    up, down = dst // g, src // g
    taps = 16 * max(up, down) + 1
    cutoff = 0.5 / max(up, down)  # cycles/sample at the up-sampled rate (slightly under Nyquist of the lower rate)
    n = np.arange(taps) - taps // 2
    h = 2 * cutoff * np.sinc(2 * cutoff * n) * np.hamming(taps) * up
    stuffed = np.zeros(len(x) * up, dtype=np.float32)
    stuffed[::up] = x
    return np.convolve(stuffed, h.astype(np.float32), mode="same")[::down].astype(np.float32)


def write_device_wav(samples: np.ndarray, rate: int, path: Path) -> None:
    """float mono in [-1, 1] at any rate -> normalised 16 kHz mono 16-bit WAV."""
    x = _resample(np.asarray(samples, dtype=np.float32), rate, DEVICE_RATE)
    if x.size:
        x = x - float(x.mean())
        rms = float(np.sqrt(np.mean(x * x)))
        if rms > 1e-5:
            x = x * (10 ** (TARGET_RMS_DBFS / 20) / rms)
        peak = float(np.abs(x).max())
        if peak > PEAK_LIMIT:  # scale down rather than clip (tiny dynamic range loss, no distortion)
            x = x * (PEAK_LIMIT / peak)
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(DEVICE_RATE)
        w.writeframes(pcm.tobytes())


def normalize_wav(path: Path) -> bool:
    """Rewrite a WAV file in place as device-native normalised audio. False (file untouched) if unreadable."""
    try:
        with wave.open(str(path), "rb") as w:
            ch, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.readframes(w.getnframes())
        if width != 2 or not frames:
            return False
        x = np.frombuffer(frames, dtype="<i2").astype(np.float32) / 32768
        if ch > 1:
            x = x[: len(x) // ch * ch].reshape(-1, ch).mean(axis=1)
        write_device_wav(x, rate, path)
        return True
    except (wave.Error, EOFError, ValueError, OSError):
        return False


class KokoroEngine:
    """Lazy local Kokoro. Needs `pip install kokoro-onnx` and the two model files in model_dir."""

    MODEL = "kokoro-v1.0.int8.onnx"
    VOICES = "voices-v1.0.bin"

    def __init__(self, model_dir: Path, voice: str, speed: float) -> None:
        self.model_dir, self.voice, self.speed = model_dir, voice, speed
        self._k = None
        self._lock = threading.Lock()

    def _load(self):
        if self._k is None:
            try:
                from kokoro_onnx import Kokoro
            except ImportError as error:
                raise RuntimeError("kokoro_not_installed: pip install kokoro-onnx") from error
            model, voices = self.model_dir / self.MODEL, self.model_dir / self.VOICES
            if not model.exists() or not voices.exists():
                raise RuntimeError(f"kokoro_models_missing: put {self.MODEL} and {self.VOICES} in {self.model_dir} "
                                   "(github.com/thewh1teagle/kokoro-onnx/releases model-files-v1.0)")
            self._k = Kokoro(str(model), str(voices))
        return self._k

    def synth(self, text: str, path: Path) -> None:
        lang = "en-gb" if self.voice.startswith("b") else "en-us"
        with self._lock:  # one inference at a time: CPU-bound, and keeps part order roughly FIFO
            samples, rate = self._load().create(text, voice=self.voice, speed=self.speed, lang=lang)
        write_device_wav(samples, rate, path)
