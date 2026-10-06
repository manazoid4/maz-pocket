"""Firmware the Core currently holds for over-the-air updates (<root>/fw by default)."""
from __future__ import annotations

import json
import os
from pathlib import Path


def fw_dir() -> Path:
    return Path(os.environ.get("MAZ_FW_DIR") or Path(__file__).resolve().parents[1] / "fw")


def manifest() -> dict | None:
    """{version, sha, sha256, size} written by scripts/fw_manifest.py; None when no firmware is staged."""
    try:
        data = json.loads((fw_dir() / "manifest.json").read_text(encoding="utf-8"))
        return data if data.get("version") and (fw_dir() / "latest.bin").is_file() else None
    except (OSError, ValueError):
        return None
