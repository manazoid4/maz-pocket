"""Agent approval queue: Claude Code hook -> Core -> device (nod)."""
from __future__ import annotations

import threading
import time
import uuid
from collections import deque
from datetime import date
from typing import Callable, Literal

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

MAX_ITEMS = 50
MAX_EVENTS = 20
MAX_AGENTS = 8  # named agents on the Today screen; the rest count as "other"
OTHER = "other"
KIND_KEY = {"done": "done", "needs": "needs", "error": "errors", "usage": "usage"}


def _local_today() -> str:
    return date.today().isoformat()  # the hub's local calendar day


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


class Buddy:
    def __init__(self, today: Callable[[], str] | None = None) -> None:
        self._today = today or _local_today
        self._day = self._today()
        self._counts: dict[str, int] = {}
        self._agents: dict[str, int] = {}
        self._last: dict | None = None
        self._cv = threading.Condition()
        self._items: dict[str, dict] = {}
        self._allow_all: set[str] = set()
        self._events: deque[dict] = deque(maxlen=MAX_EVENTS)
        self._event_id = 0

    def _roll(self) -> None:
        """Start a fresh day when the local date changed. Call with the lock held."""
        today = self._today()
        if today != self._day:
            self._day, self._counts, self._agents, self._last = today, {}, {}, None

    def _bump(self, key: str) -> None:
        self._counts[key] = self._counts.get(key, 0) + 1

    def today(self) -> dict:
        with self._cv:
            self._roll()
            agents = sorted(self._agents.items(), key=lambda kv: (kv[0] == OTHER, -kv[1], kv[0]))
            return {"date": self._day, **{k: self._counts.get(k, 0) for k in
                    ("done", "needs", "errors", "usage", "approved", "denied")},
                    "agents": [{"source": s, "done": n} for s, n in agents],
                    "last": dict(self._last) if self._last else None}

    def post_event(self, kind: str, text: str, source: str = "") -> dict:
        with self._cv:
            self._roll()
            self._bump(KIND_KEY[kind])
            name = source.strip() or OTHER
            if kind == "done":
                named = sum(1 for a in self._agents if a != OTHER)
                if name not in self._agents and name != OTHER and named >= MAX_AGENTS:
                    name = OTHER
                self._agents[name] = self._agents.get(name, 0) + 1
            self._event_id += 1
            ev = {"id": self._event_id, "kind": kind, "text": text[:160],
                  "source": source, "ts": time.time()}
            self._events.append(ev)
            self._last = {k: ev[k] for k in ("kind", "text", "source", "ts")}
            return dict(ev)

    def events(self, since: int = 0) -> dict:
        with self._cv:
            return {"ok": True, "events": [dict(e) for e in self._events if e["id"] > since],
                    "last": self._event_id}

    def request(self, tool: str, summary: str, session_id: str = "") -> dict:
        with self._cv:
            rid = uuid.uuid4().hex[:8]
            auto = "allow" if session_id and session_id in self._allow_all else None
            if auto:
                self._roll()
                self._bump("approved")
            self._items[rid] = {"id": rid, "tool": tool, "summary": summary,
                                "session_id": session_id, "t": time.time(), "decision": auto}
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
                    return {"id": rid, "decision": item["decision"], "known": True}
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
            self._roll()
            if decision == "allow_all":
                if item["session_id"]:
                    self._allow_all.add(item["session_id"])
                for other in self._items.values():
                    if not other["decision"] and other["session_id"] == item["session_id"]:
                        other["decision"] = "allow"
                        self._bump("approved")
            else:
                item["decision"] = decision
                if decision != "cancel":
                    self._bump("approved" if decision == "allow" else "denied")
            self._cv.notify_all()
            return dict(item)

    def set_allow_all(self, session_id: str, on: bool) -> None:
        with self._cv:
            (self._allow_all.add if on else self._allow_all.discard)(session_id)


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
        buddy.set_allow_all(body.session_id, body.on)
        return {"ok": True, "session_id": body.session_id, "on": body.on}

    @api.post("/buddy/event")
    def buddy_event(body: BuddyEvent):
        return {"ok": True, **buddy.post_event(body.kind, body.text, body.source)}

    @api.get("/buddy/today")
    def buddy_today():
        return {"ok": True, **buddy.today()}

    @api.get("/buddy/events")
    def buddy_events(since: int = 0):
        return buddy.events(since)

    return buddy
