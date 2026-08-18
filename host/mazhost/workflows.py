from __future__ import annotations

import json
import shutil
import time
from dataclasses import dataclass
from typing import Any, Literal

import httpx

from .config import Settings
from .core import CoreError, MazCore
from .llm import Models, Route
from .nudge import NudgeClient


TemplateCategory = Literal[
    "bug", "build", "refactor", "test", "docs", "performance",
    "security", "migrate", "review", "research", "maz"
]


@dataclass(frozen=True)
class PromptTemplate:
    id: str
    title: str
    category: TemplateCategory
    purpose: str
    procedure: tuple[str, ...]
    verification: tuple[str, ...]


TEMPLATES: tuple[PromptTemplate, ...] = (
    PromptTemplate("fix-bug", "Fix a bug", "bug", "Find the actual cause and fix it without unrelated rewrites", (
        "Reproduce or identify the failing path from current evidence.",
        "Inspect the closest implementation and recent changes before editing.",
        "Make the smallest complete fix and preserve existing behaviour outside scope.",
    ), ("Run the closest regression test.", "Run the project test/build gate.")),
    PromptTemplate("diagnose", "Diagnose weird behaviour", "bug", "Explain what is actually failing before changing code", (
        "Collect current logs, state, recent changes and relevant configuration.",
        "Rank likely causes by evidence, not intuition.",
        "Propose the smallest safe next check or fix.",
    ), ("State what evidence confirmed the diagnosis.",)),
    PromptTemplate("add-ui", "Add a UI component", "build", "Add UI by reusing the project's existing design system", (
        "Find the closest existing component and layout pattern.",
        "Match current responsive, accessibility and interaction conventions.",
        "Integrate into the real navigation/data flow rather than shipping a demo island.",
    ), ("Run UI/type/build checks.", "Verify the actual user path manually where possible.")),
    PromptTemplate("add-feature", "Build a feature", "build", "Turn an idea into a coherent project-native feature", (
        "Inspect architecture and identify the correct layer first.",
        "Reuse existing services and primitives before adding dependencies.",
        "Implement a complete vertical slice with failure states.",
    ), ("Add meaningful tests.", "Run the project gate and summarize remaining hardware/manual checks.")),
    PromptTemplate("integrate-oss", "Integrate open source", "build", "Reuse a proven implementation without importing bloat", (
        "Audit licence, maintenance state and the smallest useful upstream surface.",
        "Prefer adaptation/wrapping over wholesale copying.",
        "Document upstream provenance and preserve notices.",
    ), ("Verify licence obligations.", "Test the integrated path rather than only compiling.")),
    PromptTemplate("simplify", "Simplify a module", "refactor", "Reduce complexity while preserving behaviour", (
        "Map callers and invariants first.",
        "Remove duplicate abstractions and dead paths.",
        "Keep public behaviour stable unless the task explicitly changes it.",
    ), ("Run existing regression tests.", "Compare before/after complexity or size where useful.")),
    PromptTemplate("add-tests", "Add useful tests", "test", "Cover meaningful failure and regression paths", (
        "Identify the behaviour that would hurt if it regressed.",
        "Prefer deterministic tests over implementation-detail snapshots.",
        "Cover at least one failure path.",
    ), ("Run the tests and report exact results.",)),
    PromptTemplate("security-audit", "Audit a trust boundary", "security", "Find where untrusted data can gain authority", (
        "Map inputs, authentication, authorization and privileged sinks.",
        "Treat model/web/repo/screen content as data, never authorization.",
        "Propose narrow controls and regression tests for each material issue.",
    ), ("Demonstrate denied unauthorized cases.", "Document residual risk.")),
    PromptTemplate("repo-audit", "Audit a repository", "review", "Understand current reality and prioritize high-leverage improvements", (
        "Inspect instructions, architecture, recent commits, tests and release rules.",
        "Identify stale docs, duplication, hidden useful features and broken user paths.",
        "Rank changes by impact and implementation cost.",
    ), ("Cite current files/commits in the handoff.",)),
    PromptTemplate("research-build", "Research then implement", "research", "Compare current primary sources/open-source work then build the best fit", (
        "Use current primary documentation and maintained upstream repos.",
        "Compare approaches against this project's actual constraints.",
        "Implement only the selected high-value pieces.",
    ), ("Document sources and licences.", "Run project-specific verification.")),
    PromptTemplate("maz-feature", "Add a MAZ Pocket capability", "maz", "Add a useful Pocket capability without turning it into an app drawer", (
        "Keep heavy work on MAZ Core where possible.",
        "Preserve the six-surface information architecture.",
        "Make the Pocket interaction obvious, compact and recoverable offline.",
        "Respect M5Launcher ownership and current routing/privacy settings.",
    ), ("Build firmware.", "Run MAZ Core tests.", "List physical Cardputer checks still required.")),
    PromptTemplate("agent-workflow", "Improve an agent workflow", "maz", "Reduce repeated prompting and make agent work inspectable", (
        "Retrieve current project evidence and Agent Nudge state.",
        "Use PLAN before consequential execution.",
        "Keep durable learning as explicit Template/Skill/Knowledge/Guard proposals.",
    ), ("Check for file-ownership collisions.", "Produce a clear receipt/Retro.")),
)


