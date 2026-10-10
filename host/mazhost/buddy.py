"""Agent approval queue: Claude Code hook -> Core -> device (nod)."""
from __future__ import annotations

import threading
import time
import uuid
from typing import Literal

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

MAX_ITEMS = 50
TIMEOUT_S = 60      # hook falls back to the terminal after this
WORKING_S = 90      # agent counts as 'working' this long after the last request


class BuddyRequest(BaseModel):
    tool: str = Field(min_length=1, max_length=120)
    summary: str = Field(default="", max_length=200)
    session_id: str = Field(default="", max_length=120)
    project: str = Field(default="", max_length=60)


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
        # No persistent session-wide grants: every sensitive action needs a fresh decision.

    def request(self, tool: str, summary: str, session_id: str = "", project: str = "") -> dict:
        with self._cv:
            rid = uuid.uuid4().hex[:8]
            auto = None  # never inherit approval from another request
            self._items[rid] = {"id": rid, "tool": tool, "summary": summary,
                                "session_id": session_id, "project": project, "t": time.time(),
                                "deadline": time.monotonic() + TIMEOUT_S,
                                "decision": auto}
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
                if not item["decision"] and time.monotonic() >= item["deadline"]:
                    item["decision"] = "cancel"
                    self._cv.notify_all()
                if item["decision"] or time.monotonic() >= end:
                    return {"id": rid, "decision": item["decision"], "known": True}
                self._cv.wait(end - time.monotonic())

    def pending(self) -> list[dict]:
        with self._cv:
            now = time.monotonic()
            return [dict(i) for i in self._items.values()
                    if not i["decision"] and now < i["deadline"]]

    def summary(self, limit: int = 3) -> dict:
        """Cheap device poll: oldest-first live pending items + idle/working/needs_you."""
        now = time.time()
        monotonic_now = time.monotonic()
        with self._cv:
            live = [i for i in self._items.values()
                    if not i["decision"] and monotonic_now < i["deadline"]]
            recent = any(now - i["t"] < WORKING_S for i in self._items.values())
            items = [{"id": i["id"], "tool": i["tool"], "summary": i["summary"],
                      "project": i.get("project", ""), "session_id": i["session_id"],
                      "left": max(0, int(i["deadline"] - monotonic_now))} for i in live[:limit]]
            return {"ok": True, "agent": "needs_you" if live else "working" if recent else "idle",
                    "count": len(live), "items": items}

    def decide(self, rid: str, decision: str) -> dict:
        with self._cv:
            item = self._items.get(rid)
            if item is None:
                raise KeyError(rid)
            if item["decision"]:
                return dict(item)
            if decision == "allow_all":
                raise ValueError("session_wide_approval_disabled")
            if time.monotonic() >= item["deadline"]:
                item["decision"] = "cancel"
                self._cv.notify_all()
                raise TimeoutError("approval_expired")
            if decision not in ("allow", "deny", "cancel"):
                raise ValueError("unsupported_decision")
            item["decision"] = decision
            self._cv.notify_all()
            return dict(item)

    def set_allow_all(self, session_id: str, on: bool) -> None:
        if on:
            raise ValueError("session_wide_approval_disabled")
        # Revocation stays backwards-compatible for old clients. Nothing to revoke.


def install_buddy_routes(api: FastAPI, buddy: Buddy | None = None) -> Buddy:
    buddy = buddy or Buddy()

    @api.post("/buddy/request")
    def buddy_request(body: BuddyRequest):
        return buddy.request(body.tool, body.summary, body.session_id, body.project)

    @api.get("/buddy/state")
    def buddy_state(id: str, wait: float = 0):
        return buddy.state(id, min(max(wait, 0), 30))

    @api.get("/buddy/summary")
    def buddy_summary():
        return buddy.summary()

    @api.get("/buddy/pending")
    def buddy_pending():
        return {"ok": True, "pending": buddy.pending()}

    @api.post("/buddy/decide")
    def buddy_decide(body: BuddyDecision):
        try:
            return buddy.decide(body.id, body.decision)
        except KeyError:
            raise HTTPException(404, "request_not_found")
        except TimeoutError:
            raise HTTPException(409, "approval_expired")
        except ValueError as error:
            raise HTTPException(400, str(error))

    @api.post("/buddy/allow-all")
    def buddy_allow_all(body: AllowAll):
        try:
            buddy.set_allow_all(body.session_id, body.on)
        except ValueError as error:
            raise HTTPException(400, str(error))
        return {"ok": True, "session_id": body.session_id, "on": False}

    return buddy
