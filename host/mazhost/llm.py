from __future__ import annotations

from typing import Literal

import httpx

from .config import Settings

Route = Literal["local", "auto", "cloud"]


class Models:
    """Route MAZ requests through a backend-neutral OpenAI-compatible API.

    llama-swap is the stable local front door. It supervises the tested local
    runtime and swaps models one-at-a-time so a failed primary can fall back to
    a separately proven backup without involving cloud in LOCAL mode.
    """

    def __init__(self, settings: Settings, client: httpx.Client | None = None) -> None:
        self.settings = settings
        self.client = client or httpx.Client(timeout=120)

    def _local_models(self) -> list[str]:
        result: list[str] = []
        for name in (self.settings.local_model, self.settings.local_backup_model):
            name = name.strip()
            if name and name not in result:
                result.append(name)
        return result

    def status(self) -> dict:
        local = False
        available: list[str] = []
        try:
            response = self.client.get(
                f"{self.settings.local_api_url.rstrip('/')}/models", timeout=2
            )
            local = response.is_success
            if local:
                payload = response.json()
                available = [str(item.get("id", "")) for item in payload.get("data", []) if item.get("id")]
        except (httpx.HTTPError, KeyError, TypeError, ValueError):
            try:
                response = self.client.get(self.settings.local_health_url, timeout=2)
                local = response.is_success
            except httpx.HTTPError:
                pass
        return {
            "local": local,
            "local_engine": self.settings.local_engine,
            "local_runtime": self.settings.local_runtime,
            "local_model": self.settings.local_model,
            "local_backup_model": self.settings.local_backup_model,
            "local_chain": self._local_models(),
            "available_models": available[:20],
            "local_context": self.settings.local_context,
            "cloud": bool(self.settings.cloud_key),
        }

    def _local_one(self, model: str, messages: list[dict[str, str]]) -> tuple[str, str]:
        response = self.client.post(
            f"{self.settings.local_api_url.rstrip('/')}/chat/completions",
            json={
                "model": model,
                "messages": messages,
                "stream": False,
                "temperature": self.settings.local_temperature,
                "max_tokens": self.settings.local_max_tokens,
            },
        )
        response.raise_for_status()
        text = response.json()["choices"][0]["message"]["content"].strip()
        if not text:
            raise RuntimeError("local_model_empty_reply")
        return text, f"local:{self.settings.local_engine}:{model}"

    def _local(self, messages: list[dict[str, str]]) -> tuple[str, str]:
        errors: list[str] = []
        for model in self._local_models():
            try:
                return self._local_one(model, messages)
            except (httpx.HTTPError, KeyError, TypeError, ValueError, RuntimeError) as error:
                errors.append(f"{model}:{error}")
        raise RuntimeError("local_models_unavailable:" + "|".join(errors))

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
                    raise RuntimeError("local_models_unavailable")
        return self._cloud(messages)
