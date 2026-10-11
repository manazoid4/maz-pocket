"""Exercise recovery preflight without contacting or writing any device."""
import os
from pathlib import Path
import shutil
import subprocess

import pytest

pytestmark = pytest.mark.skipif(os.name != "nt", reason="Windows USB recovery script")
SCRIPT = Path(os.environ.get("NOD_FLASH_TEST_SCRIPT", Path(__file__).parents[2] / "scripts/flash-usb.ps1"))


@pytest.fixture
def recovery(tmp_path):
    root = tmp_path / "repo"
    script = root / "scripts/flash-usb.ps1"
    script.parent.mkdir(parents=True)
    shutil.copyfile(SCRIPT, script)
    home = tmp_path / "profile"
    tool = home / ".platformio/packages/tool-esptoolpy/esptool.py"
    tool.parent.mkdir(parents=True)
    tool.write_text("import sys\nfrom pathlib import Path\nPath(sys.argv[sys.argv.index('-o')+1]).write_bytes(bytes([0xE9])+bytes(0x20000))\n")
    boot = home / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
    boot.parent.mkdir(parents=True)
    boot.write_bytes(b"boot")
    env = {**os.environ, "USERPROFILE": str(home)}

    def run(*args):
        return subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script), "-DryRun", "-Port", "COM999", *args], env=env, capture_output=True, text=True, timeout=20)
    return root, run


def factory():
    image = bytearray(0x20000)
    image[0], image[12] = 0xE9, 9
    image[0x8000:0x8004] = bytes([0xAA, 0x50, 0, 0])
    image[0x8004:0x8008] = (0x10000).to_bytes(4, "little")
    image[0x10000], image[0x1000C] = 0xE9, 9
    return image


def test_rejects_app_only_image_before_device_contact(recovery):
    root, run = recovery
    app = root / "firmware.bin"
    app.write_bytes(bytes([0xE9]) + bytes(0x20000))
    result = run("-Image", str(app))
    assert result.returncode == 1
    assert "factory image" in result.stdout


def test_accepts_factory_layout_in_dry_run(recovery):
    root, run = recovery
    image = root / "factory.bin"
    image.write_bytes(factory())
    result = run("-Image", str(image))
    assert result.returncode == 0, result.stdout + result.stderr
    assert "DRY RUN" in result.stdout


def test_rejects_partition_table_without_app(recovery):
    root, run = recovery
    image = factory()
    image[0x8002] = 1
    path = root / "wrong-partition.bin"
    path.write_bytes(image)
    result = run("-Image", str(path))
    assert result.returncode == 1
    assert "no app at 0x10000" in result.stdout


def test_dry_run_does_not_generate_factory_image(recovery):
    root, run = recovery
    build = root / ".pio/build/cardputer-adv"
    build.mkdir(parents=True)
    for name in ("bootloader.bin", "partitions.bin", "firmware.bin"):
        (build / name).write_bytes(b"fixture")
    result = run()
    assert result.returncode == 0, result.stdout + result.stderr
    assert not (root / "dist").exists()
