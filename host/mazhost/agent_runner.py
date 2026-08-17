from __future__ import annotations

import json
import os
import secrets
import shutil
import subprocess
import threading
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Literal

from .authority import AuthorityBroker, AuthorityError
from .config import Settings


Provider = Literal["claude", "codex", "hermes"]


@dataclass
class AgentJob:
    job_id: str
    provider: str
    project: str
    task: str
    state: str = "queued"
    created_at: float = field(default_factory=time.time)
    started_at: float = 0.0
    finished_at: float = 0.0
    exit_code: int | None = None
    output: str = ""
    error: str = ""
    grant_id: str = ""


@dataclass
class CrewJob:
    job_id: str
    project: str
    task: str
    state: str = "queued"
    created_at: float = field(default_factory=time.time)
    started_at: float = 0.0
    finished_at: float = 0.0
    grant_id: str = ""
    plan: dict[str, Any] = field(default_factory=dict)
    runs: list[dict[str, Any]] = field(default_factory=list)
    error: str = ""


class AgentRunError(RuntimeError):
    pass


class AgentRunner:
    """Run installed coding agents only under a signed MAZ capability grant.

    This is deliberately separate from the model itself. A prompt can request
    execution, but the local subprocess is not started until AuthorityBroker
    verifies a non-expired PROJECT FULL/PC FULL/ADMIN grant from the phone.
    """

    def __init__(self, settings: Settings, broker: AuthorityBroker) -> None:
        self.settings = settings
        self.broker = broker
        self._jobs: dict[str, AgentJob] = {}
        self._crew_jobs: dict[str, CrewJob] = {}
        self._lock = threading.RLock()
        self._help_cache: dict[str, str] = {}

    def providers(self) -> dict[str, Any]:
        return {
            name: {"available": bool(shutil.which(name)), "path": shutil.which(name) or ""}
            for name in ("claude", "codex", "hermes")
        }

    def _help(self, exe: str) -> str:
        if exe in self._help_cache:
            return self._help_cache[exe]
        try:
            result = subprocess.run(
                [exe, "--help"], capture_output=True, text=True, errors="replace",
                timeout=8, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
            text = (result.stdout or "") + "\n" + (result.stderr or "")
        except (OSError, subprocess.TimeoutExpired):
            text = ""
        self._help_cache[exe] = text
        return text

    def _argv(self, provider: Provider, prompt: str) -> list[str]:
        exe = shutil.which(provider)
        if not exe:
            raise AgentRunError(f"agent_not_installed:{provider}")
        help_text = self._help(exe)
        if provider == "claude":
            argv = [exe, "-p", prompt, "--output-format", "json", "--max-turns", "20"]
            # Claude's own interactive permission prompt would otherwise hang a
            # non-interactive Crew. This flag is used only after the external
            # MAZ phone broker has already granted PROJECT FULL or broader.
            if "--dangerously-skip-permissions" in help_text:
                argv.append("--dangerously-skip-permissions")
            return argv
        if provider == "codex":
            argv = [exe, "exec"]
            if "--ephemeral" in help_text:
                argv.append("--ephemeral")
            argv.append(prompt)
            return argv
        # Hermes' current one-shot mode is `hermes -z <prompt>`. If the local
        # install is older and does not advertise it, fail rather than guessing.
        if "-z" not in help_text and "--query" not in help_text:
            raise AgentRunError("hermes_noninteractive_mode_unavailable")
        return [exe, "-z", prompt]

    def _verify(self, grant_token: str, project: Path, provider: str):
        try:
            grant = self.broker.verify_grant(
                grant_token, required={"project_full"}, agent=provider
            )
        except AuthorityError as error:
            raise AgentRunError(str(error)) from error
        if grant.scope == "project_full":
            approved = Path(grant.project).expanduser().resolve()
            if approved != project.resolve():
                raise AgentRunError("wrong_approved_project")
        return grant

    def run_sync(
        self,
        *,
        provider: Provider,
        prompt: str,
        project: str,
        grant_token: str,
        timeout_seconds: int | None = None,
    ) -> dict[str, Any]:
        root = Path(project).expanduser().resolve()
        if not root.exists() or not root.is_dir():
            raise AgentRunError("project_not_found")
        grant = self._verify(grant_token, root, provider)
        argv = self._argv(provider, prompt)
        timeout = max(30, min(
            int(timeout_seconds or self.settings.agent_job_timeout_seconds),
            self.settings.agent_job_timeout_seconds,
        ))
        command_id = __import__("hashlib").sha256(
            (provider + "\0" + prompt).encode("utf-8")
        ).hexdigest()
        self.broker.audit_action("agent_start", {
            "grant_id": grant.grant_id,
            "provider": provider,
            "project": str(root),
            "prompt_hash": command_id,
        })
        started = time.time()
        try:
            result = subprocess.run(
                argv,
                cwd=str(root),
                capture_output=True,
                text=True,
                errors="replace",
                timeout=timeout,
                env=os.environ.copy(),
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
        except subprocess.TimeoutExpired as error:
            self.broker.audit_action("agent_timeout", {
                "grant_id": grant.grant_id, "provider": provider, "project": str(root)
            })
            raise AgentRunError("agent_timeout") from error
        except OSError as error:
            raise AgentRunError(f"agent_start_failed:{error}") from error
        output = ((result.stdout or "") + ("\n" + result.stderr if result.stderr else "")).strip()
        output = output[-self.settings.control_max_output_chars :]
        receipt = {
            "ok": result.returncode == 0,
            "provider": provider,
            "project": str(root),
            "exit_code": result.returncode,
            "duration_ms": round((time.time() - started) * 1000),
            "output": output,
            "grant_id": grant.grant_id,
        }
        self.broker.audit_action("agent_end", {
            "grant_id": grant.grant_id,
            "provider": provider,
            "project": str(root),
            "exit_code": result.returncode,
            "duration_ms": receipt["duration_ms"],
        })
        return receipt

    def start(
        self,
        *,
        provider: Provider,
        prompt: str,
        project: str,
        grant_token: str,
    ) -> dict[str, Any]:
        root = Path(project).expanduser().resolve()
        grant = self._verify(grant_token, root, provider)
        job = AgentJob(
            job_id="agent_" + secrets.token_urlsafe(10),
            provider=provider,
            project=str(root),
            task=" ".join(prompt.split())[:300],
            grant_id=grant.grant_id,
        )
        with self._lock:
            self._jobs[job.job_id] = job
        thread = threading.Thread(
            target=self._run_job, args=(job.job_id, prompt, grant_token),
            name=f"maz-{provider}-{job.job_id[-6:]}", daemon=True,
        )
        thread.start()
        return asdict(job)

    def _run_job(self, job_id: str, prompt: str, grant_token: str) -> None:
        with self._lock:
            job = self._jobs[job_id]
            job.state = "running"
            job.started_at = time.time()
        try:
            result = self.run_sync(
                provider=job.provider, prompt=prompt, project=job.project,
                grant_token=grant_token,
            )
            with self._lock:
                job.exit_code = int(result["exit_code"])
                job.output = str(result["output"])
                job.state = "done" if result["ok"] else "failed"
        except AgentRunError as error:
            with self._lock:
                job.state = "failed"
                job.error = str(error)
        finally:
            with self._lock:
                job.finished_at = time.time()

    def job(self, job_id: str) -> dict[str, Any]:
        with self._lock:
            job = self._jobs.get(job_id)
            if not job:
                raise AgentRunError("agent_job_not_found")
            return asdict(job)

    def jobs(self, limit: int = 20) -> list[dict[str, Any]]:
        with self._lock:
            rows = sorted(self._jobs.values(), key=lambda item: item.created_at, reverse=True)
            return [asdict(item) for item in rows[: max(1, min(limit, 100))]]

    # --------------------------------------------------------------- CREW
    def start_crew(
        self,
        *,
        plan: dict[str, Any],
        task: str,
        project: str,
        grant_token: str,
    ) -> dict[str, Any]:
        root = Path(project).expanduser().resolve()
        grant = self._verify(grant_token, root, "crew")
        job = CrewJob(
            job_id="crew_" + secrets.token_urlsafe(10),
            project=str(root),
            task=" ".join(task.split())[:400],
            grant_id=grant.grant_id,
            plan=plan,
        )
        with self._lock:
            self._crew_jobs[job.job_id] = job
        threading.Thread(
            target=self._run_crew,
            args=(job.job_id, grant_token),
            name=f"maz-crew-{job.job_id[-6:]}", daemon=True,
        ).start()
        return asdict(job)

    def _choose_provider(self, preferred: str) -> Provider:
        available = self.providers()
        if preferred in available and available[preferred]["available"]:
            return preferred  # type: ignore[return-value]
        for fallback in ("claude", "codex", "hermes"):
            if available[fallback]["available"]:
                return fallback  # type: ignore[return-value]
        raise AgentRunError("no_supported_agent_installed")

    def _run_crew(self, job_id: str, grant_token: str) -> None:
        with self._lock:
            job = self._crew_jobs[job_id]
            job.state = "running"
            job.started_at = time.time()
            plan = dict(job.plan)
        try:
            packages = plan.get("packages") or []
            order = plan.get("run_order") or [pkg.get("id") for pkg in packages]
            by_id = {str(pkg.get("id")): pkg for pkg in packages if pkg.get("id")}
            completed: list[str] = []
            # Conservative default: execute packages serially. The plan may mark
            # work parallelizable, but serial execution avoids file collisions
            # until separate worktree ownership is proven for each package.
            for package_id in order:
                package = by_id.get(str(package_id))
                if not package:
                    continue
                provider = self._choose_provider(str(package.get("preferred_agent", "")))
                objective = str(package.get("objective") or job.task)
                likely = ", ".join(str(x) for x in (package.get("likely_files") or [])[:20])
                prompt = (
                    f"MAZ CREW PACKAGE: {package.get('role','agent')}\n\n"
                    f"OVERALL TASK\n{job.task}\n\nYOUR PACKAGE\n{objective}\n\n"
                    f"PROJECT\n{job.project}\n\nLIKELY FILES (advisory only)\n{likely or 'inspect first'}\n\n"
                    "Inspect the current checkout and AGENTS/instructions before editing. "
                    "Preserve unrelated work. Do the package completely, run the closest tests, "
                    "and leave the working tree in a reviewable state. Do not publish a release."
                )
                result = self.run_sync(
                    provider=provider, prompt=prompt, project=job.project,
                    grant_token=grant_token,
                )
                receipt = {
                    "package_id": package_id,
                    "role": package.get("role", "agent"),
                    "provider": provider,
                    **result,
                }
                with self._lock:
                    job.runs.append(receipt)
                if not result["ok"]:
                    raise AgentRunError(f"crew_package_failed:{package_id}:{provider}")
                completed.append(str(package_id))
            with self._lock:
                job.state = "done"
        except AgentRunError as error:
            with self._lock:
                job.state = "failed"
                job.error = str(error)
        finally:
            with self._lock:
                job.finished_at = time.time()

    def crew_job(self, job_id: str) -> dict[str, Any]:
        with self._lock:
            job = self._crew_jobs.get(job_id)
            if not job:
                raise AgentRunError("crew_job_not_found")
            return asdict(job)

    def crew_jobs(self, limit: int = 20) -> list[dict[str, Any]]:
        with self._lock:
            rows = sorted(self._crew_jobs.values(), key=lambda item: item.created_at, reverse=True)
            return [asdict(item) for item in rows[: max(1, min(limit, 100))]]
