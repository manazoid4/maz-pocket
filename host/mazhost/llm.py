from __future__ import annotations

import json
from collections.abc import Iterator
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

    def _options(self) -> dict:
        return {
            "temperature": 0.15,
            "top_p": 0.85,
            "repeat_penalty": 1.05,
            "num_ctx": 8192,
        }

    def _local(self, messages: list[dict[str, str]]) -> tuple[str, str]:
        response = self.client.post(
            f"{self.settings.ollama_url.rstrip('/')}/api/chat",
            json={
                "model": self.settings.ollama_model,
                "messages": messages,
                "stream": False,
                "keep_alive": "30m",
                "options": self._options(),
            },
        )
        response.raise_for_status()
        text = response.json()["message"]["content"].strip()
        if not text:
            raise RuntimeError("local_model_empty_reply")
        return text, f"local:{self.settings.ollama_model}"

    def _local_stream(self, messages: list[dict[str, str]]) -> Iterator[tuple[str, str]]:
        provider = f"local:{self.settings.ollama_model}"
        saw_text = False
        with self.client.stream(
            "POST",
            f"{self.settings.ollama_url.rstrip('/')}/api/chat",
            json={
                "model": self.settings.ollama_model,
                "messages": messages,
                "stream": True,
                "keep_alive": "30m",
                "options": self._options(),
            },
            timeout=90,
        ) as response:
            response.raise_for_status()
            for line in response.iter_lines():
                if not line:
                    continue
                payload = json.loads(line)
                chunk = str((payload.get("message") or {}).get("content") or "")
                if chunk:
                    saw_text = True
                    yield chunk, provider
                if payload.get("done"):
                    break
        if not saw_text:
            raise RuntimeError("local_model_empty_reply")

    def _cloud(self, messages: list[dict[str, str]]) -> tuple[str, str]:
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
        text = response.json()["choices"][0]["message"]["content"].strip()
        if not text:
            raise RuntimeError("cloud_model_empty_reply")
        return text, "cloud"

    def chat(self, messages: list[dict[str, str]], route: Route) -> tuple[str, str]:
        if route in ("local", "auto"):
            try:
                return self._local(messages)
            except (httpx.HTTPError, KeyError, TypeError, ValueError, RuntimeError):
                if route == "local":
                    raise RuntimeError(
                        f"local_model_unavailable:{self.settings.ollama_model}"
                    )
        return self._cloud(messages)

    def stream_chat(
        self, messages: list[dict[str, str]], route: Route
    ) -> Iterator[tuple[str, str]]:
        """Yield reply deltas while preserving the normal local/auto/cloud rules.

        Auto may fall back to cloud only if the local stream failed before it
        emitted any user-visible text. Once a local answer starts, mixing a
        cloud continuation into the same turn would be misleading, so failures
        after the first delta are surfaced to the client and its REST fallback.
        """
        if route in ("local", "auto"):
            emitted = False
            try:
                for chunk in self._local_stream(messages):
                    emitted = True
                    yield chunk
                return
            except (httpx.HTTPError, KeyError, TypeError, ValueError, RuntimeError, json.JSONDecodeError):
                if route == "local" or emitted:
                    raise RuntimeError(
                        f"local_model_unavailable:{self.settings.ollama_model}"
                    )
        text, provider = self._cloud(messages)
        yield text, provider
