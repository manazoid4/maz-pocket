from __future__ import annotations

import json
import re
import secrets
import time
from pathlib import Path
from typing import Any, Callable

from .config import Settings


_SECRET_PATTERNS = [
    re.compile(r"(?i)(bearer\s+)[A-Za-z0-9._~+/=-]{12,}"),
    re.compile(r"(?i)\b(sk|ghp|github_pat|xox[baprs])-[_A-Za-z0-9-]{10,}"),
    re.compile(r"(?i)(api[_-]?key|token|password|secret)\s*[:=]\s*([^\s,;]+)"),
    re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----.*?-----END [A-Z ]*PRIVATE KEY-----", re.S),
]


def redact_text(text: str) -> str:
    value = text
    for pattern in _SECRET_PATTERNS:
        if pattern.groups >= 2:
            value = pattern.sub(lambda m: m.group(1) + "=[REDACTED]", value)
        elif pattern.groups == 1:
            value = pattern.sub(lambda m: m.group(1) + "[REDACTED]", value)
        else:
            value = pattern.sub("[REDACTED]", value)
    return value


def redact(value: Any) -> Any:
    if isinstance(value, str):
        return redact_text(value)
    if isinstance(value, dict):
        out: dict[str, Any] = {}
        for key, item in value.items():
            if any(word in str(key).lower() for word in ("token", "password", "secret", "api_key", "cookie")):
                out[str(key)] = "[REDACTED]"
            else:
                out[str(key)] = redact(item)
        return out
    if isinstance(value, list):
        return [redact(x) for x in value]
    return value


class DebugCapsules:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self.root = Path(settings.debug_dir).expanduser()
        self.root.mkdir(parents=True, exist_ok=True)

    def collect(
        self,
        *,
        project: str,
        core_status: Callable[[], Any],
        project_status: Callable[[str], Any],
        jobs_status: Callable[[], Any],
        system_status: Callable[[], Any],
        model_status: Callable[[], Any],
        nudge_status: Callable[[], Any],
        device_status: Callable[[], Any],
        authority_status: Callable[[], Any],
    ) -> dict[str, Any]:
        def attempt(name: str, fn: Callable[[], Any]) -> Any:
            try:
                return fn()
            except Exception as error:  # bounded diagnostic path, never hides failure
                return {"ok": False, "error": f"{type(error).__name__}: {error}"}

        capsule_id = "dbg_" + secrets.token_urlsafe(10)
        data: dict[str, Any] = {
            "capsule_id": capsule_id,
            "created_at": time.time(),
            "project": project,
            "core": attempt("core", core_status),
            "project_state": attempt("project", lambda: project_status(project)) if project else {},
            "recent_jobs": attempt("jobs", jobs_status),
            "system": attempt("system", system_status),
            "models": attempt("models", model_status),
            "nudge": attempt("nudge", nudge_status),
            "device": attempt("device", device_status),
            "authority": attempt("authority", authority_status),
        }
        clean = redact(data)
        path = self.root / f"{capsule_id}.json"
        path.write_text(json.dumps(clean, indent=2, default=str), encoding="utf-8")
        self._trim()
        return clean

    def get(self, capsule_id: str) -> dict[str, Any]:
        if not re.fullmatch(r"dbg_[A-Za-z0-9_-]{6,40}", capsule_id):
            raise ValueError("invalid_capsule_id")
        path = self.root / f"{capsule_id}.json"
        if not path.exists():
            raise FileNotFoundError(capsule_id)
        return json.loads(path.read_text(encoding="utf-8"))

    def recent(self) -> list[dict[str, Any]]:
        result: list[dict[str, Any]] = []
        files = sorted(self.root.glob("dbg_*.json"), key=lambda p: p.stat().st_mtime, reverse=True)
        for path in files[: self.settings.debug_capsule_limit]:
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
                result.append({
                    "capsule_id": data.get("capsule_id"),
                    "created_at": data.get("created_at"),
                    "project": data.get("project", ""),
                })
            except (OSError, json.JSONDecodeError):
                continue
        return result

    def _trim(self) -> None:
        files = sorted(self.root.glob("dbg_*.json"), key=lambda p: p.stat().st_mtime, reverse=True)
        for path in files[self.settings.debug_capsule_limit :]:
            try:
                path.unlink()
            except OSError:
                pass
