from __future__ import annotations

import time
from typing import Literal

import httpx

from .config import Settings
from .errors import ErrorCode, RouteError, normalize_upstream_error

Route = Literal["local", "auto", "cloud", "mazlatest"]

# The handheld prompt explicitly asks for compact answers. Bounding generation
# prevents a verbose local model from making the user wait for text that cannot
# usefully fit on the Pocket screen. Both local engines honour the same bound.
MAX_OUTPUT_TOKENS = 160


class Models:
    def __init__(self, settings: Settings, client: httpx.Client | None = None) -> None:
        self.settings = settings
        self.client = client or httpx.Client(timeout=90)
        self._last_usage: dict[str, int | float | str] = {}
        self._last_route: dict[str, str | int | bool | None] = {}

    @staticmethod
    def _auth_headers(key: str) -> dict[str, str]:
        return {"Authorization": f"Bearer {key}"} if key else {}

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
            "last_route": dict(self._last_route),
            "cloud": bool(self.settings.cloud_url and self.settings.cloud_model),
            "cloud_credential_configured": bool(self.settings.cloud_key),
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
        last_error: RouteError | None = None
        for model in self._local_models():
            try:
                return self._local_one(model, messages)
            except Exception as error:
                last_error = normalize_upstream_error(
                    error, route="local", model=model, local=True
                )
        if last_error is not None:
            raise RouteError(
                ErrorCode.LOCAL_UNAVAILABLE,
                "local",
                upstream_status=last_error.upstream_status,
                retryable=True,
                model=last_error.model,
            ) from last_error
        raise RouteError(ErrorCode.LOCAL_UNAVAILABLE, "local", retryable=True)

    def _openai_completion(
        self,
        *,
        route: str,
        url: str,
        key: str,
        model: str,
        messages: list[dict[str, str]],
        max_tokens: int | None = None,
    ) -> tuple[str, str]:
        body: dict = {
            "model": model,
            "messages": messages,
            "temperature": 0.2,
            "stream": False,
        }
        if max_tokens is not None:
            body["max_tokens"] = max_tokens
        try:
            response = self.client.post(
                f"{url.rstrip('/')}/chat/completions",
                headers=self._auth_headers(key),
                json=body,
            )
            response.raise_for_status()
            payload = response.json()
            choices = payload["choices"]
            if not isinstance(choices, list) or not choices:
                raise ValueError("missing choices")
            message = choices[0]["message"]
            text = message["content"]
            if not isinstance(text, str) or not text.strip():
                raise ValueError("empty content")
            resolved_model = str(payload.get("model") or model)
            return text.strip(), resolved_model
        except Exception as error:
            raise normalize_upstream_error(error, route=route, model=model) from error

    def _cloud(self, messages: list[dict[str, str]]) -> tuple[str, str]:
        if not self.settings.cloud_key:
            raise RouteError(
                ErrorCode.AUTH_FAILED, "cloud", retryable=False, model=self.settings.cloud_model
            )
        text, resolved_model = self._openai_completion(
            route="cloud",
            url=self.settings.cloud_url,
            key=self.settings.cloud_key,
            model=self.settings.cloud_model,
            messages=messages,
        )
        self._last_usage = {"provider": "cloud", "model": resolved_model}
        return text, "cloud"

    def _mazlatest(self, messages: list[dict[str, str]]) -> tuple[str, str]:
        text, resolved_model = self._openai_completion(
            route="mazlatest",
            url=self.settings.mazlatest_url,
            key=self.settings.mazlatest_key,
            model=self.settings.mazlatest_model,
            messages=messages,
            max_tokens=MAX_OUTPUT_TOKENS,
        )
        provider = f"mazlatest:{self.settings.mazlatest_model}"
        self._last_usage = {"provider": provider, "model": resolved_model}
        return text, provider

    def chat(self, messages: list[dict[str, str]], route: Route) -> tuple[str, str]:
        started = time.perf_counter()
        active_route = route
        try:
            if route == "mazlatest":
                result = self._mazlatest(messages)
            elif route in ("local", "auto"):
                active_route = "local"
                try:
                    result = self._local(messages)
                except RouteError:
                    if route == "local":
                        raise
                    active_route = "cloud"
                    result = self._cloud(messages)
            else:
                result = self._cloud(messages)
            self._last_route = {
                "requested": route,
                "active": active_route,
                "ok": True,
                "latency_ms": round((time.perf_counter() - started) * 1000),
            }
            return result
        except Exception as error:
            normalized = normalize_upstream_error(
                error,
                route=active_route,
                model=(
                    self.settings.mazlatest_model
                    if active_route == "mazlatest"
                    else self.settings.cloud_model if active_route == "cloud" else None
                ),
                local=active_route == "local",
            )
            normalized.requested_route = route
            self._last_route = {
                "requested": route,
                "active": active_route,
                "ok": False,
                "error": normalized.code.value,
                "latency_ms": round((time.perf_counter() - started) * 1000),
            }
            raise normalized from error

    def _models_probe(self, url: str, key: str) -> dict:
        endpoint = f"{url.rstrip('/')}/models"
        try:
            response = self.client.get(endpoint, headers=self._auth_headers(key), timeout=3)
        except httpx.TimeoutException:
            return {"reachable": False, "authenticated": False, "auth_status": "timeout", "models": []}
        except httpx.HTTPError:
            return {"reachable": False, "authenticated": False, "auth_status": "unreachable", "models": []}
        if response.status_code in (401, 403):
            return {"reachable": True, "authenticated": False, "auth_status": "rejected", "models": []}
        if response.status_code == 404:
            return {"reachable": True, "authenticated": False, "auth_status": "endpoint_missing", "models": []}
        if not response.is_success:
            return {"reachable": True, "authenticated": False, "auth_status": "upstream_error", "models": []}
        try:
            payload = response.json()
            items = payload.get("data", payload)
            models = [str(item["id"]) for item in items if isinstance(item, dict) and item.get("id")]
        except (AttributeError, KeyError, TypeError, ValueError):
            return {"reachable": True, "authenticated": False, "auth_status": "invalid_response", "models": []}

        auth_status = "not_required"
        if key:
            try:
                invalid = self.client.get(
                    endpoint,
                    headers={"Authorization": "Bearer maz-diagnostics-invalid"},
                    timeout=3,
                )
                if invalid.status_code in (401, 403):
                    auth_status = "accepted"
            except httpx.HTTPError:
                pass
        return {
            "reachable": True,
            "authenticated": True,
            "auth_status": auth_status,
            "models": models,
        }

    def diagnostics(self, preferred_route: Route) -> dict:
        runtime = self.status()
        maz_probe = self._models_probe(self.settings.mazlatest_url, self.settings.mazlatest_key)
        if (
            self.settings.cloud_url.rstrip("/") == self.settings.mazlatest_url.rstrip("/")
            and self.settings.cloud_key == self.settings.mazlatest_key
        ):
            cloud_probe = maz_probe
        else:
            cloud_probe = self._models_probe(self.settings.cloud_url, self.settings.cloud_key)
        maz_model_available = self.settings.mazlatest_model in maz_probe["models"]
        cloud_model_available = self.settings.cloud_model in cloud_probe["models"]
        cloud_available = bool(cloud_probe["authenticated"] and cloud_model_available)
        local_available = bool(runtime["local"])
        return {
            "preferred_route": preferred_route,
            "active_route": self._last_route.get("active"),
            "last_route": dict(self._last_route),
            "9router": {
                "reachable": maz_probe["reachable"],
                "authenticated": maz_probe["authenticated"],
                "auth_status": maz_probe["auth_status"],
            },
            "mazlatest": {
                "configured_aliases": [self.settings.mazlatest_model],
                "model_available": maz_model_available,
                "available": bool(maz_probe["authenticated"] and maz_model_available),
            },
            "cloud": {
                "model": self.settings.cloud_model,
                "model_available": cloud_model_available,
                "available": cloud_available,
            },
            "local": {
                "engine": runtime["local_engine"],
                "endpoint_available": local_available,
                "model": runtime["local_model"],
                "model_available": runtime["local_model_installed"],
                "available": local_available,
            },
            "auto": {
                "available": local_available or cloud_available,
                "degraded": not local_available and cloud_available,
                "fallback": "cloud" if not local_available and cloud_available else None,
            },
        }
