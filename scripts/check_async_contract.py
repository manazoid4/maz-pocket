from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCAN = [ROOT / "src" / "apps", ROOT / "src" / "net" / "portal.cpp"]
BLOCKING_CALLS = (
    "host::startSession(",
    "host::talkAudio(",
    "host::brainDump(",
    "host::pcAction(",
    "host::assurance(",
    "host::sendNudge(",
    "host::speak(",
    "host::coreStatus(",
    "host::coreProjects(",
    "host::coreStartJob(",
    "host::coreJob(",
    "host::health(",
)

files: list[Path] = []
for item in SCAN:
    files.extend(item.rglob("*.cpp") if item.is_dir() else [item])

bad: list[str] = []
for path in files:
    text = path.read_text(encoding="utf-8", errors="ignore")
    for call in BLOCKING_CALLS:
        if call in text:
            bad.append(f"{path.relative_to(ROOT)} -> {call}")
    if re.search(r"\bHTTPClient\b", text):
        bad.append(f"{path.relative_to(ROOT)} -> HTTPClient in UI/main-task code")

if bad:
    raise SystemExit("blocking Host I/O escaped worker:\n" + "\n".join(bad))
print("async Host I/O contract: PASS")
