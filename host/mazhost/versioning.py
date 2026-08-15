from __future__ import annotations

from pathlib import Path


def _root() -> Path:
    return Path(__file__).resolve().parent.parent


def core_version() -> str:
    path = _root() / "CORE_VERSION"
    try:
        value = path.read_text(encoding="utf-8").strip()
    except OSError:
        return "dev"
    return value or "dev"


def build_id() -> str:
    path = _root() / "BUILD-ID"
    try:
        value = path.read_text(encoding="utf-8").strip()
    except OSError:
        return "unknown"
    return value or "unknown"
