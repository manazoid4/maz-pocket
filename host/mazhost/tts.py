"""Small laptop-side speech output for Call PC.

The Cardputer asks for a WAV; Windows does the synthesis. This keeps voices,
models and credentials off the ESP32 and gives v0.3 spoken replies without
adding another cloud dependency.
"""

from __future__ import annotations

import hashlib
import logging
import shutil
import tempfile
from pathlib import Path

import httpx
import pyttsx3

from .config import Settings
from .voices import ADRIAN_ID, VoiceService


log = logging.getLogger("uvicorn.error")

# Spoken often and identical every time: synthesised once per voice, then served
# from disk (no Fish call, so no ~1-4 s wait). Any reply of <= CACHE_MAX_CHARS is
# cached too ("Done.", "Okay, noted.").
COMMON_PHRASES = frozenset(p.lower() for p in (
    "Sorry, my brain is offline, try again",
    "Sorry, my brain is offline",
    "Okay.", "Okay", "Done.", "Done", "Got it.", "Got it", "Sure.", "Sure", "Noted.",
    "On it.", "One moment.", "Sorry, I didn't catch that.", "Sorry, I did not catch that.",
    "I couldn't reach the PC.", "Something went wrong, try again.", "Hi, I'm nod.",
))
CACHE_MAX_CHARS = 40
CACHE_MAX_FILES = 300


def _norm(text: str) -> str:
    return " ".join(text.lower().split())


class SpeechOut:
    def __init__(self, settings: Settings, voices: VoiceService | None = None) -> None:
        self.settings = settings
        self.last_provider = ""
        self.voices = voices or VoiceService(settings)
        self.voices._synth = self._fish_voice
        self.cache_hits = 0

    def _cache_file(self, text: str, voice: str) -> Path | None:
        norm = _norm(text)
        if norm not in COMMON_PHRASES and len(norm) > CACHE_MAX_CHARS:
            return None
        digest = hashlib.sha1(norm.encode("utf-8")).hexdigest()[:20]
        return self.voices.dir / "tts_cache" / voice / f"{digest}.wav"

    def _cache_store(self, src: Path, cached: Path) -> None:
        try:
            cached.parent.mkdir(parents=True, exist_ok=True)
            tmp = cached.with_suffix(".tmp")
            shutil.copyfile(src, tmp)
            tmp.replace(cached)
            files = sorted(cached.parent.glob("*.wav"), key=lambda p: p.stat().st_mtime)
            for old in files[:-CACHE_MAX_FILES]:
                old.unlink(missing_ok=True)
        except OSError:
            log.warning("tts cache write failed", exc_info=True)

    def available(self) -> bool:
        return self.settings.tts_enabled

    def _fish_voice(self, text: str, path: Path, voice: str) -> None:
        """Single Fish call for an explicit voice (used by cached previews); no fallback."""
        self._fish(text, path, voice, fallback=False)

    def _fish(self, text: str, path: Path, voice: str | None = None, fallback: bool = True) -> None:
        key = self.settings.fish_api_key
        if not key:
            raise RuntimeError("no_fish_key")
        model = self.settings.tts_model
        voice = voice or self.voices.current()
        resp = httpx.post(
            "https://api.fish.audio/v1/tts",
            headers={"Authorization": f"Bearer {key}", "model": model},
            json={
                "text": text,
                "reference_id": voice,
                "format": "wav",
                "sample_rate": 16000,
            },
            timeout=30,
        )
        if fallback and resp.status_code in (400, 404, 422) and voice != ADRIAN_ID:
            self.voices.reject(voice)
            return self._fish(text, path, ADRIAN_ID, fallback=False)
        if resp.status_code != 200 or len(resp.content) <= 44:
            raise RuntimeError(f"fish_http_{resp.status_code}:{resp.text[:120]}")
        data = resp.content
        # Fish streams WAV with a bogus length header; rewrite sizes from real byte count.
        i = data.find(b"data")
        if data[:4] == b"RIFF" and i > 0:
            import struct
            data = (data[:4] + struct.pack("<I", len(data) - 8) + data[8:i + 4]
                    + struct.pack("<I", len(data) - i - 8) + data[i + 8:])
        path.write_bytes(data)

    def synthesize(self, text: str) -> Path:
        if not self.settings.tts_enabled:
            raise RuntimeError("tts_disabled")
        clean = " ".join(text.strip().split())
        if not clean:
            raise RuntimeError("empty_tts_text")
        clean = clean[:1400]

        voice = self.voices.current()
        cached = self._cache_file(clean, voice)
        if cached is not None and cached.exists() and cached.stat().st_size > 44:
            handle = tempfile.NamedTemporaryFile(delete=False, suffix=".wav")
            handle.close()
            shutil.copyfile(cached, handle.name)
            self.cache_hits += 1
            self.last_provider = "cache"
            log.info("tts_provider=cache bytes=%d", cached.stat().st_size)
            return Path(handle.name)

        handle = tempfile.NamedTemporaryFile(delete=False, suffix=".wav")
        path = Path(handle.name)
        handle.close()
        reason = ""
        for _ in range(2):  # Fish first, retried once
            try:
                self._fish(clean, path)
                self.last_provider = "fish"
                if cached is not None:
                    self._cache_store(path, cached)
                log.info("tts_provider=fish voice=%s model=%s bytes=%d",
                         self.voices.current()[:8] + "...", self.settings.tts_model, path.stat().st_size)
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
