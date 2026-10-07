"""Focus sprint: timed task, mutes nudges, queues a "Done?" device event at the end."""
from __future__ import annotations

import json
import os
import re
import threading
import time
from pathlib import Path

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

DEFAULT_NOW = r"C:\Users\manaz\Desktop\Maz Works Knowledge Vault\NOW.md"
DEFAULT_LOG = "~/.maz-pocket/focus.jsonl"
UNCHECKED = re.compile(r"^\s*[-*]\s*\[ \]\s*(.+?)\s*$")


class FocusStart(BaseModel):
    minutes: int = Field(default=25, ge=1, le=180)
    task: str = Field(default="", max_length=200)


class FocusEnd(BaseModel):
    done: bool


class Focus:
    def __init__(self, now_file: str | None = None, log_file: str | None = None, clock=time.time) -> None:
        self.now_file = Path(now_file or os.environ.get("MAZ_NOW_FILE") or DEFAULT_NOW)
        self.log_file = Path(log_file or os.environ.get("MAZ_FOCUS_LOG") or DEFAULT_LOG).expanduser()
        self.clock = clock
        self._lock = threading.Lock()
        self._sprint: dict | None = None
        self._events: list[dict] = []

    def default_task(self) -> str:
        try:
            for line in self.now_file.read_text(encoding="utf-8").splitlines():
                m = UNCHECKED.match(line)
                if m:
                    return m.group(1)[:200]
        except OSError:
            pass
        return "Focus"

    def _tick(self) -> None:  # call with lock held
        s = self._sprint
        if s and s["state"] == "running" and self.clock() >= s["ends_at"]:
            s["state"] = "awaiting_done"
            self._events.append({"type": "focus_done_check", "text": "Done?", "task": s["task"]})

    def start(self, minutes: int = 25, task: str = "") -> dict:
        with self._lock:
            self._tick()
            now = self.clock()
            self._sprint = {"task": task.strip() or self.default_task(), "minutes": minutes,
                            "started_at": now, "ends_at": now + minutes * 60, "state": "running"}
            return self._view()

    def _view(self) -> dict:  # lock held
        s = self._sprint
        if not s:
            return {"state": "idle", "muted": False, "remaining_s": 0}
        left = max(0, int(s["ends_at"] - self.clock())) if s["state"] == "running" else 0
        return {**s, "muted": s["state"] != "idle", "remaining_s": left}

    def state(self) -> dict:
        with self._lock:
            self._tick()
            return self._view()

    @property
    def muted(self) -> bool:
        return self.state()["muted"]

    def end(self, done: bool) -> dict:
        with self._lock:
            self._tick()
            s = self._sprint
            if not s:
                raise KeyError("no_sprint")
            elapsed = min(self.clock(), s["ends_at"]) - s["started_at"]
            row = {"ts": self.clock(), "task": s["task"], "minutes": s["minutes"],
                   "elapsed_s": int(elapsed), "done": done}
            self.log_file.parent.mkdir(parents=True, exist_ok=True)
            with self.log_file.open("a", encoding="utf-8") as f:
                f.write(json.dumps(row) + "\n")
            self._sprint = None
            self._events = [e for e in self._events if e["type"] != "focus_done_check"]
            return row

    def pop_events(self) -> list[dict]:
        with self._lock:
            self._tick()
            out, self._events = self._events, []
            return out


def install_focus_routes(api: FastAPI, focus: Focus) -> Focus:
    @api.post("/focus/start")
    def focus_start(body: FocusStart | None = None):
        body = body or FocusStart()
        return focus.start(body.minutes, body.task)

    @api.get("/focus/state")
    def focus_state():
        return focus.state()

    @api.post("/focus/end")
    def focus_end(body: FocusEnd):
        try:
            return {"ok": True, "logged": focus.end(body.done)}
        except KeyError:
            raise HTTPException(409, "no_active_sprint")

    @api.get("/focus/events")
    def focus_events():
        return {"ok": True, "events": focus.pop_events()}

    return focus
