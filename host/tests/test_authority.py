from __future__ import annotations

import time

import pytest

from mazhost.authority import AuthorityBroker, AuthorityError
from mazhost.config import Settings
from mazhost.executor import ElevatedExecutor, ExecutionError


def settings(tmp_path):
    return Settings(
        _env_file=None,
        token="test-token-123456789",
        control_dir=str(tmp_path / "control"),
        debug_dir=str(tmp_path / "debug"),
        project_roots=str(tmp_path),
        control_default_grant_seconds=120,
        control_max_grant_seconds=600,
    )


def test_request_needs_phone_approval_before_grant_exists(tmp_path):
    broker = AuthorityBroker(settings(tmp_path))
    request = broker.request(task="repair build", agent="claude", scope="pc_full")
    assert request["status"] == "pending"
    assert "grant_token" not in request
    assert broker.active_grants() == []


def test_approve_mints_signed_short_lived_grant_and_revoke_kills_it(tmp_path):
    broker = AuthorityBroker(settings(tmp_path))
    request = broker.request(task="repair build", agent="claude", scope="pc_full", duration_seconds=60)
    approved = broker.approve(request["request_id"])
    grant = broker.verify_grant(approved["grant_token"], required={"pc_full"}, agent="claude")
    assert grant.scope == "pc_full"
    broker.revoke(grant.grant_id)
    with pytest.raises(AuthorityError, match="grant_revoked"):
        broker.verify_grant(approved["grant_token"], required={"pc_full"}, agent="claude")


def test_phone_session_is_bound_to_user_agent(tmp_path):
    broker = AuthorityBroker(settings(tmp_path))
    session = broker.issue_phone_session("Pixel-MAZ")
    broker.verify_phone_session(session, "Pixel-MAZ")
    with pytest.raises(AuthorityError, match="phone_session_mismatch"):
        broker.verify_phone_session(session, "Different Browser")


def test_agent_cannot_swap_identity_on_grant(tmp_path):
    broker = AuthorityBroker(settings(tmp_path))
    request = broker.request(task="repair build", agent="claude", scope="pc_full")
    approved = broker.approve(request["request_id"])
    with pytest.raises(AuthorityError, match="wrong_agent"):
        broker.verify_grant(approved["grant_token"], required={"pc_full"}, agent="codex")


def test_project_grant_rejects_working_directory_escape(tmp_path):
    project = tmp_path / "repo"
    project.mkdir()
    outside = tmp_path / "other"
    outside.mkdir()
    cfg = settings(tmp_path)
    broker = AuthorityBroker(cfg)
    executor = ElevatedExecutor(cfg, broker)
    approved = broker.approve(
        broker.request(
            task="test project",
            agent="claude",
            scope="project_full",
            project=str(project),
        )["request_id"]
    )
    with pytest.raises(ExecutionError, match="cwd_outside_approved_project"):
        executor.run(
            grant_token=approved["grant_token"],
            command="Write-Output no",
            cwd=str(outside),
            agent="claude",
        )


def test_pc_full_grant_executes_after_confirmation(tmp_path):
    cfg = settings(tmp_path)
    broker = AuthorityBroker(cfg)
    executor = ElevatedExecutor(cfg, broker)
    approved = broker.approve(
        broker.request(task="test command", agent="claude", scope="pc_full")["request_id"]
    )
    result = executor.run(
        grant_token=approved["grant_token"],
        command="Write-Output MAZ_AUTH_OK",
        cwd=str(tmp_path),
        agent="claude",
    )
    assert result["ok"] is True
    assert "MAZ_AUTH_OK" in result["stdout"]


def test_token_id_is_fingerprint_not_secret(tmp_path):
    cfg = settings(tmp_path)
    broker = AuthorityBroker(cfg)
    assert len(broker.token_id) == 12
    assert cfg.token not in broker.token_id
