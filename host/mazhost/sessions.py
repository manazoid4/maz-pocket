"""Conversation memory.

In-memory on purpose. A session is a conversation you are having right now; if
the host restarts mid-thought the honest behaviour is to start again rather than
resurrect half a context.
"""

from __future__ import annotations

import secrets
import time
from dataclasses import dataclass, field


@dataclass
class Session:
    id: str
    created_at: float
    touched_at: float
    messages: list[dict[str, str]] = field(default_factory=list)


class SessionStore:
    def __init__(self, max_turns: int = 24, ttl_minutes: int = 120) -> None:
        self._max_turns = max_turns
        self._ttl = ttl_minutes * 60
        self._sessions: dict[str, Session] = {}

    def start(self) -> str:
        self._evict()
        sid = secrets.token_urlsafe(9)
        now = time.time()
        self._sessions[sid] = Session(id=sid, created_at=now, touched_at=now)
        return sid

    def add_turn(self, sid: str, user_text: str, assistant_text: str) -> None:
        self._evict()
        session = self._sessions.get(sid)
        if session is None:
            raise KeyError(sid)
        session.messages.append({"role": "user", "content": user_text})
        session.messages.append({"role": "assistant", "content": assistant_text})
        session.messages = session.messages[-self._max_turns * 2 :]
        session.touched_at = time.time()

    def messages(self, sid: str) -> list[dict[str, str]]:
        self._evict()
        session = self._sessions.get(sid)
        return list(session.messages) if session else []

    def end(self, sid: str) -> bool:
        return self._sessions.pop(sid, None) is not None

    def has(self, sid: str) -> bool:
        self._evict()
        return sid in self._sessions

    def _evict(self) -> None:
        cutoff = time.time() - self._ttl
        expired = [sid for sid, session in self._sessions.items() if session.touched_at < cutoff]
        for sid in expired:
            del self._sessions[sid]
