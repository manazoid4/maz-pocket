"""MAZ Core: factual local-PC tools for MAZ Pocket and the private web console.

The Core deliberately exposes a small allow-list. It can inspect projects and
run their normal test/build/git maintenance commands, but there is no generic
remote shell endpoint.
"""

from __future__ import annotations

import json
import os
import platform
import re
import shutil
import socket
import subprocess
from pathlib import Path
from typing import Any

import httpx

from .config import Settings


PROJECT_MARKERS = (
    ".git",
    "platformio.ini",
    "package.json",
    "pyproject.toml",
    "Cargo.toml",
    "go.mod",
)
TEXT_EXTENSIONS = {
    ".md", ".txt", ".py", ".cpp", ".c", ".h", ".hpp", ".ino", ".json",
    ".yaml", ".yml", ".toml", ".ini", ".ts", ".tsx", ".js", ".mjs", ".css",
    ".html", ".ps1", ".sh",
}
SKIP_DIRS = {".git", "node_modules", ".venv", "venv", ".pio", "dist", "build", ".next"}


class CoreError(RuntimeError):
    pass


class MazCore:
    def __init__(self, settings: Settings, client: httpx.Client | None = None) -> None:
        self.settings = settings
        self.client = client or httpx.Client(timeout=12)

    # ---------------------------------------------------------------- system
    def status(self) -> dict[str, Any]:
        projects = self.projects(limit=80)
        return {
            "ok": True,
            "version": "0.5.0",
            "hostname": socket.gethostname(),
            "os": platform.platform(),
            "python": platform.python_version(),
            "ollama_model": self.settings.ollama_model,
            "ollama": self._ollama_status(),
            "git": bool(shutil.which("git")),
            "platformio": bool(shutil.which("pio") or shutil.which("platformio")),
            "project_roots": [str(path) for path in self.settings.project_root_paths],
            "project_count": len(projects),
            "obsidian": str(self._obsidian_root() or ""),
            "bridge": self.settings.bridge_enabled,
            "bridge_repo": self.settings.bridge_repo if self.settings.bridge_enabled else "",
            "cardputer_url": self.settings.cardputer_url,
        }

    def _ollama_status(self) -> dict[str, Any]:
        try:
            response = self.client.get(
                f"{self.settings.ollama_url.rstrip('/')}/api/tags", timeout=2
            )
            response.raise_for_status()
            names = [item.get("name", "") for item in response.json().get("models", [])]
            return {
                "online": True,
                "selected_installed": self.settings.ollama_model in names,
                "models": names[:30],
            }
        except (httpx.HTTPError, KeyError, TypeError, ValueError):
            return {"online": False, "selected_installed": False, "models": []}

    # --------------------------------------------------------------- projects
    def _looks_like_project(self, path: Path) -> bool:
        return path.is_dir() and any((path / marker).exists() for marker in PROJECT_MARKERS)

    def _project_paths(self, limit: int = 80) -> list[Path]:
        found: list[Path] = []
        seen: set[str] = set()
        for root in self.settings.project_root_paths:
            if not root.exists() or not root.is_dir():
                continue
            candidates = [root]
            try:
                children = [p for p in root.iterdir() if p.is_dir() and not p.name.startswith(".")]
            except OSError:
                children = []
            candidates.extend(children)
            for child in children[:80]:
                if self._looks_like_project(child):
                    continue
                try:
                    candidates.extend(
                        p for p in child.iterdir()
                        if p.is_dir() and not p.name.startswith(".") and p.name not in SKIP_DIRS
                    )
                except OSError:
                    pass
            for path in candidates:
                if not self._looks_like_project(path):
                    continue
                try:
                    key = str(path.resolve()).lower()
                except OSError:
                    continue
                if key in seen:
                    continue
                seen.add(key)
                found.append(path)
                if len(found) >= limit:
                    return sorted(found, key=lambda p: p.name.lower())
        return sorted(found, key=lambda p: p.name.lower())

    def _git(self, path: Path, *args: str, timeout: int = 12) -> dict[str, Any]:
        if not shutil.which("git"):
            return {"ok": False, "error": "git_not_installed", "output": ""}
        return self._run(["git", *args], path, timeout=timeout)

    def _project_summary(self, path: Path) -> dict[str, Any]:
        branch = ""
        dirty = 0
        last = ""
        if (path / ".git").exists():
            branch_result = self._git(path, "branch", "--show-current")
            if branch_result["ok"]:
                branch = branch_result["output"].strip().splitlines()[0] if branch_result["output"].strip() else "detached"
            status_result = self._git(path, "status", "--porcelain")
            if status_result["ok"]:
                dirty = len([line for line in status_result["output"].splitlines() if line.strip()])
            last_result = self._git(path, "log", "-1", "--pretty=%h %s")
            if last_result["ok"]:
                last = last_result["output"].strip()[:180]
        return {
            "name": path.name,
            "path": str(path),
            "branch": branch,
            "dirty": dirty,
            "last_commit": last,
            "kind": self._project_kind(path),
        }

    def _project_kind(self, path: Path) -> str:
        if (path / "platformio.ini").exists():
            return "platformio"
        if (path / "package.json").exists():
            return "node"
        if (path / "pyproject.toml").exists() or (path / "requirements.txt").exists():
            return "python"
        if (path / "Cargo.toml").exists():
            return "rust"
        if (path / "go.mod").exists():
            return "go"
        return "git"

    def projects(self, limit: int = 60) -> list[dict[str, Any]]:
        return [self._project_summary(path) for path in self._project_paths(limit=limit)]

    def _find_project(self, name: str) -> Path:
        wanted = name.strip().lower()
        if not wanted:
            raise CoreError("project_required")
        for path in self._project_paths():
            if path.name.lower() == wanted:
                return path
        raise CoreError(f"project_not_found:{name}")

    def project(self, name: str) -> dict[str, Any]:
        path = self._find_project(name)
        result = self._project_summary(path)
        if (path / ".git").exists():
            status = self._git(path, "status", "--short", "--branch")
            log = self._git(path, "log", "-5", "--pretty=%h %ad %s", "--date=short")
            result["git_status"] = status["output"][:6000]
            result["recent_commits"] = log["output"][:6000]
        result["files"] = self._top_files(path)
        return result

    def _top_files(self, path: Path) -> list[str]:
        try:
            return sorted(
                p.name for p in path.iterdir()
                if p.is_file() and p.name not in {".env"}
            )[:40]
        except OSError:
            return []

    # ------------------------------------------------------------- safe files
    def _safe_path(self, project: str, relative: str) -> Path:
        root = self._find_project(project).resolve()
        candidate = (root / relative).resolve()
        try:
            candidate.relative_to(root)
        except ValueError as error:
            raise CoreError("path_outside_project") from error
        if not candidate.is_file():
            raise CoreError("file_not_found")
        if candidate.name == ".env" or candidate.suffix.lower() in {".key", ".pem", ".p12", ".pfx"}:
            raise CoreError("secret_file_blocked")
        return candidate

    def read_file(self, project: str, relative: str, max_bytes: int = 32_000) -> dict[str, Any]:
        path = self._safe_path(project, relative)
        if path.stat().st_size > max_bytes:
            raise CoreError("file_too_large")
        data = path.read_text(encoding="utf-8", errors="replace")
        return {"project": project, "path": relative, "content": data[:max_bytes]}

    def _obsidian_root(self) -> Path | None:
        if not self.settings.obsidian_root:
            return None
        path = Path(self.settings.obsidian_root).expanduser()
        return path.resolve() if path.exists() and path.is_dir() else None

    def search(self, query: str, project: str = "", limit: int = 30) -> dict[str, Any]:
        needle = query.strip().lower()
        if len(needle) < 2:
            raise CoreError("query_too_short")
        roots: list[tuple[str, Path]] = []
        if project:
            roots.append((project, self._find_project(project)))
        else:
            roots.extend((path.name, path) for path in self._project_paths(limit=40))
            obsidian = self._obsidian_root()
            if obsidian:
                roots.append(("Obsidian", obsidian))

        hits: list[dict[str, Any]] = []
        for label, root in roots:
            for path in self._iter_text_files(root, max_files=700):
                try:
                    if path.stat().st_size > 512_000:
                        continue
                    text = path.read_text(encoding="utf-8", errors="ignore")
                except OSError:
                    continue
                low = text.lower()
                pos = low.find(needle)
                if pos < 0:
                    continue
                start = max(0, pos - 120)
                end = min(len(text), pos + len(needle) + 220)
                try:
                    rel = str(path.relative_to(root))
                except ValueError:
                    rel = path.name
                hits.append({
                    "project": label,
                    "path": rel,
                    "snippet": " ".join(text[start:end].split())[:360],
                })
                if len(hits) >= limit:
                    return {"query": query, "hits": hits}
        return {"query": query, "hits": hits}

    def _iter_text_files(self, root: Path, max_files: int):
        count = 0
        for base, dirs, files in os.walk(root):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith(".")]
            for filename in files:
                if filename == ".env" or Path(filename).suffix.lower() not in TEXT_EXTENSIONS:
                    continue
                yield Path(base) / filename
                count += 1
                if count >= max_files:
                    return

    # ---------------------------------------------------------- safe actions
    def action(self, action: str, project: str = "") -> dict[str, Any]:
        allowed = {
            "git_status", "git_fetch", "git_pull_ff", "tests", "build", "open_folder"
        }
        if action not in allowed:
            raise CoreError("action_not_allowed")
        path = self._find_project(project)
        if action == "git_status":
            return {"action": action, "project": project, **self._git(path, "status", "--short", "--branch")}
        if action == "git_fetch":
            return {"action": action, "project": project, **self._git(path, "fetch", "--prune", timeout=45)}
        if action == "git_pull_ff":
            return {"action": action, "project": project, **self._git(path, "pull", "--ff-only", timeout=45)}
        if action == "tests":
            cmd = self._test_command(path)
            if not cmd:
                raise CoreError("no_known_test_command")
            return {"action": action, "project": project, **self._run(cmd, path, timeout=180)}
        if action == "build":
            cmd = self._build_command(path)
            if not cmd:
                raise CoreError("no_known_build_command")
            return {"action": action, "project": project, **self._run(cmd, path, timeout=300)}
        if os.name != "nt":
            raise CoreError("open_folder_windows_only")
        os.startfile(path)  # type: ignore[attr-defined]
        return {"ok": True, "action": action, "project": project, "output": str(path)}

    def _test_command(self, path: Path) -> list[str] | None:
        if (path / "tests").exists() and ((path / "pyproject.toml").exists() or (path / "requirements.txt").exists()):
            return [shutil.which("python") or "python", "-m", "pytest", "-q"]
        if (path / "package.json").exists() and shutil.which("npm"):
            return ["npm", "test"]
        if (path / "Cargo.toml").exists() and shutil.which("cargo"):
            return ["cargo", "test"]
        if (path / "go.mod").exists() and shutil.which("go"):
            return ["go", "test", "./..."]
        return None

    def _build_command(self, path: Path) -> list[str] | None:
        if (path / "platformio.ini").exists():
            if shutil.which("pio"):
                return ["pio", "run"]
            return [shutil.which("python") or "python", "-m", "platformio", "run"]
        if (path / "package.json").exists() and shutil.which("npm"):
            return ["npm", "run", "build"]
        if (path / "Cargo.toml").exists() and shutil.which("cargo"):
            return ["cargo", "build"]
        if (path / "go.mod").exists() and shutil.which("go"):
            return ["go", "build", "./..."]
        return None

    def _run(self, command: list[str], cwd: Path, timeout: int) -> dict[str, Any]:
        try:
            completed = subprocess.run(
                command,
                cwd=cwd,
                capture_output=True,
                text=True,
                timeout=timeout,
                shell=False,
                encoding="utf-8",
                errors="replace",
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            return {"ok": False, "returncode": -1, "output": str(error)[:12000]}
        output = ((completed.stdout or "") + (completed.stderr or ""))[-12000:]
        return {"ok": completed.returncode == 0, "returncode": completed.returncode, "output": output}

    # ----------------------------------------------------------- Cardputer LAN
    def _cardputer_headers(self) -> dict[str, str]:
        return {"X-MAZ-Token": self.settings.token}

    def cardputer_status(self) -> dict[str, Any]:
        try:
            response = self.client.get(
                f"{self.settings.cardputer_url.rstrip('/')}/api/status",
                headers=self._cardputer_headers(),
                timeout=3,
            )
            response.raise_for_status()
            return response.json()
        except (httpx.HTTPError, ValueError) as error:
            return {"ok": False, "error": f"cardputer_unreachable:{error}"}

    def cardputer_screen(self) -> bytes:
        try:
            response = self.client.get(
                f"{self.settings.cardputer_url.rstrip('/')}/api/screen",
                headers=self._cardputer_headers(),
                timeout=4,
            )
            response.raise_for_status()
        except httpx.HTTPError as error:
            raise CoreError(f"cardputer_unreachable:{error}") from error
        if len(response.content) != 240 * 135 * 2:
            raise CoreError("invalid_cardputer_frame")
        return response.content

    # --------------------------------------------------------------- grounding
    def context_for_prompt(self, text: str) -> str:
        if not self.settings.core_enabled:
            return ""
        lower = text.lower()
        projects = self.projects(limit=40)
        matched = [p for p in projects if p["name"].lower() in lower]
        evidence: dict[str, Any] = {
            "pc": {
                "hostname": socket.gethostname(),
                "ollama_model": self.settings.ollama_model,
            },
            "projects": matched[:3] if matched else [
                {"name": p["name"], "branch": p["branch"], "dirty": p["dirty"]}
                for p in projects[:15]
            ],
        }
        if matched:
            detailed = []
            for item in matched[:2]:
                try:
                    detailed.append(self.project(item["name"]))
                except CoreError:
                    pass
            evidence["matched_project_evidence"] = detailed
        if any(word in lower for word in ("cardputer", "pocket", "device", "wifi")):
            evidence["cardputer"] = self.cardputer_status()
        if any(word in lower for word in ("note", "memory", "obsidian", "remember")):
            try:
                terms = [w for w in re.findall(r"[a-zA-Z0-9_-]{4,}", text) if w.lower() not in {"what", "with", "from", "that", "this"}]
                if terms:
                    evidence["memory_hits"] = self.search(terms[-1], limit=6)["hits"]
            except CoreError:
                pass
        encoded = json.dumps(evidence, ensure_ascii=False, default=str)
        return "\nMAZ Core factual evidence (use this, do not invent beyond it):\n" + encoded[:7000]

    # ------------------------------------------------------------- bridge API
    def dispatch(self, command: dict[str, Any]) -> dict[str, Any]:
        kind = str(command.get("command", "")).strip()
        if kind == "status":
            return self.status()
        if kind == "projects":
            return {"ok": True, "projects": self.projects()}
        if kind == "project":
            return {"ok": True, "project": self.project(str(command.get("project", "")))}
        if kind == "search":
            return self.search(str(command.get("query", "")), str(command.get("project", "")))
        if kind == "read":
            return {"ok": True, **self.read_file(str(command.get("project", "")), str(command.get("path", "")))}
        if kind == "run":
            return self.action(str(command.get("action", "")), str(command.get("project", "")))
        if kind == "cardputer":
            return self.cardputer_status()
        raise CoreError("command_not_allowed")
