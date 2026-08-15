"""Small laptop-side speech output for Call PC.

The Cardputer asks for a WAV; Windows does the synthesis. This keeps voices,
models and credentials off the ESP32 and gives v0.3 spoken replies without
adding another cloud dependency.
"""

from __future__ import annotations

import tempfile
from pathlib import Path

import pyttsx3

from .config import Settings


class SpeechOut:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings

    def available(self) -> bool:
        return self.settings.tts_enabled

    def synthesize(self, text: str) -> Path:
        if not self.settings.tts_enabled:
            raise RuntimeError("tts_disabled")
        clean = " ".join(text.strip().split())
        if not clean:
            raise RuntimeError("empty_tts_text")
        clean = clean[:1400]

        handle = tempfile.NamedTemporaryFile(delete=False, suffix=".wav")
        path = Path(handle.name)
        handle.close()
        try:
            engine = pyttsx3.init()
            engine.setProperty("rate", self.settings.tts_rate)
            engine.save_to_file(clean, str(path))
            engine.runAndWait()
            engine.stop()
            if not path.exists() or path.stat().st_size <= 44:
                raise RuntimeError("tts_no_audio")
            return path
        except Exception:
            path.unlink(missing_ok=True)
            raise
