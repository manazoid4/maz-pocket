#!/usr/bin/env python3
"""Claude Code Stop hook -> nod "done" ping (notify only).

Env: MAZ_TOKEN (required), MAZ_CORE_URL (default http://127.0.0.1:8787). stdlib only.
Posts kind=done to Core /buddy/event. 3 s timeout. Always exits 0, prints nothing:
a notification hook must never block Claude.
"""
from __future__ import annotations

import json
import os
import sys
import urllib.request

BASE = os.environ.get("MAZ_CORE_URL", "http://127.0.0.1:8787").rstrip("/")
TOKEN = os.environ.get("MAZ_TOKEN", "")


def call(method: str, path: str, body: dict, timeout: float = 3):
    req = urllib.request.Request(
        BASE + path, data=json.dumps(body).encode(), method=method,
        headers={"Authorization": f"Bearer {TOKEN}", "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read())


def first_line(event: dict) -> str:
    msg = event.get("last_assistant_message")
    if isinstance(msg, str):
        for line in msg.splitlines():
            if line.strip():
                return line.strip()[:160]
    return "Claude finished"


def main() -> int:
    try:
        try:
            event = json.load(sys.stdin)
        except ValueError:
            event = {}
        if TOKEN:
            call("POST", "/buddy/event", {"kind": "done", "text": first_line(event),
                                          "source": "claude-code"})
    except Exception:  # noqa: BLE001 - never block Claude
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
