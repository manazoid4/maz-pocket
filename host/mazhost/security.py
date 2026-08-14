from __future__ import annotations

import hmac
import wave
from pathlib import Path

from fastapi import Header, HTTPException, status

from .config import Settings


class Security:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings

    def require_configured(self) -> None:
        if not self.settings.token_configured:
            raise RuntimeError("Set MAZ_TOKEN to a non-default random value before starting MAZ Host")

    def authorize(self, authorization: str | None = Header(default=None)) -> None:
        self.require_configured()
        expected = f"Bearer {self.settings.token}"
        if not authorization or not hmac.compare_digest(authorization, expected):
            raise HTTPException(status.HTTP_401_UNAUTHORIZED, "authentication_required")

    def validate_upload(self, path: Path, size: int) -> float:
        if size > self.settings.max_upload_mb * 1024 * 1024:
            raise HTTPException(status.HTTP_413_REQUEST_ENTITY_TOO_LARGE, "audio_too_large")
        try:
            with wave.open(str(path), "rb") as wav:
                if wav.getnchannels() != 1 or wav.getsampwidth() != 2:
                    raise HTTPException(status.HTTP_400_BAD_REQUEST, "unsupported_wav_format")
                duration = wav.getnframes() / max(wav.getframerate(), 1)
        except (wave.Error, EOFError) as error:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "invalid_wav") from error
        if duration > self.settings.max_audio_seconds:
            raise HTTPException(status.HTTP_413_REQUEST_ENTITY_TOO_LARGE, "audio_too_long")
        return duration
