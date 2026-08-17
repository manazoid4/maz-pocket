from __future__ import annotations

from pathlib import Path


SOURCE = Path(__file__).resolve().parent.parent / "src" / "core" / "launcher.cpp"
text = SOURCE.read_text(encoding="utf-8")

required = [
    "esp_ota_set_boot_partition",
    "handBackTarget",
    "ESP.restart()",
]
for marker in required:
    if marker not in text:
        raise SystemExit(f"launcher hand-back guard: missing {marker}")

for forbidden in (
    "esp_rom_spiflash_write",
    "spi_flash_write",
    "running->address, &invalid",
):
    if forbidden in text:
        raise SystemExit(
            "launcher hand-back guard: MAZ Pocket must never invalidate its own app "
            f"image ({forbidden})"
        )

print("M5Launcher hand-back preserves the MAZ Pocket app image")
