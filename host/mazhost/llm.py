from __future__ import annotations

from typing import Literal

import httpx

from .config import Settings

Route = Literal["local", "auto", "cloud"]


class Models:
    def __init__(self, settings: Settings, client: httpx.Client | None = None) -> None:
        self.settings = settings
        self.client = client or httpx.Client(timeout=90)

    def _local_models(self) -> list[str]:
        primary = self.settings.ollama_model.strip()
        backup = self.settings.ollama_backup_model.strip()
        policy = self.settings.local_model_policy
        if policy == "primary":
            return [primary] if primary else []
        if policy == "backup":
            return [backup] if backup else []
        result: list[str] = []
        for model in (primary, backup):
            if model and model not in result:
                result.append(model)
        return result

    def status(self) -> dict:
        local = False
        names: set[str] = set()
        try:
            response = self.client.get(f"{self.settings.ollama_url.rstrip('/')}/api/tags", timeout=2)
            local = response.is_success
            if local:
                for item in response.json().get("models", []):
                    name = str(item.get("name") or item.get("model") or "")
                    if name:
                        names.add(name)
        except (httpx.HTTPError, KeyError, TypeError, ValueError):
            pass
        primary = self.settings.ollama_model
        backup = self.settings.ollama_backup_model
        return {
            "local": local,
            "local_policy": self.settings.local_model_policy,
            "local_model": primary,
            "local_model_installed": primary in names,
            "backup_model": backup,
            "backup_model_installed": backup in names,
            "local_chain": self._local_models(),
            "cloud": bool(self.settings.cloud_key),
        }

    def _options(self, model: str) -> dict:
        if model == self.settings.ollama_backup_model:
            return {
                "temperature": 0.30,
                "top_p": 0.90,
                "top_k": 30,
                "repeat_penalty": 1.05,
                "num_ctx": 8192,
            }
        return {
            "temperature": 0.15,
            "top_p": 0.85,
            "repeat_penalty": 1.05,
            "num_ctx": 8192,
        }

    def _local_one(self, model: str, messages: list[dict[str, str]]) -> tuple[str, str]:
        response = self.client.post(
            f"{self.settings.ollama_url.rstrip('/')}/api/chat",
            json={
                "model": model,
                "messages": messages,
                "stream": False,
                "keep_alive": "30m",
                "options": self._options(model),
            },
        )
        response.raise_for_status()
        text = response.json()["message"]["content"].strip()
        if not text:
            raise RuntimeError("local_model_empty_reply")
        return text, f"local:{model}"

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
