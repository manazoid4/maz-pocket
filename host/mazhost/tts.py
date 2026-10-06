"""Small laptop-side speech output for Call PC.

The Cardputer asks for a WAV; Windows does the synthesis. This keeps voices,
models and credentials off the ESP32 and gives v0.3 spoken replies without
adding another cloud dependency.
"""

from __future__ import annotations

import logging
import tempfile
from pathlib import Path

import httpx
import pyttsx3

from .config import Settings


log = logging.getLogger("uvicorn.error")


class SpeechOut:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self.last_provider = ""

    def available(self) -> bool:
        return self.settings.tts_enabled

    def _fish(self, text: str, path: Path) -> None:
        key = self.settings.fish_api_key
        if not key:
            raise RuntimeError("no_fish_key")
        model = self.settings.tts_model
        resp = httpx.post(
            "https://api.fish.audio/v1/tts",
            headers={"Authorization": f"Bearer {key}", "model": model},
            json={
                "text": text,
                "reference_id": self.settings.tts_voice,
                "format": "wav",
                "sample_rate": 16000,
            },
            timeout=30,
        )
        if resp.status_code != 200 or len(resp.content) <= 44:
            raise RuntimeError(f"fish_http_{resp.status_code}:{resp.text[:120]}")
        path.write_bytes(resp.content)

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
        reason = ""
        for _ in range(2):  # Fish first, retried once
            try:
                self._fish(clean, path)
                self.last_provider = "fish"
                log.info("tts_provider=fish voice=%s model=%s bytes=%d",
                         self.settings.tts_voice[:8] + "...", self.settings.tts_model, path.stat().st_size)
                return path
            except (RuntimeError, httpx.HTTPError) as error:
                reason = str(error)
        log.warning("TTS FALLBACK reason=%s", reason)
        self.last_provider = "windows"
        try:
            engine = pyttsx3.init()
            engine.setProperty("rate", self.settings.tts_rate)
            engine.save_to_file(clean, str(path))
            engine.runAndWait()
            engine.stop()
            if not path.exists() or path.stat().st_size <= 44:
                raise RuntimeError("tts_no_audio")
            log.info("tts_provider=windows")
            return path
        except Exception:
            path.unlink(missing_ok=True)
            raise
