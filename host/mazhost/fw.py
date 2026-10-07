"""Firmware the Core currently holds for over-the-air updates (<root>/fw by default)."""
from __future__ import annotations

import json
import logging
import os
from datetime import datetime, timezone
from pathlib import Path

log = logging.getLogger("mazhost.fw")


def fw_dir() -> Path:
    return Path(os.environ.get("MAZ_FW_DIR") or Path(__file__).resolve().parents[1] / "fw")


def manifest() -> dict | None:
    """{version, sha, sha256, size} written by scripts/fw_manifest.py; None when no firmware is staged."""
    try:
        data = json.loads((fw_dir() / "manifest.json").read_text(encoding="utf-8"))
        return data if data.get("version") and (fw_dir() / "latest.bin").is_file() else None
    except (OSError, ValueError):
        return None


def report_path() -> Path:
    return fw_dir() / "last_report.json"


def save_report(report: dict) -> dict:
    """Keep the device's last OTA report (success or failure) readable at GET /core/update."""
    report = {**report, "at": datetime.now(timezone.utc).isoformat(timespec="seconds")}
    try:
        fw_dir().mkdir(parents=True, exist_ok=True)
        report_path().write_text(json.dumps(report), encoding="utf-8")
    except OSError:
        pass  # logged below either way; the file is a convenience
    log.warning("fw report: stage=%s error=%r code=%s slot=%s next=%s size=%s build=%s state=%s otadata=%s battery=%s",
                report.get("stage"), report.get("error"), report.get("code"), report.get("slot"),
                report.get("next_slot"), report.get("size"), report.get("build"), report.get("ota_state"),
                report.get("otadata"), report.get("battery"))
    return report


def last_report() -> dict | None:
    try:
        return json.loads(report_path().read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
