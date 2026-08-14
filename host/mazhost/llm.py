from __future__ import annotations

from typing import Literal

import httpx

from .config import Settings

Route = Literal["local", "auto", "cloud"]


class Models:
    def __init__(self, settings: Settings, client: httpx.Client | None = None) -> None:
        self.settings = settings
        self.client = client or httpx.Client(timeout=60)

    def status(self) -> dict[str, bool]:
        local = False
        try:
            local = self.client.get(f"{self.settings.ollama_url.rstrip('/')}/api/tags", timeout=2).is_success
        except httpx.HTTPError:
            pass
        return {"local": local, "cloud": bool(self.settings.cloud_key)}

    def chat(self, messages: list[dict[str, str]], route: Route) -> tuple[str, str]:
        if route in ("local", "auto"):
            try:
                response = self.client.post(
                    f"{self.settings.ollama_url.rstrip('/')}/api/chat",
                    json={"model": self.settings.ollama_model, "messages": messages, "stream": False},
                )
                response.raise_for_status()
                return response.json()["message"]["content"].strip(), "local"
            except (httpx.HTTPError, KeyError, TypeError):
                if route == "local":
                    raise RuntimeError("local_model_unavailable")
        if not self.settings.cloud_key:
            raise RuntimeError("no_model_available")
        response = self.client.post(
            f"{self.settings.cloud_url.rstrip('/')}/chat/completions",
            headers={"Authorization": f"Bearer {self.settings.cloud_key}"},
            json={"model": self.settings.cloud_model, "messages": messages},
        )
        response.raise_for_status()
        return response.json()["choices"][0]["message"]["content"].strip(), "cloud"
