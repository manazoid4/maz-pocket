from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
if not re.fullmatch(r"\d+\.\d+(?:\.\d+)?", version):
    raise SystemExit(f"invalid VERSION: {version!r}")

checks = {
    "README.md": f"Current release candidate: v{version}",
    "QUICKSTART.txt": f"MAZ POCKET v{version}",
    "RELEASE_NOTES.md": f"v{version}",
    "CHANGELOG.md": f"## v{version}",
    "docs/RELEASE_RULES.md": "MAZ-Cardputer-v<version>.zip",
}
for name, needle in checks.items():
    text = (ROOT / name).read_text(encoding="utf-8")
    if needle not in text:
        raise SystemExit(f"{name} does not contain required contract text: {needle}")

pio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
if "extra_scripts = pre:scripts/version.py" not in pio:
    raise SystemExit("platformio.ini is not sourcing VERSION through scripts/version.py")
if "MAZ_POCKET_VERSION=" in pio:
    raise SystemExit("platformio.ini still hard-codes MAZ_POCKET_VERSION")
if "-<net/portal.cpp>" not in pio:
    raise SystemExit("active portal_v2 guard missing: legacy portal.cpp is not excluded")

firmware_workflow = (ROOT / ".github" / "workflows" / "firmware.yml").read_text(encoding="utf-8")
required_packages = (
    "MAZ-Core-v${{ steps.version.outputs.value }}.zip",
    "MAZ-Cardputer-v${{ steps.version.outputs.value }}.zip",
    "MAZ-Pocket-v${{ steps.version.outputs.value }}-Install.zip",
)
for package in required_packages:
    if package not in firmware_workflow:
        raise SystemExit(f"firmware workflow does not enforce package: {package}")
for stale in ("v0.5.1", "MAZ-Pocket-v0.5.1", "MAZ-Core-v0.5.1"):
    if stale in firmware_workflow:
        raise SystemExit(f"firmware workflow contains stale version token {stale}")

prepare = (ROOT / "scripts" / "prepare-release.ps1").read_text(encoding="utf-8")
if '"MAZ-Cardputer-v$Version.zip"' not in prepare:
    raise SystemExit("prepare-release.ps1 must emit a standalone Cardputer ZIP")
if '"MAZ-Core-v$Version.zip"' not in prepare:
    raise SystemExit("prepare-release.ps1 must emit a standalone client/Core ZIP")

sd = (ROOT / "scripts" / "install-to-sd.ps1").read_text(encoding="utf-8")
if '$Version = "' in sd:
    raise SystemExit("install-to-sd.ps1 must read VERSION instead of hard-coding it")

control = (ROOT / "src" / "net" / "control.cpp").read_text(encoding="utf-8")
if "MAZCOREPAIR\\t" not in control:
    raise SystemExit("Core-only pairing command is missing")

portal = (ROOT / "src" / "net" / "portal_v2.cpp").read_text(encoding="utf-8")
if "Update.h" in portal or "esp_ota" in portal.lower():
    raise SystemExit("portal must stage firmware only; direct self-OTA is not allowed")

field = (ROOT / "src" / "core" / "field.cpp").read_text(encoding="utf-8")
if "submitOutboxAudio" not in field or "submitOutboxBeam" not in field:
    raise SystemExit("v0.7 durable FIELD outbox contract missing")

print(f"MAZ Pocket v{version} version/install contract OK")
