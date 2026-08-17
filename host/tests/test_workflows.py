from __future__ import annotations

from mazhost.config import Settings
from mazhost.workflows import WorkflowService


class FakeModels:
    def chat(self, messages, route):
        raise RuntimeError("offline")


class FakeCore:
    def project(self, name):
        return {"name": name, "path": f"C:/Projects/{name}", "branch": "main", "dirty": 0}


class FakeNudge:
    def summary(self):
        return {"state": "CLEAR", "agents": []}


def service():
    return WorkflowService(Settings(_env_file=None, token="configured"), FakeModels(), FakeCore(), FakeNudge())


def test_prompt_deck_has_clear_templates():
    rows = service().templates()
    ids = {row["id"] for row in rows}
    assert "add-ui" in ids
    assert "maz-feature" in ids
    assert "repo-audit" in ids


def test_compile_prompt_includes_project_perspective_and_current_context():
    result = service().compile_prompt("add-ui", "Add a status card", "maz-pocket")
    assert result["ok"] is True
    assert "PROJECT-PERSPECTIVE RULE" in result["prompt"]
    assert "maz-pocket" in result["prompt"]
    assert "Add a status card" in result["prompt"]


def test_plan_falls_back_cleanly_when_model_is_offline():
    result = service().plan("Fix the build", "maz-pocket", "local")
    assert result["ok"] is True
    assert result["kind"] == "plan"
    assert result["provider"] == "deterministic-fallback"
    assert result["steps"]


def test_crew_exposes_packages_even_when_model_is_offline():
    result = service().crew("Improve recorder", "maz-pocket", "local")
    assert result["ok"] is True
    assert result["kind"] == "crew"
    assert result["packages"]
    assert "installed_agents" in result


def test_retro_never_auto_applies_learning():
    result = service().retro("maz-pocket", "local", "Tests failed twice")
    assert result["ok"] is True
    assert result["kind"] == "retro"
    assert isinstance(result["proposals"], list)
