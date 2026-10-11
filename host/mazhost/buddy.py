"""Agent approval queue: Claude Code hook -> Core -> device (nod)."""
from __future__ import annotations

import threading
import time
import uuid
from collections import deque
from datetime import datetime, timezone
from typing import Literal

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

MAX_ITEMS = 50
MAX_EVENTS = 20
HALT_DEFAULT_MIN = 30
HALT_MAX_MIN = 480
HALT_REASON = "Halted from nod"


def _now() -> float:  # tests swap this for a fake clock
    return time.time()


class BuddyRequest(BaseModel):
    tool: str = Field(min_length=1, max_length=120)
    summary: str = Field(default="", max_length=200)
    session_id: str = Field(default="", max_length=120)


class BuddyDecision(BaseModel):
    id: str
    decision: Literal["allow", "deny", "allow_all", "cancel"]


class BuddyEvent(BaseModel):
    """Notify-only ping (done / usage / needs / error). Never approves anything."""
    kind: Literal["done", "usage", "needs", "error"]
    text: str = ""
    source: str = Field(default="", max_length=40)


class AllowAll(BaseModel):
    session_id: str = Field(min_length=1, max_length=120)
    on: bool = False


class HaltBody(BaseModel):
    on: bool
    minutes: int | None = Field(default=None, ge=1, le=HALT_MAX_MIN)


class Buddy:
    def __init__(self) -> None:
        self._cv = threading.Condition()
        self._items: dict[str, dict] = {}
        self._allow_all: set[str] = set()
        self._events: deque[dict] = deque(maxlen=MAX_EVENTS)
        self._event_id = 0
        self._halt_until: float | None = None  # memory only: a hub restart clears Halt

    def _expire_halt(self) -> None:  # call with the lock held
        if self._halt_until is not None and _now() >= self._halt_until:
            self._halt_until = None
            self.post_event("needs", "Halt off", "halt")

    def halt_state(self) -> dict:
        with self._cv:
            self._expire_halt()
            until = self._halt_until
            return {"ok": True, "halted": until is not None,
                    "until": datetime.fromtimestamp(until, timezone.utc).isoformat()
                    if until is not None else None}

    def set_halt(self, on: bool, minutes: int | None = None) -> dict:
        """Halt only ever denies. It never approves anything."""
        with self._cv:
            self._expire_halt()
            was = self._halt_until is not None
            if on:
                self._halt_until = _now() + (minutes or HALT_DEFAULT_MIN) * 60
                # Do not let a pre-existing session grant resume after Halt.
                self._allow_all.clear()
                for item in self._items.values():
                    if not item["decision"]:
                        item["decision"], item["reason"] = "deny", HALT_REASON
                self._cv.notify_all()
            else:
                self._halt_until = None
            if on != was:
                self.post_event("needs", "Halt on" if on else "Halt off", "halt")
        return self.halt_state()

    def post_event(self, kind: str, text: str, source: str = "") -> dict:
        with self._cv:
            self._event_id += 1
            ev = {"id": self._event_id, "kind": kind, "text": text[:160],
                  "source": source, "ts": time.time()}
            self._events.append(ev)
            return dict(ev)

    def events(self, since: int = 0) -> dict:
        with self._cv:
            self._expire_halt()
            return {"ok": True, "events": [dict(e) for e in self._events if e["id"] > since],
                    "last": self._event_id}

    def request(self, tool: str, summary: str, session_id: str = "") -> dict:
        with self._cv:
            self._expire_halt()
            rid = uuid.uuid4().hex[:8]
            auto = "allow" if session_id and session_id in self._allow_all else None
            item = {"id": rid, "tool": tool, "summary": summary,
                    "session_id": session_id, "t": time.time(), "decision": auto}
            if self._halt_until is not None:  # Halt beats allow-all
                item["decision"], item["reason"] = "deny", HALT_REASON
            self._items[rid] = item
            while len(self._items) > MAX_ITEMS:
                self._items.pop(next(iter(self._items)))  # dicts keep insertion order
            return dict(self._items[rid])

    def state(self, rid: str, wait: float = 0) -> dict:
        end = time.monotonic() + wait
        with self._cv:
            while True:
                item = self._items.get(rid)
                if item is None:
                    return {"id": rid, "decision": None, "known": False}
                if item["decision"] or time.monotonic() >= end:
                    out = {"id": rid, "decision": item["decision"], "known": True}
                    if item.get("reason"):
                        out["reason"] = item["reason"]
                    return out
                self._cv.wait(end - time.monotonic())

    def pending(self) -> list[dict]:
        with self._cv:
            return [dict(i) for i in self._items.values() if not i["decision"]]

    def decide(self, rid: str, decision: str) -> dict:
        with self._cv:
            item = self._items.get(rid)
            if item is None:
                raise KeyError(rid)
            if item["decision"]:
                return dict(item)
            if decision == "allow_all":
                if item["session_id"]:
                    self._allow_all.add(item["session_id"])
                for other in self._items.values():
                    if not other["decision"] and other["session_id"] == item["session_id"]:
                        other["decision"] = "allow"
            else:
                item["decision"] = decision
            self._cv.notify_all()
            return dict(item)

    def set_allow_all(self, session_id: str, on: bool) -> bool:
        with self._cv:
            self._expire_halt()
            if on and self._halt_until is not None:
                return False
            (self._allow_all.add if on else self._allow_all.discard)(session_id)
            return True


def install_buddy_routes(api: FastAPI, buddy: Buddy | None = None) -> Buddy:
    buddy = buddy or Buddy()

    @api.post("/buddy/request")
    def buddy_request(body: BuddyRequest):
        return buddy.request(body.tool, body.summary, body.session_id)

    @api.get("/buddy/state")
    def buddy_state(id: str, wait: float = 0):
        return buddy.state(id, min(max(wait, 0), 30))

    @api.get("/buddy/pending")
    def buddy_pending():
        return {"ok": True, "pending": buddy.pending()}

    @api.post("/buddy/decide")
    def buddy_decide(body: BuddyDecision):
        try:
            return buddy.decide(body.id, body.decision)
        except KeyError:
            raise HTTPException(404, "request_not_found")

    @api.post("/buddy/allow-all")
    def buddy_allow_all(body: AllowAll):
        if not buddy.set_allow_all(body.session_id, body.on):
            raise HTTPException(409, "halt_active")
        return {"ok": True, "session_id": body.session_id, "on": body.on}

    @api.post("/buddy/event")
    def buddy_event(body: BuddyEvent):
        return {"ok": True, **buddy.post_event(body.kind, body.text, body.source)}

    @api.get("/buddy/events")
    def buddy_events(since: int = 0):
        return buddy.events(since)

    @api.get("/buddy/halt")
    def buddy_halt_get():
        return buddy.halt_state()

    @api.post("/buddy/halt")
    def buddy_halt(body: HaltBody):
        return buddy.set_halt(body.on, body.minutes)

    return buddy
