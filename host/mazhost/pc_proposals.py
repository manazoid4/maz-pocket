from __future__ import annotations

import json
import secrets
import time
from dataclasses import dataclass
from typing import Callable

from .llm import Models
from .pc import PCController


class ProposalError(RuntimeError):
    pass


@dataclass
class _Proposal:
    client_id: str
    action: str
    text: str
    preview: str
    expires_at: float
    receipt: dict | None = None


class ProposalStore:
    """Bounded, single-use confirmation gate for model-proposed PC actions."""

    _SYSTEM = (
        "Classify one user request into strict JSON. Allowed shapes only: "
        '{"action":"open_youtube"}, '
        '{"action":"create_notepad_note","text":"plain note text"}, or '
        '{"action":"unsupported"}. Do not add fields. Never emit a URL, path, '
        "filename, command, executable or authority."
    )

    def __init__(
        self,
        models: Models,
        pc: PCController,
        *,
        clock: Callable[[], float] = time.monotonic,
        ttl_seconds: int = 120,
        limit: int = 32,
    ) -> None:
        self._models = models
        self._pc = pc
        self._clock = clock
        self._ttl = ttl_seconds
        self._limit = limit
        self._items: dict[str, _Proposal] = {}

    def _prune(self) -> None:
        now = self._clock()
        expired = [key for key, item in self._items.items() if item.expires_at < now and item.receipt is None]
        for key in expired:
            self._items.pop(key, None)
        while len(self._items) >= self._limit:
            self._items.pop(next(iter(self._items)))

    @staticmethod
    def _validate(raw: str) -> tuple[str, str, str]:
        try:
            payload = json.loads(raw)
        except (TypeError, json.JSONDecodeError) as error:
            raise ProposalError("invalid_proposal") from error
        if not isinstance(payload, dict) or not isinstance(payload.get("action"), str):
            raise ProposalError("invalid_proposal")
        action = payload["action"]
        if action == "open_youtube" and set(payload) == {"action"}:
            return action, "", "Open YouTube"
        if action == "create_notepad_note" and set(payload) == {"action", "text"}:
            text = payload.get("text")
            if isinstance(text, str) and 1 <= len(text) <= 4000 and "\x00" not in text:
                return action, text, f"Create note: {text[:80]}"
        if action == "unsupported" and set(payload) == {"action"}:
            raise ProposalError("action_unsupported")
        raise ProposalError("invalid_proposal")

    def propose(self, client_id: str, request: str) -> dict:
        clean_client = client_id.strip()
        clean_request = request.strip()
        if not clean_client or not clean_request or len(clean_client) > 160 or len(clean_request) > 4000:
            raise ProposalError("invalid_request")
        reply, provider = self._models.chat(
            [
                {"role": "system", "content": self._SYSTEM},
                {"role": "user", "content": clean_request},
            ],
            "cloud",
        )
        action, text, preview = self._validate(reply)
        self._prune()
        proposal_id = secrets.token_urlsafe(18)
        expires_at = self._clock() + self._ttl
        self._items[proposal_id] = _Proposal(
            client_id=clean_client,
            action=action,
            text=text,
            preview=preview,
            expires_at=expires_at,
        )
        return {
            "ok": True,
            "proposal_id": proposal_id,
            "action": action,
            "preview": preview,
            "expires_in": self._ttl,
            "provider": provider,
        }

    def confirm(self, client_id: str, proposal_id: str) -> dict:
        item = self._items.get(proposal_id)
        if item is None or item.client_id != client_id.strip():
            raise ProposalError("proposal_not_found")
        if item.receipt is not None:
            return dict(item.receipt)
        if self._clock() > item.expires_at:
            raise ProposalError("proposal_expired")

        # Consume before dispatch. A replay returns the stored receipt and can
        # never launch the action twice.
        item.receipt = {
            "ok": False,
            "action": item.action,
            "reply": "launch outcome unknown",
            "provider": "pc-local",
        }
        result = self._pc.perform(item.action, item.text)
        item.receipt = {
            "ok": True,
            "action": result.action,
            "reply": result.label,
            "provider": "pc-local",
        }
        path = getattr(result, "path", "")
        if path:
            item.receipt["path"] = path
        return dict(item.receipt)
