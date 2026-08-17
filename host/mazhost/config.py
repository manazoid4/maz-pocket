"""MAZ Core configuration. Secrets and model routing stay on the laptop."""

from __future__ import annotations

from pathlib import Path
from typing import Literal

from pydantic import Field, model_validator
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

    # Local-first brain. AUTO tries primary then backup locally before cloud.
    # LOCAL uses the same local chain but never spills to cloud.
    ollama_url: str = "http://127.0.0.1:11434"
    ollama_model: str = "lfm2.5-8b-a1b-gpu:latest"
    ollama_backup_model: str = "qwen3.5:4b"
    local_model_policy: Literal["auto", "primary", "backup"] = "auto"

    # One understandable performance knob instead of exposing a wall of Ollama
    # tuning. SMART is the default; FAST favors warm-model latency; SAVE favors
    # VRAM release between turns.
    ai_profile: Literal["smart", "save", "fast"] = "smart"

    cloud_url: str = "https://openrouter.ai/api/v1"
    cloud_key: str = ""
    cloud_model: str = "anthropic/claude-3.5-haiku"
    default_route: Literal["local", "auto", "cloud"] = "local"

    max_upload_mb: int = Field(default=12, ge=1, le=64)
    max_audio_seconds: int = Field(default=900, ge=1, le=3600)
    session_ttl_minutes: int = Field(default=120, ge=1, le=1440)
    # Pocket conversations are short by design. Keeping 12 complete turns gives
    # useful continuity without repeatedly re-evaluating a large stale history.
    max_turns: int = Field(default=12, ge=1, le=100)
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

    # MAZ Core: factual PC/project context and safe allow-listed actions.
    core_enabled: bool = True
    project_roots: str = ""
    obsidian_root: str = ""
    cardputer_url: str = "http://mazpocket.local"
    web_origins: str = "https://mazos-site.vercel.app,http://localhost:3000,http://127.0.0.1:3000"

    # Optional GitHub command bridge. When enabled, MAZ Core watches a private
    # repo for issues titled `[MAZ CORE] ...`, executes only its allow-list and
    # writes evidence back as a comment. It can reuse `gh auth token` locally.
    bridge_enabled: bool = False
    bridge_repo: str = "manazoid4/maz-pocket"
    github_token: str = ""
    bridge_poll_seconds: int = Field(default=15, ge=5, le=300)

    @model_validator(mode="after")
    def migrate_legacy_shipped_defaults(self):
        if self.ollama_model == "gemma3:1b":
            self.ollama_model = "lfm2.5-8b-a1b-gpu:latest"
        return self

    @property
    def token_configured(self) -> bool:
        return bool(self.token and self.token != "change-me-before-first-run")

    @property
    def project_root_paths(self) -> list[Path]:
        values = [p.strip() for p in self.project_roots.split(";") if p.strip()]
        if not values:
            home = Path.home()
            values = [str(home / "Desktop"), str(home / "Projects")]
        result: list[Path] = []
        for value in values:
            path = Path(value).expanduser()
            if path.exists() and path not in result:
                result.append(path)
        return result

    @property
    def web_origin_list(self) -> list[str]:
        return [x.strip() for x in self.web_origins.split(",") if x.strip()]
