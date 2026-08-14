from __future__ import annotations

from pathlib import Path

import httpx

from .config import Settings


class NudgeClient:
    def __init__(self, settings: Settings, client: httpx.Client | None = None) -> None:
        self.settings = settings
        self.client = client or httpx.Client(timeout=5)

    @property
    def token(self) -> str:
        if self.settings.nudge_token:
            return self.settings.nudge_token
        token_file = Path(self.settings.nudge_token_file).expanduser()
        if not token_file.is_file():
            return ""
        return token_file.read_text(encoding="utf-8").strip()

    @property
    def configured(self) -> bool:
        return bool(self.token)

    def _request(self, method: str, path: str):
        token = self.token
        if not token:
            raise RuntimeError("nudge_not_configured")
        response = self.client.request(
            method,
            f"{self.settings.nudge_url.rstrip('/')}{path}",
            headers={"Authorization": f"Bearer {token}"},
        )
        response.raise_for_status()
        return response.json()

    def summary(self):
        return self._request("GET", f"/v1/assurance?crossSyncDays={self.settings.nudge_cross_sync_days}")

    def detail(self, session_id: str):
        return self._request("GET", f"/v1/assurance/{session_id}?crossSyncDays={self.settings.nudge_cross_sync_days}")

    def nudge(self, session_id: str):
        return self._request("POST", f"/v1/assurance/{session_id}/nudge")

    def status(self) -> dict[str, bool]:
        if not self.configured:
            return {"configured": False, "online": False}
        try:
            self.summary()
            return {"configured": True, "online": True}
        except (RuntimeError, httpx.HTTPError):
            return {"configured": True, "online": False}
