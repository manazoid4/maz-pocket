"""MAZ Host configuration. Secrets and model routing stay on the laptop."""

from __future__ import annotations

from typing import Literal

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_prefix="MAZ_", env_file=".env", env_file_encoding="utf-8", extra="ignore"
    )

    token: str = "change-me-before-first-run"
    bind: str = "0.0.0.0"
    port: int = 8787
    whisper_model: str = "base.en"
    whisper_device: str = "cpu"
    whisper_compute: str = "int8"

    # v0.4 local-first brain. This exact Ollama tag is already present on the
    # target workstation; keeping one canonical tag avoids silent fallback to
    # tiny generic models that produced weak/hallucinated app answers.
    ollama_url: str = "http://127.0.0.1:11434"
    ollama_model: str = "lfm2.5-8b-a1b-gpu:latest"

    cloud_url: str = "https://openrouter.ai/api/v1"
    cloud_key: str = ""
    cloud_model: str = "anthropic/claude-3.5-haiku"
    default_route: Literal["local", "auto", "cloud"] = "local"

    max_upload_mb: int = Field(default=12, ge=1, le=64)
    max_audio_seconds: int = Field(default=900, ge=1, le=3600)
    session_ttl_minutes: int = Field(default=120, ge=1, le=1440)
    max_turns: int = Field(default=24, ge=1, le=100)
    tts_enabled: bool = False
    tts_rate: int = Field(default=180, ge=80, le=300)
    nudge_url: str = "http://127.0.0.1:47831"
    nudge_token: str = ""
    nudge_token_file: str = "~/.agent-nudge/control-plane.key"
    nudge_cross_sync_days: int = Field(default=3, ge=1, le=30)
    device_port: str = ""
    device_baud: int = Field(default=115200, ge=1200, le=3_000_000)
    device_vid: int = 0x303A
    device_pid: int = 0x1001

    @property
    def token_configured(self) -> bool:
        return bool(self.token and self.token != "change-me-before-first-run")
