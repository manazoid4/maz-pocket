"""Small, deterministic WAV tuning profiles for nod output hardware.

Runs on nod Core before audio reaches the handheld. No DSP on the ESP32, no
cloud dependency, no credentials and no modification of user recordings.
Prototype profiles are starting points, not hardware-calibrated production EQ.
"""

from __future__ import annotations

import math
import sys
import wave
from array import array
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class OutputProfile:
    highpass_hz: float
    lowpass_hz: float
    gain_db: float
    max_peak: float
    fade_ms: float = 3.0


PROFILES: dict[str, OutputProfile | None] = {
    # Flat is a true bypass for A/B tests and unexpected hardware.
    "flat": None,
    # ADV: ES8311 codec, NS4150B amplifier and 1 W built-in speaker.
    "cardputer_adv": OutputProfile(165, 5700, -1.0, 0.86),
    # Prototype presets are NOT calibrated. Measure the real transducer and
    # enclosure before using either in a Kickstarter production image.
    "prototype_small": OutputProfile(190, 5500, -1.5, 0.82),
    "prototype_large": OutputProfile(100, 6800, -0.5, 0.90),
    "headphones": OutputProfile(35, 7300, -1.0, 0.88),
}


def shape_speech_wav(path: Path, profile_name: str) -> bool:
    """Process PCM16 mono WAV at its native rate, atomically, or leave unchanged.

    Fixed-gain filtering with a peak ceiling preserves speech dynamics;
    no aggressive AGC or non-linear saturation is applied.
    """
    profile = PROFILES[profile_name]
    if profile is None:
        return False
    try:
        with wave.open(str(path), "rb") as reader:
            if reader.getnchannels() != 1 or reader.getsampwidth() != 2 or reader.getcomptype() != "NONE":
                return False
            rate, frames = reader.getframerate(), reader.getnframes()
            if rate < 8000 or rate > 96000 or frames == 0 or frames > 2_000_000:
                return False
            pcm = reader.readframes(frames)
    except (OSError, EOFError, ValueError, wave.Error):
        return False
    if len(pcm) != 2 * frames:
        return False

    samples = array("h")
    samples.frombytes(pcm)
    if sys.byteorder != "little":
        samples.byteswap()

    dt = 1.0 / rate
    hp_rc = 1.0 / (2 * math.pi * profile.highpass_hz)
    lp_rc = 1.0 / (2 * math.pi * min(profile.lowpass_hz, rate * 0.45))
    hp_a = hp_rc / (hp_rc + dt)
    lp_a = dt / (lp_rc + dt)
    prev_in = prev_hp = prev_lp = 0.0
    gain = 10 ** (profile.gain_db / 20.0)
    processed = array("f")
    peak = 0.0
    for sample in samples:
        x = float(sample) / 32768.0
        hp = hp_a * (prev_hp + x - prev_in)
        lp = prev_lp + lp_a * (hp - prev_lp)
        prev_in, prev_hp, prev_lp = x, hp, lp
        value = lp * gain
        processed.append(value)
        peak = max(peak, abs(value))

    scale = min(1.0, profile.max_peak / peak) if peak > 0 else 1.0
    fade = min(frames // 2, round(rate * profile.fade_ms / 1000.0))
    output = array("h")
    for i, value in enumerate(processed):
        envelope = min(1.0, (i + 1) / fade, (frames - i) / fade) if fade else 1.0
        output.append(max(-32768, min(32767, round(value * scale * envelope * 32767))))
    if sys.byteorder != "little":
        output.byteswap()

    temp = path.with_suffix(".profile.tmp")
    try:
        with wave.open(str(temp), "wb") as writer:
            writer.setnchannels(1)
            writer.setsampwidth(2)
            writer.setframerate(rate)
            writer.writeframes(output.tobytes())
        temp.replace(path)
        return True
    finally:
        temp.unlink(missing_ok=True)
