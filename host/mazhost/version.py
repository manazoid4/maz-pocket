from __future__ import annotations

from pathlib import Path


def _read_version() -> str:
    candidates = (
        Path(__file__).resolve().parents[1] / "VERSION",
        Path(__file__).resolve().parents[2] / "VERSION",
    )
    for path in candidates:
        try:
            value = path.read_text(encoding="utf-8").strip()
        except OSError:
            continue
        if value:
            return value
    return "dev"


CORE_VERSION = _read_version()
