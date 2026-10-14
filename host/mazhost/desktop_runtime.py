from __future__ import annotations

import os
import secrets
import socket
import subprocess
import sys
import time
from pathlib import Path

import httpx


APP_DIR = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local")) / "MAZ Core"
ENV_PATH = APP_DIR / ".env"


def read_env(path: Path = ENV_PATH) -> dict[str, str]:
    values: dict[str, str] = {}
    if path.exists():
        for raw in path.read_text(encoding="utf-8-sig").splitlines():
            if "=" in raw and not raw.lstrip().startswith("#"):
                key, value = raw.split("=", 1)
                values[key.strip()] = value.strip()
    return values


def ensure_config(path: Path = ENV_PATH) -> dict[str, str]:
    path.parent.mkdir(parents=True, exist_ok=True)
    values = read_env(path)
    if not values.get("MAZ_TOKEN") or values.get("MAZ_TOKEN") == "change-me-before-first-run":
        values["MAZ_TOKEN"] = secrets.token_urlsafe(32)
    values.setdefault("MAZ_BIND", "0.0.0.0")
    values.setdefault("MAZ_PORT", "8787")
    values.setdefault("MAZ_DEFAULT_ROUTE", "auto")
    values.setdefault("MAZ_CARDPUTER_URL", "http://mazpocket.local")
    ordered = [f"{key}={value}" for key, value in sorted(values.items())]
    path.write_text("\n".join(ordered) + "\n", encoding="utf-8")
    return values


def local_url(values: dict[str, str]) -> str:
    return f"http://127.0.0.1:{int(values.get('MAZ_PORT', '8787'))}"


def api_get(values: dict[str, str], route: str, timeout: float = 2.0) -> dict:
    response = httpx.get(
        local_url(values) + route,
        headers={"Authorization": f"Bearer {values['MAZ_TOKEN']}"},
        timeout=timeout,
    )
    response.raise_for_status()
    return response.json()


class CoreRuntime:
    def __init__(self, app_dir: Path = APP_DIR) -> None:
        self.app_dir = app_dir
        self.env_path = app_dir / ".env"
        self.process: subprocess.Popen | None = None

    def values(self) -> dict[str, str]:
        return ensure_config(self.env_path)

    def health(self) -> dict | None:
        try:
            return api_get(self.values(), "/health")
        except (OSError, httpx.HTTPError, ValueError):
            return None

    def _command(self) -> list[str]:
        if getattr(sys, "frozen", False):
            return [sys.executable, "--serve"]
        return [sys.executable, str(Path(__file__).resolve().parents[1] / "desktop_entry.py"), "--serve"]

    def start(self, deadline: float = 20.0) -> dict:
        current = self.health()
        if current is not None:
            return current
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        self.process = subprocess.Popen(
            self._command(), cwd=self.app_dir, creationflags=flags,
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        end = time.monotonic() + deadline
        while time.monotonic() < end:
            if self.process.poll() is not None:
                raise RuntimeError("MAZ Core stopped during startup")
            current = self.health()
            if current is not None:
                return current
            time.sleep(0.2)
        raise RuntimeError("MAZ Core did not become ready")

    def restart(self) -> dict:
        if self.process is None or self.process.poll() is not None:
            if self.health() is not None:
                raise RuntimeError("Core is running outside this window; close it before restarting")
        else:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=2)
        self.process = None
        return self.start()


def lan_address() -> str:
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("8.8.8.8", 80))
        return probe.getsockname()[0]
    except OSError:
        return socket.gethostbyname(socket.gethostname())
    finally:
        probe.close()
