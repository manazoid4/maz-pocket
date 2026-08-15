from __future__ import annotations

from typing import Literal

import httpx

from .config import Settings

Route = Literal["local", "auto", "cloud"]


class Models:
    def __init__(self, settings: Settings, client: httpx.Client | None = None) -> None:
        self.settings = settings
        self.client = client or httpx.Client(timeout=90)

    def status(self) -> dict:
        local = False
        installed = False
        try:
            response = self.client.get(f"{self.settings.ollama_url.rstrip('/')}/api/tags", timeout=2)
            local = response.is_success
            if local:
                names = {item.get("name", "") for item in response.json().get("models", [])}
                installed = self.settings.ollama_model in names
        except (httpx.HTTPError, KeyError, TypeError, ValueError):
            pass
        return {
            "local": local,
            "local_model": self.settings.ollama_model,
            "local_model_installed": installed,
            "cloud": bool(self.settings.cloud_key),
        }

    def _local(self, messages: list[dict[str, str]]) -> tuple[str, str]:
        response = self.client.post(
            f"{self.settings.ollama_url.rstrip('/')}/api/chat",
            json={
                "model": self.settings.ollama_model,
                "messages": messages,
                "stream": False,
                "keep_alive": "30m",
                "options": {
                    "temperature": 0.15,
                    "top_p": 0.85,
                    "repeat_penalty": 1.05,
                    "num_ctx": 8192,
                },
            },
        )
        response.raise_for_status()
        text = response.json()["message"]["content"].strip()
        if not text:
            raise RuntimeError("local_model_empty_reply")
        return text, f"local:{self.settings.ollama_model}"

    def chat(self, messages: list[dict[str, str]], route: Route) -> tuple[str, str]:
        if route in ("local", "auto"):
            try:
                return self._local(messages)
            except (httpx.HTTPError, KeyError, TypeError, ValueError, RuntimeError):
                if route == "local":
                    raise RuntimeError(
                        f"local_model_unavailable:{self.settings.ollama_model}"
                    )

        if not self.settings.cloud_key:
            raise RuntimeError("no_model_available")

        response = self.client.post(
            f"{self.settings.cloud_url.rstrip('/')}/chat/completions",
            headers={"Authorization": f"Bearer {self.settings.cloud_key}"},
            json={
                "model": self.settings.cloud_model,
                "messages": messages,
                "temperature": 0.2,
            },
        )
        response.raise_for_status()
        return response.json()["choices"][0]["message"]["content"].strip(), "cloud"
