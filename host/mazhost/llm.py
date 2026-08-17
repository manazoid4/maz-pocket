from __future__ import annotations

from typing import Literal

import httpx

from .config import Settings

Route = Literal["local", "auto", "cloud"]

# The handheld prompt explicitly asks for compact answers. Bounding generation
# prevents a verbose local model from making the user wait for text that cannot
# usefully fit on the Pocket screen. Both local engines honour the same bound.
MAX_OUTPUT_TOKENS = 160


class Models:
    def __init__(self, settings: Settings, client: httpx.Client | None = None) -> None:
        self.settings = settings
        self.client = client or httpx.Client(timeout=90)
        self._last_usage: dict[str, int | float | str] = {}

    def _local_models(self) -> list[str]:
        if self.settings.local_engine == "llamacpp":
            primary = self.settings.llamacpp_model.strip()
            backup = self.settings.llamacpp_backup_model.strip()
        else:
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

    def _llamacpp_url(self, model: str) -> str:
        backup = self.settings.llamacpp_backup_model.strip()
        backup_url = self.settings.llamacpp_backup_url.strip()
        if backup and backup_url and model == backup:
            return backup_url.rstrip("/")
        return self.settings.llamacpp_url.rstrip("/")

    def _ollama_probe(self) -> tuple[bool, set[str]]:
        names: set[str] = set()
        try:
            response = self.client.get(f"{self.settings.ollama_url.rstrip('/')}/api/tags", timeout=2)
        except (httpx.HTTPError, KeyError, TypeError, ValueError):
            return False, names
        if not response.is_success:
            return False, names
        try:
            for item in response.json().get("models", []):
                name = str(item.get("name") or item.get("model") or "")
                if name:
                    names.add(name)
        except (KeyError, TypeError, ValueError):
            pass
        return True, names

    def _llamacpp_probe(self) -> tuple[bool, set[str]]:
        # llama-server holds exactly the model it was started with, so a healthy
        # server is the only "installed" signal that means anything here. The
        # configured names are labels the request carries, not selectors.
        online = False
        for url in {self._llamacpp_url(model) for model in self._local_models()}:
            try:
                response = self.client.get(f"{url}/health", timeout=2)
            except (httpx.HTTPError, KeyError, TypeError, ValueError):
                continue
            if response.is_success:
                online = True
        return online, set(self._local_models()) if online else set()

    def status(self) -> dict:
        engine = self.settings.local_engine
        if engine == "llamacpp":
            local, names = self._llamacpp_probe()
            primary = self.settings.llamacpp_model
            backup = self.settings.llamacpp_backup_model
            endpoint = self.settings.llamacpp_url
        else:
            local, names = self._ollama_probe()
            primary = self.settings.ollama_model
            backup = self.settings.ollama_backup_model
            endpoint = self.settings.ollama_url
        return {
            "local": local,
            "local_engine": engine,
            "local_endpoint": endpoint,
            "local_policy": self.settings.local_model_policy,
            "local_model": primary,
            "local_model_installed": primary in names,
            "backup_model": backup,
            "backup_model_installed": bool(backup) and backup in names,
            "local_chain": self._local_models(),
            "ai_profile": self.settings.ai_profile,
            "keep_alive": self._keep_alive(),
            "last_usage": dict(self._last_usage),
            "cloud": bool(self.settings.cloud_key),
        }

    def _keep_alive(self) -> str | int:
        if self.settings.ai_profile == "save":
            return 0
        if self.settings.ai_profile == "fast":
            return "60m"
        # SMART now biases toward interactive latency. Five minutes caused a
        # cold model reload during ordinary gaps between Pocket conversations.
        return "15m"

    def _context_size(self, messages: list[dict[str, str]]) -> int:
        # Character count is intentionally cheap; tokenizing on the laptop just
        # to choose a context window would add more work than this decision is
        # worth. Ordinary Pocket turns stay at 4K; grounded/project-heavy turns
        # get headroom only when their payload actually needs it.
        chars = sum(len(str(message.get("content", ""))) for message in messages)
        if self.settings.ai_profile == "save":
            return 6144 if chars > 16_000 else 4096
        if chars > 20_000:
            return 8192
        if chars > 10_000:
            return 6144
        return 4096

    def _options(self, model: str, messages: list[dict[str, str]]) -> dict:
        options = {
            "temperature": 0.15,
            "top_p": 0.85,
            "repeat_penalty": 1.05,
            "num_ctx": self._context_size(messages),
            "num_predict": MAX_OUTPUT_TOKENS,
        }
        if model == self.settings.ollama_backup_model:
            options.update({"temperature": 0.30, "top_p": 0.90, "top_k": 30})
        return options

    def _local_one(self, model: str, messages: list[dict[str, str]]) -> tuple[str, str]:
        if self.settings.local_engine == "llamacpp":
            return self._llamacpp_one(model, messages)
        return self._ollama_one(model, messages)

    def _llamacpp_one(self, model: str, messages: list[dict[str, str]]) -> tuple[str, str]:
        # llama-server speaks the OpenAI shape and accepts llama.cpp sampling
        # names alongside it. Context size is fixed by the server's -c flag, so
        # the adaptive num_ctx used for Ollama has nothing to set here.
        url = self._llamacpp_url(model)
        response = self.client.post(
            f"{url}/v1/chat/completions",
            json={
                "model": model,
                "messages": messages,
                "stream": False,
                "temperature": 0.15,
                "top_p": 0.85,
                "repeat_penalty": 1.05,
                "max_tokens": MAX_OUTPUT_TOKENS,
                # Reusing the cached prefix is what keeps a second Pocket turn
                # from re-evaluating the whole grounded prompt.
                "cache_prompt": True,
                # Reasoning models put their thinking in `reasoning_content`
                # and leave `content` empty until they finish. Against a
                # 160-token bound that means the whole budget is spent thinking
                # and the Pocket gets nothing back, so thinking is switched off
                # at the template. Templates without the flag ignore it.
                "chat_template_kwargs": {"enable_thinking": False},
            },
        )
        response.raise_for_status()
        payload = response.json()
        message = payload["choices"][0]["message"]
        text = (message.get("content") or "").strip()
        if not text:
            # A reasoning model that ran out of budget mid-thought answers with
            # an empty content and a full reasoning_content. Name that case so
            # the chain error says why rather than just "unavailable".
            if (message.get("reasoning_content") or "").strip():
                raise RuntimeError("local_model_spent_budget_thinking")
            raise RuntimeError("local_model_empty_reply")
        usage = payload.get("usage") or {}
        timings = payload.get("timings") or {}
        self._last_usage = {
            "provider": f"local:{model}",
            "engine": "llamacpp",
            "prompt_tokens": int(usage.get("prompt_tokens") or 0),
            "output_tokens": int(usage.get("completion_tokens") or 0),
            "total_ms": round(
                float(timings.get("prompt_ms") or 0) + float(timings.get("predicted_ms") or 0)
            ),
        }
        return text, f"local:{model}"

    def _ollama_one(self, model: str, messages: list[dict[str, str]]) -> tuple[str, str]:
        options = self._options(model, messages)
        response = self.client.post(
            f"{self.settings.ollama_url.rstrip('/')}/api/chat",
            json={
                "model": model,
                "messages": messages,
                "stream": False,
                "keep_alive": self._keep_alive(),
                "options": options,
            },
        )
        response.raise_for_status()
        payload = response.json()
        text = payload["message"]["content"].strip()
        if not text:
            raise RuntimeError("local_model_empty_reply")
        self._last_usage = {
            "provider": f"local:{model}",
            "engine": "ollama",
            "num_ctx": int(options["num_ctx"]),
            "load_ms": round(int(payload.get("load_duration") or 0) / 1_000_000),
            "prompt_tokens": int(payload.get("prompt_eval_count") or 0),
            "output_tokens": int(payload.get("eval_count") or 0),
            "total_ms": round(int(payload.get("total_duration") or 0) / 1_000_000),
        }
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
                # OpenAI-compatible gateways do not agree on the default. A
                # local 9router endpoint streams unless told otherwise, which
                # returns concatenated SSE chunks and breaks the single-object
                # parse below. Ask for one complete response explicitly.
                "stream": False,
            },
        )
        response.raise_for_status()
        text = response.json()["choices"][0]["message"]["content"].strip()
        if not text:
            raise RuntimeError("cloud_model_empty_reply")
        self._last_usage = {"provider": "cloud"}
        return text, "cloud"

    def chat(self, messages: list[dict[str, str]], route: Route) -> tuple[str, str]:
        if route in ("local", "auto"):
            try:
                return self._local(messages)
            except (httpx.HTTPError, KeyError, TypeError, ValueError, RuntimeError):
                if route == "local":
                    raise RuntimeError("local_models_unavailable")
        return self._cloud(messages)
