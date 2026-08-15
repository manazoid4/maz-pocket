"""Production ASGI entry point for MAZ Core v0.5.1.

The existing REST app stays the compatibility surface. This module adds the
WebSocket COMM transport with the same local model, project grounding, Agent
Nudge evidence and pairing token, then exports the combined app.
"""

from __future__ import annotations

import json

import httpx
from fastapi import HTTPException

from .app import app
from .commands import parse_command
from .comm_stream import install_comm_stream
from .config import Settings
from .core import CoreError, MazCore
from .llm import Models
from .nudge import NudgeClient
from .pc import PCController
from .prompts import SYSTEM_PROMPT
from .refine import refine
from .security import Security
from .sessions import SessionStore
from .stt import SpeechToText

cfg = Settings()
security = Security(cfg)
speech = SpeechToText(cfg)
models = Models(cfg)
nudge = NudgeClient(cfg)
core = MazCore(cfg)
pc = PCController()
sessions = SessionStore(cfg.max_turns, cfg.session_ttl_minutes)


def grounded_messages(session_id: str, text: str) -> list[dict[str, str]]:
    history = sessions.messages(session_id)
    if not history and not sessions.has(session_id):
        raise HTTPException(404, "session_not_found")
    context = ""
    if cfg.core_enabled:
        try:
            context += core.context_for_prompt(text)
        except (CoreError, OSError, ValueError) as error:
            context += f"\nMAZ Core evidence unavailable: {error}. Say this plainly if relevant."
    if any(word in text.lower() for word in ("agent", "sync", "nudge", "stale", "working")):
        try:
            context += "\nAgent Nudge factual evidence:\n" + json.dumps(nudge.summary())
        except (RuntimeError, httpx.HTTPError):
            context += "\nAgent Nudge is unavailable; say that plainly."
    return [
        {"role": "system", "content": SYSTEM_PROMPT + context},
        *history,
        {"role": "user", "content": text},
    ]


def deterministic_command(session_id: str, text: str, command: dict, actions: list) -> dict:
    if command["type"] == "reminder.create":
        reply = f"Reminder set: {command['title']}"
        provider = "deterministic-local"
    elif command["type"] == "pc.action":
        try:
            result = pc.perform(command["action"])
        except RuntimeError as error:
            raise HTTPException(503, str(error)) from error
        reply = f"PC: {result.label}"
        provider = "pc-local"
    else:
        raise HTTPException(400, "unsupported_command")
    sessions.add_turn(session_id, text, reply)
    return {
        "text": text,
        "reply": reply,
        "provider": provider,
        "actions": actions,
        "commands": [command],
        "timings": {"llm_ms": 0},
    }


install_comm_stream(
    app,
    cfg=cfg,
    security=security,
    speech=speech,
    model_router=models,
    sessions=sessions,
    grounded_messages=grounded_messages,
    deterministic_command=deterministic_command,
)
app.version = "0.5.1"
