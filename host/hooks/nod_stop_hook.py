#!/usr/bin/env python3
"""Claude Code Stop hook -> nod "done" ping (notify only).

Thin adapter over nod_notify.py: reads the Stop event JSON on stdin and sends
kind=done, source="claude-code". Same env, same guarantees (always exits 0,
silent). stdlib only.
"""
from __future__ import annotations

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nod_notify  # noqa: E402


def main() -> int:
    try:
        event = json.load(sys.stdin)
    except Exception:  # noqa: BLE001
        event = {}
    nod_notify.send("done", nod_notify.first_line(event) or "Claude finished", "claude-code")
    return 0


if __name__ == "__main__":
    sys.exit(main())
