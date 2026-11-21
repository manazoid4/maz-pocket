from __future__ import annotations

import os
import shutil
import subprocess
import sys
import winreg
from pathlib import Path


def app_dir() -> Path:
    return Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local")) / "MAZ Core"


def install_if_needed() -> bool:
    if not getattr(sys, "frozen", False) or "--installed" in sys.argv or "--serve" in sys.argv:
        return False
    install_dir = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local")) / "Programs" / "MAZ Core"
    install_dir.mkdir(parents=True, exist_ok=True)
    target = install_dir / "MAZ Core.exe"
    source = Path(sys.executable).resolve()
    if source != target.resolve():
        shutil.copy2(source, target)
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\Run", 0, winreg.KEY_SET_VALUE) as key:
        winreg.SetValueEx(key, "MAZ Core", 0, winreg.REG_SZ, f'"{target}" --installed')
    desktop = Path(os.environ.get("USERPROFILE", str(Path.home()))) / "Desktop"
    shortcut = desktop / "MAZ Core.url"
    shortcut.write_text(f"[InternetShortcut]\nURL=file:///{target.as_posix()}\n", encoding="ascii")
    subprocess.Popen([str(target), "--installed"], cwd=app_dir(), close_fds=True)
    return True


def main() -> None:
    config = app_dir()
    config.mkdir(parents=True, exist_ok=True)
    os.chdir(config)
    if install_if_needed():
        return
    if "--serve" in sys.argv:
        import uvicorn
        from mazhost.config import Settings
        settings = Settings()
        uvicorn.run("mazhost.app:app", host=settings.bind, port=settings.port)
        return
    from mazhost.desktop import main as desktop_main
    desktop_main()


if __name__ == "__main__":
    main()
