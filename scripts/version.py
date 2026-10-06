Import("env")

import re, subprocess
from pathlib import Path

root = Path(env["PROJECT_DIR"])
version_path = root / "VERSION"
version = version_path.read_text(encoding="utf-8").strip()
if not re.fullmatch(r"\d+\.\d+(?:\.\d+)?", version):
    raise RuntimeError(f"Invalid MAZ Pocket VERSION: {version!r}")

# One canonical version feeds the firmware banner/status instead of duplicating
# a hard-coded define in platformio.ini and every packaging script.
env.Append(CPPDEFINES=[("MAZ_POCKET_VERSION", f'\\"{version}\\"')])
print(f"MAZ Pocket version: {version}")

try:
    sha = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=root,
                                  stderr=subprocess.DEVNULL, text=True).strip()
except Exception:
    sha = ""
env.Append(CPPDEFINES=[("NOD_FW_VERSION", f'\\"{version}\\"'),
                       ("NOD_FW_SHA", f'\\"{sha or "nogit"}\\"')])
