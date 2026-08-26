from __future__ import annotations

import importlib.util
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[2] / "scripts" / "launcher-device.py"
SPEC = importlib.util.spec_from_file_location("launcher_device", SCRIPT)
assert SPEC and SPEC.loader
launcher_device = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(launcher_device)


def test_handoff_accepts_explicit_ack_or_launcher_banner():
    assert launcher_device.handoff_complete(["MAZLAUNCHER OK"])
    assert launcher_device.handoff_complete(
        ["Press the button to enter the Launcher!"]
    )


def test_handoff_accepts_launcher_fastboot_back_to_healthy_maz():
    assert launcher_device.handoff_complete(
        [
            "ESP-ROM:esp32s3-20210327",
            "MAZ Pocket 1.0.0 READY board=24 keyboard=ok storage=internal",
        ]
    )


def test_handoff_does_not_accept_ready_without_a_reset():
    assert not launcher_device.handoff_complete(
        ["MAZ Pocket 1.0.0 READY board=24 keyboard=ok storage=internal"]
    )
