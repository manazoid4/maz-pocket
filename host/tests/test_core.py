from __future__ import annotations

from pathlib import Path

import pytest

from mazhost.config import Settings
from mazhost.core import CoreError, MazCore
from mazhost.jobs import CoreJobs


def make_core(tmp_path: Path) -> tuple[MazCore, Path]:
    root = tmp_path / "Projects"
    project = root / "demo"
    project.mkdir(parents=True)
    (project / "package.json").write_text('{"name":"demo","scripts":{"build":"echo build"}}', encoding="utf-8")
    (project / "README.md").write_text("MAZ Core knows this factual demo project.\n", encoding="utf-8")
    settings = Settings(project_roots=str(root), core_enabled=True, _env_file=None)
    return MazCore(settings), project


def test_core_discovers_and_reads_only_inside_project(tmp_path: Path):
    core, project = make_core(tmp_path)
    projects = core.projects()
    assert [item["name"] for item in projects] == ["demo"]
    assert core.read_file("demo", "README.md")["content"].startswith("MAZ Core")
    outside = tmp_path / "secret.txt"
    outside.write_text("nope", encoding="utf-8")
    with pytest.raises(CoreError, match="path_outside_project"):
        core.read_file("demo", "../../secret.txt")


def test_secret_files_are_blocked(tmp_path: Path):
    core, project = make_core(tmp_path)
    (project / ".env").write_text("SECRET=1", encoding="utf-8")
    with pytest.raises(CoreError, match="secret_file_blocked"):
        core.read_file("demo", ".env")


def test_search_and_grounding_use_real_project_evidence(tmp_path: Path):
    core, _ = make_core(tmp_path)
    hits = core.search("factual demo", "demo")["hits"]
    assert hits and hits[0]["path"] == "README.md"
    context = core.context_for_prompt("what changed in demo?")
    assert "demo" in context
    assert "MAZ Core factual evidence" in context


def test_dispatch_has_no_generic_shell(tmp_path: Path):
    core, _ = make_core(tmp_path)
    with pytest.raises(CoreError, match="command_not_allowed"):
        core.dispatch({"command": "shell", "cmd": "whoami"})


def test_jobs_are_allowlisted_and_unknown_actions_rejected(tmp_path: Path):
    core, _ = make_core(tmp_path)
    jobs = CoreJobs(core)
    with pytest.raises(CoreError, match="action_not_allowed"):
        jobs.start("shell", "demo")
