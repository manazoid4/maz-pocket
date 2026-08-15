"""MAZ Core configuration. Secrets and local inference stay on the laptop."""

from __future__ import annotations

from pathlib import Path
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

    # Backend-neutral OpenAI-compatible local inference contract.
    # v0.6.4 uses llama-swap as the stable local front door. llama.cpp is the
    # Windows runtime; a future Linux host can swap in vLLM without firmware changes.
    local_engine: Literal["llama_swap", "llamacpp", "vllm", "openai_compat"] = "llama_swap"
    local_runtime: Literal["llamacpp", "vllm", "other"] = "llamacpp"
    local_api_url: str = "http://127.0.0.1:8790/v1"
    local_health_url: str = "http://127.0.0.1:8790/health"
    local_model: str = "maz-primary"
    local_backup_model: str = "maz-backup"
    local_context: int = Field(default=8192, ge=1024, le=32768)
    local_max_tokens: int = Field(default=512, ge=64, le=4096)
    local_temperature: float = Field(default=0.15, ge=0.0, le=2.0)

    # llama.cpp supervisor settings. The HTTP server is loopback-only; MAZ Core
    # is the authenticated LAN-facing gateway used by the Cardputer.
    llamacpp_model_path: str = ""
    llamacpp_threads: int = Field(default=6, ge=1, le=64)
    llamacpp_parallel: int = Field(default=1, ge=1, le=4)
    llamacpp_ttl_seconds: int = Field(default=300, ge=30, le=3600)

    # Deprecated Ollama compatibility fields kept temporarily so older status/
    # config consumers do not crash during the v0.6.x migration. Inference no
    # longer uses these when local_engine=llama_swap.
    ollama_url: str = "http://127.0.0.1:11434"
    ollama_model: str = "qwen3.5:4b"
    ollama_backup_model: str = ""
    local_model_policy: Literal["auto", "primary", "backup"] = "auto"

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

    core_enabled: bool = True
    project_roots: str = ""
    obsidian_root: str = ""
    cardputer_url: str = "http://mazpocket.local"
    web_origins: str = "https://mazos-site.vercel.app,http://localhost:3000,http://127.0.0.1:3000"

    bridge_enabled: bool = False
    bridge_repo: str = "manazoid4/maz-pocket"
    github_token: str = ""
    bridge_poll_seconds: int = Field(default=15, ge=5, le=300)

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
