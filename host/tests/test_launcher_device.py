from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = (ROOT / "scripts" / "launcher-device.py").read_text(encoding="utf-8")
INSTALLER = (ROOT / "scripts" / "install.ps1").read_text(encoding="utf-8")


def test_handoff_requires_launcher_banner_and_serial_navigation():
    assert "M5Launcher banner/navigation was not observed after hand-back" in SCRIPT
    assert 'device.write(b"nav SelPress\\n")' in SCRIPT
    assert 'line.startswith("OK nav")' in SCRIPT
    assert "handoff_complete" not in SCRIPT


def test_prepare_can_preserve_the_verified_launcher_session():
    assert "already_in_launcher: bool = False" in SCRIPT
    assert "if not already_in_launcher:" in SCRIPT
    assert "--already-in-launcher" in SCRIPT
    assert "prepare(args.port, args.require_free, args.already_in_launcher)" in SCRIPT


def test_installer_does_not_reset_after_successful_handoff():
    assert "prepare --port $Port --require-free $ImageBytes --already-in-launcher" in INSTALLER
