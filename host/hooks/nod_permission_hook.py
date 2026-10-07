#!/usr/bin/env python3
"""Claude Code PermissionRequest hook -> nod Core approval.

Env: MAZ_TOKEN (required), MAZ_CORE_URL (default http://127.0.0.1:8787),
MAZ_BUDDY_TIMEOUT (seconds, default 60). stdlib only.

Device allow/deny -> decision JSON on stdout.
Timeout or any error -> no stdout, "ask" on stderr, exit 0: Claude Code then
shows its normal terminal prompt.
"""
from __future__ import annotations

import json
import os
import sys
import time
import urllib.error
import urllib.request

BASE = os.environ.get("MAZ_CORE_URL", "http://127.0.0.1:8787").rstrip("/")
TOKEN = os.environ.get("MAZ_TOKEN", "")
TIMEOUT = float(os.environ.get("MAZ_BUDDY_TIMEOUT", "60"))


def call(method: str, path: str, body: dict | None = None, timeout: float = 10):
    req = urllib.request.Request(
        BASE + path,
        data=json.dumps(body).encode() if body is not None else None,
        method=method,
        headers={"Authorization": f"Bearer {TOKEN}", "Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read())


def summarize(tool_input: dict) -> str:
    text = (tool_input.get("command") or tool_input.get("file_path") or tool_input.get("url")
            or json.dumps(tool_input, separators=(",", ":")))
    return " ".join(str(text).split())[:200]


def decide(event: dict) -> str | None:
    """'allow' | 'deny' | None (None = ask in terminal)."""
    if not TOKEN:
        return None
    try:
        req = call("POST", "/buddy/request", {
            "tool": event.get("tool_name", "?"),
            "summary": summarize(event.get("tool_input") or {}),
            "session_id": event.get("session_id", ""),
            "project": str(event.get("cwd", "")).replace("\\", "/").rstrip("/").rsplit("/", 1)[-1][:60],
        })
        end = time.monotonic() + TIMEOUT
        while True:
            if req.get("decision") in ("allow", "deny"):
                return req["decision"]
            left = end - time.monotonic()
            if left <= 0:
                break
            req = call("GET", f"/buddy/state?id={req['id']}&wait={min(left, 20):.2f}",
                       timeout=min(left, 20) + 5)
        call("POST", "/buddy/decide", {"id": req["id"], "decision": "cancel"})
    except (urllib.error.URLError, OSError, ValueError, KeyError):
        pass
    return None


def main() -> int:
    try:
        event = json.load(sys.stdin)
    except ValueError:
        event = {}
    verdict = decide(event)
    if not verdict:
        print("ask", file=sys.stderr)
        return 0
    decision = {"behavior": verdict}
    if verdict == "deny":
        decision["message"] = "Denied from nod device"
    print(json.dumps({"hookSpecificOutput": {"hookEventName": "PermissionRequest",
                                             "decision": decision}}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
