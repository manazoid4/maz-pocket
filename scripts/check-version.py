from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
if not re.fullmatch(r"\d+\.\d+(?:\.\d+)?", version):
    raise SystemExit(f"invalid VERSION: {version!r}")

checks = {
    "README.md": f"Current release: v{version}",
    "QUICKSTART.txt": f"MAZ POCKET v{version}",
    "RELEASE_NOTES.md": f"v{version}",
}
for name, needle in checks.items():
    text = (ROOT / name).read_text(encoding="utf-8")
    if needle not in text:
        raise SystemExit(f"{name} does not identify v{version}")

pio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
if "extra_scripts = pre:scripts/version.py" not in pio:
    raise SystemExit("platformio.ini is not sourcing VERSION through scripts/version.py")
if "MAZ_POCKET_VERSION=" in pio:
    raise SystemExit("platformio.ini still hard-codes MAZ_POCKET_VERSION")
if "-<net/portal.cpp>" not in pio:
    raise SystemExit("active portal_v2 guard missing: legacy portal.cpp is not excluded")

firmware_workflow = (ROOT / ".github" / "workflows" / "firmware.yml").read_text(encoding="utf-8")
for stale in ("v0.5.1", "MAZ-Pocket-v0.5.1", "MAZ-Core-v0.5.1"):
    if stale in firmware_workflow:
        raise SystemExit(f"firmware workflow contains stale version token {stale}")

sd = (ROOT / "scripts" / "install-to-sd.ps1").read_text(encoding="utf-8")
if '$Version = "' in sd:
    raise SystemExit("install-to-sd.ps1 must read VERSION instead of hard-coding it")

control = (ROOT / "src" / "net" / "control.cpp").read_text(encoding="utf-8")
if "MAZCOREPAIR\\t" not in control:
    raise SystemExit("v0.6 Core-only pairing command is missing")

portal = (ROOT / "src" / "net" / "portal_v2.cpp").read_text(encoding="utf-8")
if "Update.h" in portal or "esp_ota" in portal.lower():
    raise SystemExit("portal must stage firmware only; direct self-OTA is not allowed")

print(f"MAZ Pocket v{version} version/install contract OK")
