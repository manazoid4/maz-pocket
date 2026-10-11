#!/usr/bin/env python3
"""nod_notify: send one notify-only ping to your nod. Any agent, any script.

    nod_notify.py <done|usage|needs|error> "text" [--source NAME]
    echo '{"message": "build ok"}' | nod_notify.py done --source codex

Env: NOD_REQUEST_TOKEN (preferred) or NOD_TOKEN, NOD_CORE_URL (default http://127.0.0.1:8787).
Legacy MAZ_TOKEN / MAZ_CORE_URL are used when the NOD_ ones are unset.
stdlib only. 3 s timeout. ALWAYS exits 0 and prints nothing: a notifier must
never break the tool that called it. Contract: docs/AGENT-EVENTS.md
"""
from __future__ import annotations

import json
import os
import sys
import urllib.request

KINDS = ("done", "usage", "needs", "error")
TEXT_FIELDS = ("text", "message", "last_assistant_message", "last-assistant-message",
               "prompt_response", "summary")  # last two: Codex, Gemini CLI


def env(name: str, default: str = "") -> str:
    return os.environ.get("NOD_" + name) or os.environ.get("MAZ_" + name) or default


def call(path: str, body: dict, timeout: float = 3):
    req = urllib.request.Request(
        env("CORE_URL", "http://127.0.0.1:8787").rstrip("/") + path,
        data=json.dumps(body).encode(), method="POST",
        headers={"Authorization": "Bearer " + (env("REQUEST_TOKEN") or env("TOKEN")), "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read())


def first_line(obj) -> str:
    """First non-empty line of the first text-like field, max 160 chars."""
    if isinstance(obj, dict):
        for key in TEXT_FIELDS:
            val = obj.get(key)
            if isinstance(val, str):
                for line in val.splitlines():
                    if line.strip():
                        return line.strip()[:160]
    return ""


def send(kind: str, text: str, source: str = "") -> None:
    """Post one event. Never raises."""
    try:
        if kind in KINDS and (env("REQUEST_TOKEN") or env("TOKEN")):
            call("/buddy/event", {"kind": kind, "text": text[:160], "source": source[:40]})
    except Exception:  # noqa: BLE001 - a notifier must never break its caller
        pass


def main(argv: list[str] | None = None) -> int:
    try:
        args = list(sys.argv[1:] if argv is None else argv)
        source = ""
        if "--source" in args:
            i = args.index("--source")
            source = args[i + 1] if i + 1 < len(args) else ""
            del args[i:i + 2]
        kind, text = (args + ["", ""])[:2]
        if text.lstrip().startswith("{"):  # agent passed its JSON payload as an argument (Codex)
            try:
                text = first_line(json.loads(text))
            except ValueError:
                pass
        if not text and not sys.stdin.isatty():
            raw = sys.stdin.read()
            try:
                text = first_line(json.loads(raw))
            except ValueError:
                text = next((ln.strip()[:160] for ln in raw.splitlines() if ln.strip()), "")
        send(kind, text, source)
    except Exception:  # noqa: BLE001
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
