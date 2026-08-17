from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
version = (ROOT / "VERSION").read_text(encoding="utf-8-sig").strip()
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
    text = (ROOT / name).read_text(encoding="utf-8-sig")
    if needle not in text:
        raise SystemExit(f"{name} does not contain required contract text: {needle}")

pio = (ROOT / "platformio.ini").read_text(encoding="utf-8-sig")
if "extra_scripts = pre:scripts/version.py" not in pio:
    raise SystemExit("platformio.ini is not sourcing VERSION through scripts/version.py")
if "MAZ_POCKET_VERSION=" in pio:
    raise SystemExit("platformio.ini still hard-codes MAZ_POCKET_VERSION")
if "-<net/portal.cpp>" not in pio or "-<net/portal_v2.cpp>" not in pio:
    raise SystemExit("portal_v3 guard missing: older portal implementations are not excluded")

firmware_workflow = (ROOT / ".github" / "workflows" / "firmware.yml").read_text(encoding="utf-8-sig")
required_packages = (
    "MAZ-Core-v${{ steps.version.outputs.value }}.zip",
    "MAZ-Cardputer-v${{ steps.version.outputs.value }}.zip",
    "MAZ-Pocket-v${{ steps.version.outputs.value }}-Install.zip",
)
for package in required_packages:
    if package not in firmware_workflow:
        raise SystemExit(f"firmware workflow does not enforce package: {package}")
if "check-launcher-handoff.py" not in firmware_workflow:
    raise SystemExit("firmware workflow does not guard non-destructive M5Launcher hand-back")

prepare = (ROOT / "scripts" / "prepare-release.ps1").read_text(encoding="utf-8-sig")
if '"MAZ-Cardputer-v$Version.zip"' not in prepare:
    raise SystemExit("prepare-release.ps1 must emit a standalone Cardputer ZIP")
if '"MAZ-Core-v$Version.zip"' not in prepare:
    raise SystemExit("prepare-release.ps1 must emit a standalone client/Core ZIP")

sd = (ROOT / "scripts" / "install-to-sd.ps1").read_text(encoding="utf-8-sig")
if '$Version = "' in sd:
    raise SystemExit("install-to-sd.ps1 must read VERSION instead of hard-coding it")

control = (ROOT / "src" / "net" / "control.cpp").read_text(encoding="utf-8-sig")
if "MAZCOREPAIR\\t" not in control:
    raise SystemExit("Core-only pairing command is missing")

portal = (ROOT / "src" / "net" / "portal_v3.cpp").read_text(encoding="utf-8-sig")
if "Update.h" in portal or "esp_ota" in portal.lower():
    raise SystemExit("portal must stage firmware only; direct self-OTA is not allowed")
if "PROMPT DECK" not in portal or "CALL MAZ" not in portal:
    raise SystemExit("v0.8 phone-first portal is missing clear product actions")

launcher = (ROOT / "src" / "core" / "launcher.cpp").read_text(encoding="utf-8-sig")
if "esp_ota_set_boot_partition" not in launcher:
    raise SystemExit("M5Launcher hand-back must select a boot partition")
if "esp_rom_spiflash_write" in launcher or "spi_flash_write" in launcher:
    raise SystemExit("M5Launcher hand-back must never erase/invalidate the running MAZ app")

field = (ROOT / "src" / "core" / "field.cpp").read_text(encoding="utf-8-sig")
if "submitOutboxAudio" not in field or "submitOutboxBeam" not in field:
    raise SystemExit("durable FIELD outbox contract missing")

print(f"MAZ Pocket v{version} version/install contract OK")
