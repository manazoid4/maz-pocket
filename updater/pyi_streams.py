"""PyInstaller runtime shim for the windowed Maz Pocket updater.

`--windowed` deliberately gives Python no console, so sys.stdin/stdout/stderr
can be None. esptool and the pinned M5Launcher helper legitimately call
.flush() on those streams even though the updater itself writes to Tk. Give
those libraries harmless real file objects instead of changing their code.
"""

import os
import sys


if sys.stdin is None:
    sys.stdin = open(os.devnull, "r", encoding="utf-8")
if sys.stdout is None:
    sys.stdout = open(os.devnull, "w", encoding="utf-8", buffering=1)
if sys.stderr is None:
    sys.stderr = open(os.devnull, "w", encoding="utf-8", buffering=1)
