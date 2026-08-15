from __future__ import annotations

import threading
import time
import uuid
from typing import Any

from .core import CoreError, MazCore


class CoreJobs:
    """Tiny in-memory job runner so builds/tests never block the Cardputer."""

    def __init__(self, core: MazCore) -> None:
        self.core = core
        self._lock = threading.Lock()
        self._jobs: dict[str, dict[str, Any]] = {}

    def start(self, action: str, project: str) -> dict[str, Any]:
        # Validate project/action before creating a job. `git_status` is cheap
        # enough to run synchronously elsewhere, but treating all actions the
        # same keeps the device UI predictable.
        if action not in {"git_status", "git_fetch", "git_pull_ff", "tests", "build", "open_folder"}:
            raise CoreError("action_not_allowed")
        self.core.project(project)
        job_id = uuid.uuid4().hex[:12]
        job = {
            "id": job_id,
            "action": action,
            "project": project,
            "state": "queued",
            "ok": None,
            "output": "",
            "created": int(time.time()),
            "finished": 0,
        }
        with self._lock:
            self._jobs[job_id] = job
            # Bound memory. Oldest finished jobs are disposable evidence after
            # the caller has already received them.
            if len(self._jobs) > 80:
                old = sorted(self._jobs.values(), key=lambda item: item["created"])
                for item in old[:20]:
                    if item["state"] == "done":
                        self._jobs.pop(item["id"], None)
        threading.Thread(target=self._run, args=(job_id,), daemon=True, name=f"maz-job-{job_id}").start()
        return dict(job)

    def _run(self, job_id: str) -> None:
        with self._lock:
            job = self._jobs[job_id]
            job["state"] = "running"
            action = job["action"]
            project = job["project"]
        try:
            result = self.core.action(action, project)
            ok = bool(result.get("ok"))
            output = str(result.get("output", ""))[-12000:]
            error = "" if ok else output or "action_failed"
        except CoreError as exc:
            ok = False
            output = ""
            error = str(exc)
        with self._lock:
            job = self._jobs[job_id]
            job.update({
                "state": "done",
                "ok": ok,
                "output": output,
                "error": error,
                "finished": int(time.time()),
            })

    def status(self, job_id: str) -> dict[str, Any]:
        with self._lock:
            job = self._jobs.get(job_id)
            if not job:
                raise CoreError("job_not_found")
            return dict(job)

    def recent(self, limit: int = 20) -> list[dict[str, Any]]:
        with self._lock:
            values = sorted(self._jobs.values(), key=lambda item: item["created"], reverse=True)
            return [dict(item) for item in values[: max(1, min(limit, 50))]]
