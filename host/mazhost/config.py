"""MAZ Core configuration. Secrets and model routing stay on the laptop."""

from __future__ import annotations

import ipaddress
from pathlib import Path
from typing import Literal
from urllib.parse import urlsplit

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
    local_engine: Literal["ollama", "llamacpp"] = "ollama"
    llamacpp_url: str = "http://127.0.0.1:8080"
    llamacpp_model: str = "local"
    llamacpp_backup_url: str = ""
    llamacpp_backup_model: str = ""
    ollama_url: str = "http://127.0.0.1:11434"
    # LOCAL_FAST: the reliability-critical default. Small enough to stay fully
    # resident on a 6 GB card and answer in single-digit seconds once warm.
    ollama_model: str = "qwen3.5:4b"
    # LOCAL_SMART: optional, larger/slower. Never the AUTO reliability default.
    ollama_backup_model: str = "lfm2.5-8b-a1b-gpu:latest"
    local_model_policy: Literal["auto", "primary", "backup"] = "auto"
    ai_profile: Literal["smart", "save", "fast"] = "smart"

    cloud_url: str = "https://openrouter.ai/api/v1"
    cloud_key: str = ""
    cloud_model: str = "anthropic/claude-3.5-haiku"
    # Explicit 9router route. Keeping this loopback-only ensures the Cardputer
    # never receives gateway credentials or contacts the model router itself.
    mazlatest_url: str = Field(default="http://localhost:20128/v1", max_length=200)
    mazlatest_key: str = ""
    mazlatest_model: str = Field(default="MazLatest", min_length=1, max_length=120)
    default_route: Literal["local", "auto", "cloud", "mazlatest"] = "local"
    build_id: str = Field(default="dev", min_length=1, max_length=120)

    max_upload_mb: int = Field(default=12, ge=1, le=64)
    max_audio_seconds: int = Field(default=900, ge=1, le=3600)
    session_ttl_minutes: int = Field(default=120, ge=1, le=1440)
    max_turns: int = Field(default=12, ge=1, le=100)
    tts_enabled: bool = False
    tts_rate: int = Field(default=180, ge=80, le=300)
    fish_api_key: str = ""
    tts_voice: str = "bf322df2096a46f18c579d0baa36f41d"  # Fish "Adrian"
    tts_model: str = "s2.1-pro-free"
    nudge_url: str = "http://127.0.0.1:47831"
    nudge_token: str = ""
    nudge_token_file: str = "~/.agent-nudge/control-plane.key"
    nudge_cross_sync_days: int = Field(default=3, ge=1, le=30)
    device_port: str = ""
    device_baud: int = Field(default=115200, ge=1200, le=3_000_000)
    device_vid: int = 0x303A
    device_pid: int = 0x1001

    # MAZ Core: factual PC/project context and safe default actions.
    core_enabled: bool = True
    project_roots: str = ""
    obsidian_root: str = ""
    cardputer_url: str = "http://mazpocket.local"
    web_origins: str = "https://mazos-site.vercel.app,http://localhost:3000,http://127.0.0.1:3000"

    # Optional GitHub command bridge. This remains narrow and should not be used
    # as the authority channel for arbitrary PC execution.
    bridge_enabled: bool = False
    bridge_repo: str = "manazoid4/maz-pocket"
    github_token: str = ""
    bridge_poll_seconds: int = Field(default=15, ge=5, le=300)

    # Phone-approved authority broker. The normal MAZ bearer token may REQUEST
    # power, but approval is only available through the separately signed phone
    # control session. Grants are short-lived and independently revocable.
    control_enabled: bool = True
    control_dir: str = "~/.maz-pocket/control"
    control_phone_session_seconds: int = Field(default=43_200, ge=300, le=604_800)
    control_request_ttl_seconds: int = Field(default=900, ge=60, le=3600)
    control_default_grant_seconds: int = Field(default=600, ge=30, le=3600)
    control_max_grant_seconds: int = Field(default=3600, ge=60, le=14_400)
    control_command_timeout_seconds: int = Field(default=300, ge=5, le=3600)
    control_max_output_chars: int = Field(default=40_000, ge=2_000, le=500_000)

    # WORK Consistency: JOB HUNT / MAZ WORKS / custom tracks. Same authenticated
    # phone-control session boundary as the rest of /control; own SQLite store.
    work_dir: str = "~/.maz-pocket/work"

    # Claude/Codex/Hermes jobs are subprocesses on the PC. Their own CLI
    # permission bypasses are only used after the external MAZ phone broker has
    # approved PROJECT FULL or broader authority.
    agent_job_timeout_seconds: int = Field(default=1800, ge=60, le=7200)

    # Debug Capsules are intentionally bounded snapshots rather than whole-PC
    # dumps. Raw secrets are filtered before anything is presented to a model.
    debug_dir: str = "~/.maz-pocket/debug"
    debug_capsule_limit: int = Field(default=40, ge=5, le=500)
    debug_log_lines: int = Field(default=120, ge=20, le=1000)

    # Teach-by-Demonstration host-side resource defaults. Recording stays light;
    # heavier model work happens after STOP. 3 GB VRAM is the compatibility aim.
    resource_profile: Literal["auto", "eco", "balanced", "quality"] = "auto"
    vram_budget_mb: int = Field(default=3000, ge=512, le=48_000)
    teach_dir: str = "~/.maz-pocket/teach"
    teach_capture_mode: Literal["primary", "display", "window", "region"] = "primary"
    teach_display: str = "primary"
    teach_fps: int = Field(default=15, ge=2, le=30)
    teach_audio_device: str = ""
    teach_voice_profile: Literal["eco", "balanced", "quality"] = "balanced"
    teach_voice_cleanup: Literal["off", "light", "smart"] = "light"
    teach_sound_ai: bool = False
    teach_keep_video: bool = True
    skill_vault_dir: str = "~/.maz-pocket/skills"
    skill_draft_dir: str = "~/.maz-pocket/skills/drafts"

    @model_validator(mode="after")
    def migrate_legacy_shipped_defaults(self):
        if self.ollama_model == "gemma3:1b":
            self.ollama_model = "lfm2.5-8b-a1b-gpu:latest"
        parsed = None
        try:
            parsed = urlsplit(self.mazlatest_url)
            host = parsed.hostname or ""
            loopback = host.lower() == "localhost" or ipaddress.ip_address(host).is_loopback
        except ValueError:
            loopback = False
        if (
            parsed is None
            or parsed.scheme != "http"
            or not loopback
            or parsed.username is not None
            or parsed.password is not None
            or parsed.query
            or parsed.fragment
        ):
            raise ValueError("mazlatest_url must be a loopback HTTP URL without embedded credentials")
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
