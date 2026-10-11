"""Agent approval queue: Claude Code hook -> Core -> device (nod)."""
from __future__ import annotations

import hashlib
import threading
import time
import uuid
from datetime import datetime, timezone
from collections import deque
from datetime import date
from typing import Callable, Literal

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

MAX_ITEMS = 50
MAX_EVENTS = 20     # notify-only ring buffer polled by the device
TIMEOUT_S = 60      # hook falls back to the terminal after this
WORKING_S = 90      # agent counts as 'working' this long after the last request
HALT_DEFAULT_MIN = 30
HALT_MAX_MIN = 480
HALT_REASON = "Halted from nod (permission request denied; running processes unaffected)"
MAX_AGENTS = 8
MAX_SUMMARY = 2000   # full request text; the device pages it, nothing is cut silently
READ_TOOLS = {"read", "glob", "grep", "ls", "notebookread", "todowrite", "exitplanmode"}
WRITE_TOOLS = {"edit", "write", "multiedit", "notebookedit"}
NET_TOOLS = {"webfetch", "websearch"}


def risk_word(tool: str) -> str:
    """READ / WRITE / NET / MCP / RUN for the approval card. Unknown tools are RUN (worst case)."""
    t = tool.strip().lower()
    if t in READ_TOOLS:
        return "READ"
    if t in WRITE_TOOLS:
        return "WRITE"
    if t in NET_TOOLS:
        return "NET"
    if t.startswith("mcp__"):
        return "MCP"
    return "RUN"


def short_hash(tool: str, summary: str) -> str:
    """6 hex chars of sha256(tool NUL full text): the same value is shown at the terminal side."""
    return hashlib.sha256(f"{tool}\x00{summary}".encode()).hexdigest()[:6]
OTHER = "other"
KIND_KEY = {"done": "done", "needs": "needs", "error": "errors", "usage": "usage"}

def _local_today() -> str:
    return date.today().isoformat()


class BuddyRequest(BaseModel):
    tool: str = Field(min_length=1, max_length=120)
    summary: str = Field(default="", max_length=MAX_SUMMARY)
    session_id: str = Field(default="", max_length=120)
    project: str = Field(default="", max_length=60)


class BuddyDecision(BaseModel):
    id: str
    decision: Literal["allow", "deny", "cancel"]


class BuddyEvent(BaseModel):
    """Notify-only ping (done / usage / needs / error). It can never approve,
    deny or decide anything: it only ever lands in the event ring below."""
    kind: Literal["done", "usage", "needs", "error"]
    text: str = Field(default="", max_length=2000)
    source: str = Field(default="", max_length=40)


class BuddyCancel(BaseModel):
    id: str


class HaltBody(BaseModel):
    on: bool
    minutes: int | None = Field(default=None, ge=1, le=HALT_MAX_MIN)


