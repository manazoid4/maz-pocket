"""Agent approval queue: Claude Code hook -> Core -> device (nod)."""
from __future__ import annotations

import threading
import time
import uuid
from typing import Literal

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

MAX_ITEMS = 50


class BuddyRequest(BaseModel):
    tool: str = Field(min_length=1, max_length=120)
    summary: str = Field(default="", max_length=200)
    session_id: str = Field(default="", max_length=120)


class BuddyDecision(BaseModel):
    id: str
    decision: Literal["allow", "deny", "allow_all", "cancel"]


class AllowAll(BaseModel):
    session_id: str = Field(min_length=1, max_length=120)
    on: bool = False


class Buddy:
    def __init__(self) -> None:
        self._cv = threading.Condition()
        self._items: dict[str, dict] = {}
        self._allow_all: set[str] = set()

    def request(self, tool: str, summary: str, session_id: str = "") -> dict:
        with self._cv:
            rid = uuid.uuid4().hex[:8]
            auto = "allow" if session_id and session_id in self._allow_all else None
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

    return buddy
