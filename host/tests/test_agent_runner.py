from __future__ import annotations

from pathlib import Path

import pytest

from mazhost.agent_runner import AgentRunError, AgentRunner
from mazhost.authority import AuthorityBroker
from mazhost.config import Settings


def make(tmp_path):
    cfg = Settings(
        _env_file=None,
        token="configured-token-123",
        control_dir=str(tmp_path / "control"),
        project_roots=str(tmp_path),
        agent_job_timeout_seconds=120,
    )
    return cfg, AuthorityBroker(cfg)


def test_agent_run_needs_signed_project_grant(tmp_path, monkeypatch):
    project = tmp_path / "repo"
    project.mkdir()
    cfg, broker = make(tmp_path)
    runner = AgentRunner(cfg, broker)
    with pytest.raises(AgentRunError):
        runner.run_sync(
            provider="claude",
            prompt="do nothing",
            project=str(project),
            grant_token="not-a-grant",
        )


def test_claude_never_bypasses_provider_permissions(tmp_path, monkeypatch):
    cfg, broker = make(tmp_path)
    runner = AgentRunner(cfg, broker)
    monkeypatch.setattr("mazhost.agent_runner.shutil.which", lambda name: "C:/bin/claude.exe" if name == "claude" else None)
    runner._help_cache["C:/bin/claude.exe"] = "--dangerously-skip-permissions --output-format --max-turns"
    argv = runner._argv("claude", "test")
    assert "--dangerously-skip-permissions" not in argv
    assert argv[1:3] == ["-p", "test"]


def test_codex_exec_is_noninteractive(tmp_path, monkeypatch):
    cfg, broker = make(tmp_path)
    runner = AgentRunner(cfg, broker)
    monkeypatch.setattr("mazhost.agent_runner.shutil.which", lambda name: "C:/bin/codex.exe" if name == "codex" else None)
    runner._help_cache["C:/bin/codex.exe"] = "codex exec --ephemeral"
    argv = runner._argv("codex", "test")
    assert argv[1] == "exec"
    assert "--ephemeral" in argv


def test_hermes_old_cli_fails_closed(tmp_path, monkeypatch):
    cfg, broker = make(tmp_path)
    runner = AgentRunner(cfg, broker)
    monkeypatch.setattr("mazhost.agent_runner.shutil.which", lambda name: "C:/bin/hermes.exe" if name == "hermes" else None)
    runner._help_cache["C:/bin/hermes.exe"] = "old help without query mode"
    with pytest.raises(AgentRunError, match="noninteractive"):
        runner._argv("hermes", "test")


def test_project_scoped_agent_does_not_treat_cwd_as_sandbox(tmp_path, monkeypatch):
    project = tmp_path / "repo"
    project.mkdir()
    cfg, broker = make(tmp_path)
    runner = AgentRunner(cfg, broker)
    approval = broker.approve(broker.request(
        task="edit source", agent="claude", scope="project_full", project=str(project),
    )["request_id"])
    with pytest.raises(AgentRunError, match="project_scope_requires_os_sandbox"):
        runner.run_sync(
            provider="claude", prompt="edit readme", project=str(project),
            grant_token=approval["grant_token"],
        )
    with pytest.raises(AgentRunError, match="project_scope_requires_os_sandbox"):
        runner.start(
            provider="claude", prompt="edit readme", project=str(project),
            grant_token=approval["grant_token"],
        )


def test_explicit_pc_full_grant_still_validates_for_agent(tmp_path):
    project = tmp_path / "repo"
    project.mkdir()
    cfg, broker = make(tmp_path)
    runner = AgentRunner(cfg, broker)
    approval = broker.approve(broker.request(
        task="supervised system action", agent="claude", scope="pc_full",
    )["request_id"])
    grant = runner._verify(approval["grant_token"], project, "claude")
    assert grant.scope == "pc_full"