class Buddy:
    def __init__(self, today: Callable[[], str] | None = None) -> None:
        self._today = today or _local_today
        self._day = self._today()
        self._counts: dict[str, int] = {}
        self._agents: dict[str, int] = {}
        self._last: dict | None = None
        self._cv = threading.Condition()
        self._items: dict[str, dict] = {}
        self._events: deque[dict] = deque(maxlen=MAX_EVENTS)  # separate from _items on purpose
        self._event_id = 0
        self._halt_until_mono = 0.0
        self._halt_until_wall = 0.0
        # No persistent session-wide grants: every sensitive action needs a fresh decision.

    def _halted_locked(self) -> bool:
        if self._halt_until_mono and time.monotonic() >= self._halt_until_mono:
            self._halt_until_mono = self._halt_until_wall = 0.0
        return self._halt_until_mono > 0

    def halt_state(self) -> dict:
        with self._cv:
            return self._halt_state_locked()

    def _halt_state_locked(self) -> dict:
        halted = self._halted_locked()
        return {"ok": True, "halted": halted, "until":
                datetime.fromtimestamp(self._halt_until_wall, timezone.utc).isoformat()
                if halted else None}

    def set_halt(self, on: bool, minutes: int | None = None) -> dict:
        """Pause future nod-mediated asks; NEVER promises to stop running tools."""
        with self._cv:
            self._halted_locked()
            if on:
                duration = HALT_DEFAULT_MIN if minutes is None else minutes
                self._halt_until_mono = time.monotonic() + duration * 60
                self._halt_until_wall = time.time() + duration * 60
                self._roll()
                for item in self._items.values():
                    if item["decision"] in (None, "allow") and time.monotonic() < item["deadline"]:
                        if item["decision"] is None:
                            self._bump("denied")
                        item["decision"], item["reason"] = "deny", HALT_REASON
                self._cv.notify_all()
            else:
                self._halt_until_mono = self._halt_until_wall = 0.0
            return self._halt_state_locked()

    def _roll(self) -> None:
        current = self._today()
        if current != self._day:
            self._day, self._counts, self._agents, self._last = current, {}, {}, None

    def _bump(self, key: str) -> None:
        self._counts[key] = self._counts.get(key, 0) + 1

    def today(self) -> dict:
        with self._cv:
            self._roll()
            agents = sorted(self._agents.items(), key=lambda kv: (kv[0] == OTHER, -kv[1], kv[0]))
            return {"date": self._day, **{k: self._counts.get(k, 0)
                    for k in ("done", "needs", "errors", "usage", "approved", "denied")},
                    "agents": [{"source": src, "done": count} for src, count in agents],
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
            self._last = {key: ev[key] for key in ("kind", "text", "source", "ts")}
            return dict(ev)

    def events(self, since: int = 0) -> dict:
        with self._cv:
            return {"ok": True, "events": [dict(e) for e in self._events if e["id"] > since],
                    "last": self._event_id}

    def request(self, tool: str, summary: str, session_id: str = "", project: str = "") -> dict:
        with self._cv:
            rid = uuid.uuid4().hex[:8]
            auto = "deny" if self._halted_locked() else None
            if auto:
                self._roll()
                self._bump("denied")
            self._items[rid] = {"id": rid, "tool": tool, "summary": summary,
                                "session_id": session_id, "project": project, "t": time.time(),
                                "deadline": time.monotonic() + TIMEOUT_S,
                                "risk": risk_word(tool), "hash": short_hash(tool, summary),
                                "decision": auto}
            if auto:
                self._items[rid]["reason"] = HALT_REASON
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
                if time.monotonic() >= item["deadline"]:
                    if not item["decision"]:
                        item["decision"] = "cancel"
                        self._cv.notify_all()
                    # A grant consumed after its deadline is never safe, even if
                    # somebody approved it while the hook was temporarily offline.
                    return {"id": rid, "decision": "cancel", "known": True, "expired": True}
                if item["decision"] or time.monotonic() >= end:
                    result = {"id": rid, "decision": item["decision"], "known": True}
                    if item.get("reason"):
                        result["reason"] = item["reason"]
                    return result
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
                      "risk": i["risk"], "hash": i["hash"],
                      "left": max(0, int(i["deadline"] - monotonic_now))} for i in live[:limit]]
            return {"ok": True, "agent": "needs_you" if live else "working" if recent else "idle",
                    "count": len(live), "items": items}

    def decide(self, rid: str, decision: str) -> dict:
        with self._cv:
            item = self._items.get(rid)
            if item is None:
                raise KeyError(rid)
            if time.monotonic() >= item["deadline"]:
                if not item["decision"]:
                    item["decision"] = "cancel"
                    self._cv.notify_all()
                raise TimeoutError("approval_expired")
            if item["decision"]:
                return dict(item)
            if decision not in ("allow", "deny", "cancel"):
                raise ValueError("unsupported_decision")
            if self._halted_locked() and decision == "allow":
                decision = "deny"
                item["reason"] = HALT_REASON
            item["decision"] = decision
            if decision != "cancel":
                self._roll()
                self._bump("approved" if decision == "allow" else "denied")
            self._cv.notify_all()
            return dict(item)


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

    @api.post("/buddy/cancel")
    def buddy_cancel(body: BuddyCancel):
        """Request-token-safe: the hook withdraws its own request on timeout."""
        try:
            return buddy.decide(body.id, "cancel")
        except KeyError:
            raise HTTPException(404, "request_not_found")
        except TimeoutError:
            raise HTTPException(409, "approval_expired")

    @api.post("/buddy/event")
    def buddy_event(body: BuddyEvent):
        return {"ok": True, **buddy.post_event(body.kind, body.text, body.source)}

    @api.get("/buddy/today")
    def buddy_today():
        return {"ok": True, **buddy.today()}

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