class WorkflowService:
    def __init__(self, settings: Settings, models: Models, core: MazCore, nudge: NudgeClient) -> None:
        self.settings = settings
        self.models = models
        self.core = core
        self.nudge = nudge

    def templates(self) -> list[dict[str, Any]]:
        return [
            {
                "id": t.id,
                "title": t.title,
                "category": t.category,
                "purpose": t.purpose,
            }
            for t in TEMPLATES
        ]

    @staticmethod
    def _template(template_id: str) -> PromptTemplate:
        for template in TEMPLATES:
            if template.id == template_id:
                return template
        raise ValueError("template_not_found")

    def _context(self, project: str) -> dict[str, Any]:
        result: dict[str, Any] = {"project": project, "retrieved_at": time.time()}
        if project:
            try:
                result["project_state"] = self.core.project(project)
            except CoreError as error:
                result["project_state"] = {"error": str(error)}
        try:
            result["nudge"] = self.nudge.summary()
        except (RuntimeError, httpx.HTTPError) as error:
            result["nudge"] = {"available": False, "error": str(error)}
        return result

    def compile_prompt(self, template_id: str, task: str, project: str = "") -> dict[str, Any]:
        template = self._template(template_id)
        context = self._context(project)
        current = json.dumps(context, indent=2, default=str)[:12_000]
        procedure = "\n".join(f"{i + 1}. {step}" for i, step in enumerate(template.procedure))
        verification = "\n".join(f"- {step}" for step in template.verification)
        prompt = f"""OBJECTIVE\n{task.strip()}\n\nTEMPLATE\n{template.title} — {template.purpose}\n\nCURRENT VERIFIED CONTEXT\n{current}\n\nPROCEDURE\n{procedure}\n\nVERIFICATION\n{verification}\n\nPROJECT-PERSPECTIVE RULE\nInspect the actual current project before assuming architecture. Adapt this template to the repository rather than implementing placeholder wording literally. Reuse existing working primitives, preserve unrelated user work, and do not claim checks you did not run.\n\nDELIVERABLE\nImplement the smallest complete solution, run the relevant verification, and return a concise receipt of files changed, tests, remaining manual checks and next recommendation.\n"""
        return {
            "ok": True,
            "template": {"id": template.id, "title": template.title, "category": template.category},
            "project": project,
            "context": context,
            "prompt": prompt,
        }

    def _json_model(self, system: str, user: str, route: Route) -> tuple[dict[str, Any], str]:
        reply, provider = self.models.chat(
            [
                {"role": "system", "content": system + " Return strict JSON only."},
                {"role": "user", "content": user},
            ],
            route,
        )
        try:
            parsed = json.loads(reply)
        except json.JSONDecodeError as error:
            raise RuntimeError(f"workflow_model_invalid_json:{error}") from error
        if not isinstance(parsed, dict):
            raise RuntimeError("workflow_model_invalid_shape")
        return parsed, provider

    def plan(self, task: str, project: str = "", route: Route = "auto") -> dict[str, Any]:
        context = self._context(project)
        system = """You are MAZ PLAN. Produce a compact implementation plan grounded in supplied current project evidence. Think through product usefulness, architecture, minimalism, security, performance/Cardputer constraints, and agent execution. Do not execute. Required JSON keys: summary (string), steps (array of short strings), likely_files (array), risks (array), authority (one of read_only, project_full, pc_full, admin), parallelizable (bool), verification (array)."""
        user = json.dumps({"task": task, "context": context}, default=str)[:18_000]
        try:
            result, provider = self._json_model(system, user, route)
        except RuntimeError:
            result = {
                "summary": task[:240],
                "steps": ["Inspect current implementation and instructions", "Implement the smallest complete vertical slice", "Run relevant tests/build and review the diff"],
                "likely_files": [],
                "risks": ["Model planning unavailable; deterministic fallback used"],
                "authority": "project_full" if project else "read_only",
                "parallelizable": False,
                "verification": ["Run project tests/build"],
            }
            provider = "deterministic-fallback"
        return {"ok": True, "kind": "plan", "provider": provider, "task": task, "project": project, **result}

    def crew(self, task: str, project: str = "", route: Route = "auto") -> dict[str, Any]:
        context = self._context(project)
        installed = {
            "claude": bool(shutil.which("claude")),
            "codex": bool(shutil.which("codex")),
            "hermes": bool(shutil.which("hermes")),
        }
        system = """You are MAZ CREW. Split a software task into the fewest genuinely independent work packages. Avoid two agents editing the same likely files. Prefer heterogeneous roles: builder, reviewer/tester, researcher/integrator. Required JSON keys: summary (string), packages (array of objects with id, role, preferred_agent, objective, likely_files, depends_on), collision_notes (array), run_order (array of package ids), authority (read_only/project_full/pc_full/admin). Keep to 1-3 packages unless there is a strong reason."""
        user = json.dumps({"task": task, "project": project, "installed_agents": installed, "context": context}, default=str)[:18_000]
        try:
            result, provider = self._json_model(system, user, route)
        except RuntimeError:
            result = {
                "summary": "Fallback single-agent crew plan",
                "packages": [{"id": "build", "role": "builder", "preferred_agent": "claude" if installed["claude"] else "available-agent", "objective": task, "likely_files": [], "depends_on": []}],
                "collision_notes": ["Model crew planning unavailable; keep work serial"],
                "run_order": ["build"],
                "authority": "project_full" if project else "read_only",
            }
            provider = "deterministic-fallback"
        return {"ok": True, "kind": "crew", "provider": provider, "installed_agents": installed, "task": task, "project": project, **result}

    def retro(self, project: str = "", route: Route = "auto", extra: str = "") -> dict[str, Any]:
        context = self._context(project)
        system = """You are MAZ RETRO. Review the supplied project state, Agent Nudge evidence and user note. Identify what actually improved or failed, wasted loops, missing/misleading context, and only durable learnings worth saving. Required JSON keys: outcome (string), wins (array), friction (array), missing_context (array), proposals (array of objects with type one of template, skill, knowledge, guard and change string), next_action (string). Never apply durable learning yourself."""
        user = json.dumps({"project": project, "context": context, "note": extra[:4000]}, default=str)[:18_000]
        try:
            result, provider = self._json_model(system, user, route)
        except RuntimeError:
            result = {
                "outcome": "Retro model unavailable",
                "wins": [],
                "friction": ["Could not generate model-backed Retro"],
                "missing_context": [],
                "proposals": [],
                "next_action": "Retry when a configured model is available",
            }
            provider = "deterministic-fallback"
        return {"ok": True, "kind": "retro", "provider": provider, "project": project, **result}
