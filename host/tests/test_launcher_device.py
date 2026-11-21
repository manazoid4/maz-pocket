from pathlib import Path
import importlib.util

import pytest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = (ROOT / "scripts" / "launcher-device.py").read_text(encoding="utf-8")
INSTALLER = (ROOT / "scripts" / "install.ps1").read_text(encoding="utf-8")


def test_handoff_requires_launcher_banner_and_serial_navigation():
    assert "M5Launcher banner/navigation was not observed after hand-back" in SCRIPT
    assert 'device.write(b"nav SelPress\\n")' in SCRIPT
    assert 'line.startswith("OK nav")' in SCRIPT


def test_installer_keeps_verified_launcher_session():
    assert "already_in_launcher: bool = False" in SCRIPT
    assert "if not already_in_launcher:" in SCRIPT
    assert "prepare --port $Port --require-free $ImageBytes --already-in-launcher" in INSTALLER


def launcher_module():
    spec = importlib.util.spec_from_file_location("launcher_device", ROOT / "scripts" / "launcher-device.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FakeSerial:
    def __enter__(self):
        return self

    def __exit__(self, *args):
        pass


@pytest.mark.parametrize("free,storage,error", [
    (192 * 1024, True, "Insufficient free flash"),
    (4 * 1024 * 1024, False, "Existing MAZ storage"),
])
def test_prepare_preserves_all_partitions_on_failed_preflight(monkeypatch, free, storage, error):
    module = launcher_module()
    commands = []
    lines = ["mazpoc type=app", "otherapp type=app"]
    if storage:
        lines.append("mazdata type=data")
    monkeypatch.setattr(module.serial, "Serial", lambda *a, **kw: FakeSerial())
    monkeypatch.setattr(module, "read_line", lambda *a: module.BOOT_BANNER)
    monkeypatch.setattr(module, "partitions", lambda *a: (lines, free))
    monkeypatch.setattr(module, "command", lambda device, cmd, *a, **kw: commands.append(cmd) or ["OK nav"])
    with pytest.raises(RuntimeError, match=error):
        module.prepare("COM_TEST", 1500000, already_in_launcher=True)
    assert not any(cmd.startswith("partition ") for cmd in commands)


def test_prepare_keeps_existing_apps_and_storage_when_image_fits(monkeypatch):
    module = launcher_module()
    commands = []
    monkeypatch.setattr(module.serial, "Serial", lambda *a, **kw: FakeSerial())
    monkeypatch.setattr(module, "read_line", lambda *a: module.BOOT_BANNER)
    monkeypatch.setattr(module, "partitions", lambda *a: (["mazpoc type=app", "mazpo2 type=app", "mazdata type=data"], 3 * 1024 * 1024))
    monkeypatch.setattr(module, "command", lambda device, cmd, *a, **kw: commands.append(cmd) or ["OK nav"])
    module.prepare("COM_TEST", 1500000, already_in_launcher=True)
    assert not any(cmd.startswith("partition ") for cmd in commands)
