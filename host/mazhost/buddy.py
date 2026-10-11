"""Agent approval queue: Claude Code hook -> Core -> device (nod)."""
from __future__ import annotations

import threading
import time
import uuid
from collections import deque
from typing import Literal

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

MAX_ITEMS = 50
MAX_EVENTS = 20
MAX_REQUEST_TTL_SECONDS = 60


class BuddyRequest(BaseModel):
    tool: str = Field(min_length=1, max_length=120)
    summary: str = Field(default="", max_length=200)
    session_id: str = Field(default="", max_length=120)
    ttl_seconds: int = Field(default=MAX_REQUEST_TTL_SECONDS, ge=1, le=MAX_REQUEST_TTL_SECONDS)


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
    def __init__(self) -> None:
        self._cv = threading.Condition()
        self._items: dict[str, dict] = {}
        self._events: deque[dict] = deque(maxlen=MAX_EVENTS)
        self._event_id = 0

    @staticmethod
    def _public(item: dict) -> dict:
        return {key: value for key, value in item.items() if not key.startswith("_")}

    def _expire(self, item: dict, now: float | None = None) -> None:
        current = time.monotonic() if now is None else now
        if item["decision"] is None and current >= item["_expires_at"]:
            item["decision"] = "expired"
            self._cv.notify_all()

    def post_event(self, kind: str, text: str, source: str = "") -> dict:
        with self._cv:
            self._event_id += 1
            ev = {"id": self._event_id, "kind": kind, "text": text[:160],
                  "source": source, "ts": time.time()}
            self._events.append(ev)
            return dict(ev)

    def events(self, since: int = 0) -> dict:
        with self._cv:
            return {"ok": True, "events": [dict(e) for e in self._events if e["id"] > since],
                    "last": self._event_id}

    def request(self, tool: str, summary: str, session_id: str = "",
                ttl_seconds: int = MAX_REQUEST_TTL_SECONDS) -> dict:
        with self._cv:
            rid = uuid.uuid4().hex[:8]
            self._items[rid] = {"id": rid, "tool": tool, "summary": summary,
                                "session_id": session_id, "t": time.time(), "decision": None,
                                "_expires_at": time.monotonic() + ttl_seconds}
            while len(self._items) > MAX_ITEMS:
                self._items.pop(next(iter(self._items)))  # dicts keep insertion order
            return self._public(self._items[rid])

    def state(self, rid: str, wait: float = 0) -> dict:
        end = time.monotonic() + wait
        with self._cv:
            while True:
                item = self._items.get(rid)
                if item is None:
                    return {"id": rid, "decision": None, "known": False}
                now = time.monotonic()
                self._expire(item, now)
                if item["decision"] or now >= end:
                    return {"id": rid, "decision": item["decision"], "known": True}
                self._cv.wait(max(0, min(end, item["_expires_at"]) - now))

    def pending(self) -> list[dict]:
        with self._cv:
            items = list(self._items.values())
            now = time.monotonic()
            for item in items:
                self._expire(item, now)
            return [self._public(i) for i in items if not i["decision"]]

    def decide(self, rid: str, decision: str) -> dict:
        if decision == "allow_all":
            raise ValueError("automatic_approval_disabled")
        with self._cv:
            item = self._items.get(rid)
            if item is None:
                raise KeyError(rid)
            self._expire(item)
            if item["decision"]:
                return self._public(item)
            item["decision"] = decision
            self._cv.notify_all()
            return self._public(item)


def install_buddy_routes(api: FastAPI, buddy: Buddy | None = None) -> Buddy:
    buddy = buddy or Buddy()

    @api.post("/buddy/request")
    def buddy_request(body: BuddyRequest):
        return buddy.request(body.tool, body.summary, body.session_id, body.ttl_seconds)

    @api.get("/buddy/state")
    def buddy_state(id: str, wait: float = 0):
        return buddy.state(id, min(max(wait, 0), 30))

    @api.get("/buddy/pending")
    def buddy_pending():
        return {"ok": True, "pending": buddy.pending()}

    @api.post("/buddy/decide")
    def buddy_decide(body: BuddyDecision):
        if body.decision == "allow_all":
            raise HTTPException(400, "automatic_approval_disabled")
        try:
            result = buddy.decide(body.id, body.decision)
        except KeyError:
            raise HTTPException(404, "request_not_found")
        except ValueError as error:
            raise HTTPException(400, str(error))
        if result["decision"] == "expired":
            raise HTTPException(409, "request_expired")
        return result

    @api.post("/buddy/allow-all")
    def buddy_allow_all(body: AllowAll):
        raise HTTPException(400, "automatic_approval_disabled")

    @api.post("/buddy/event")
    def buddy_event(body: BuddyEvent):
        return {"ok": True, **buddy.post_event(body.kind, body.text, body.source)}

    @api.get("/buddy/events")
    def buddy_events(since: int = 0):
        return buddy.events(since)

    return buddy
