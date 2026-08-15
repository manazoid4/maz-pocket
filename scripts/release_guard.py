from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore")


def fail(message: str) -> None:
    raise SystemExit("release guard failed: " + message)


def active_files():
    for root in (ROOT / "src", ROOT / "host" / "mazhost"):
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in {".h", ".hpp", ".c", ".cpp", ".py"}:
                yield path


# Obsolete product paths must be deleted rather than silently retained.
for stale in (
    ROOT / "src" / "apps" / "v03.cpp",
    ROOT / "src" / "ui" / "lvgl_ui.cpp",
    ROOT / "src" / "ui" / "lvgl_ui.h",
):
    if stale.exists():
        fail(f"stale path still exists: {stale.relative_to(ROOT)}")

platformio = read(ROOT / "platformio.ini").lower()
if "lvgl" in platformio:
    fail("LVGL dependency/name returned to platformio.ini")

# Firmware must never take ownership of its own flash update path. Launcher is
# the sole updater and rollback boundary.
firmware_forbidden = (
    "arduinoota",
    "#include <update.h>",
    "#include \"update.h\"",
    "httpupdate",
    "esp_ota_begin",
    "esp_ota_write",
    "esp_ota_set_boot_partition",
)
for path in (ROOT / "src").rglob("*"):
    if not path.is_file():
        continue
    text = read(path).lower()
    for pattern in firmware_forbidden:
        if pattern in text:
            fail(f"firmware updater API {pattern!r} in {path.relative_to(ROOT)}")

# Remote-facing Python may call curated subprocess commands, but never through
# a shell parser or generic system()/eval bridge.
host_forbidden = (
    "shell=true",
    "os.system(",
    "eval(",
    "exec(",
    "cmd.exe /c",
    "powershell -command",
)
for path in (ROOT / "host" / "mazhost").rglob("*.py"):
    compact = re.sub(r"\s+", "", read(path).lower())
    for pattern in host_forbidden:
        if re.sub(r"\s+", "", pattern) in compact:
            fail(f"arbitrary shell/code execution pattern in {path.relative_to(ROOT)}: {pattern}")

# Catch common accidental credential commits while allowing the intentional
# placeholder in .env.example.
secret_patterns = (
    re.compile(r"sk-[A-Za-z0-9_-]{20,}"),
    re.compile(r"gh[pousr]_[A-Za-z0-9]{30,}"),
    re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
)
for path in ROOT.rglob("*"):
    if not path.is_file() or ".git" in path.parts or ".pio" in path.parts or "dist" in path.parts:
        continue
    if path.suffix.lower() not in {".py", ".cpp", ".c", ".h", ".hpp", ".md", ".txt", ".ps1", ".yml", ".yaml", ".json", ".ini", ".example"}:
        continue
    text = read(path)
    for pattern in secret_patterns:
        if pattern.search(text):
            fail(f"possible secret in {path.relative_to(ROOT)}")

print("release policy guard: PASS")
