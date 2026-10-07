from __future__ import annotations

import json
import subprocess
import types
import time

from fastapi.testclient import TestClient

from mazhost import needs as needs_mod
from mazhost.app import create_app
from mazhost.config import Settings

TOKEN = "test-token-that-is-not-default"
H = {"Authorization": f"Bearer {TOKEN}"}


def PR(n, title, created):
    return {"number": n, "title": title, "createdAt": created, "url": f"https://example.test/pr/{n}"}


def hub(tmp_path, monkeypatch, gh=None, repos="o/r", handoff=None):
    monkeypatch.setenv("MAZ_NEEDS_REPOS", repos)
    if handoff:
        monkeypatch.setenv("MAZ_HANDOFF_DIR", str(handoff))
    else:
        monkeypatch.delenv("MAZ_HANDOFF_DIR", raising=False)

    def run(cmd, **kw):
        if isinstance(gh, Exception):
            raise gh
        return subprocess.CompletedProcess(cmd, 0, stdout=json.dumps(gh or []), stderr="")

    monkeypatch.setattr(needs_mod, "subprocess", types.SimpleNamespace(run=run))
    return TestClient(create_app(Settings(token=TOKEN, data_dir=str(tmp_path), _env_file=None)))


def test_tiers_and_order(tmp_path, monkeypatch):
    (tmp_path / "h").mkdir()
    (tmp_path / "h" / "p.md").write_text(
        "## Owner actions\n- [ ] flash the board\n- [x] done thing\n## Agent\n- [ ] not mine\nMaz: call the client\n",
        encoding="utf-8")
    c = hub(tmp_path, monkeypatch, handoff=tmp_path / "h", gh=[
        PR(1, "Newer (UNPROVEN: x)", "2026-10-06T00:00:00Z"),
        PR(2, "Older (UNPROVEN: y)", "2026-10-01T00:00:00Z"),
        PR(3, "Proven one", "2026-09-01T00:00:00Z")])
    c.post("/buddy/request", json={"tool": "Bash", "summary": "ls", "project": "p"}, headers=H)
    r = c.get("/needs", headers=H).json()
    assert r["tiers"] == {"approvals": 1, "prs": 2, "actions": 2} and r["count"] == 5
    assert [i["tier"] for i in r["items"]] == ["approvals", "prs", "prs", "actions", "actions"]
    assert [i["title"] for i in r["items"] if i["tier"] == "prs"] == ["Older (UNPROVEN: y)", "Newer (UNPROVEN: x)"]
    assert {i["title"] for i in r["items"] if i["tier"] == "actions"} == {"flash the board", "call the client"}
    assert set(r["items"][0]) == {"id", "tier", "title", "project", "age", "detail"}


def test_empty_and_unauthorised(tmp_path, monkeypatch):
    c = hub(tmp_path, monkeypatch, repos="")
    assert c.get("/needs").status_code == 401
    assert c.get("/needs", headers=H).json() == {"count": 0, "tiers": {"approvals": 0, "prs": 0, "actions": 0}, "items": []}


def test_done_and_snooze_persist(tmp_path, monkeypatch):
    gh = [PR(1, "A (UNPROVEN)", "2026-10-01T00:00:00Z"), PR(2, "B (UNPROVEN)", "2026-10-02T00:00:00Z"),
          PR(3, "C (UNPROVEN)", "2026-10-03T00:00:00Z")]
    c = hub(tmp_path, monkeypatch, gh=gh)
    ids = {i["title"][0]: i["id"] for i in c.get("/needs", headers=H).json()["items"]}
    assert c.post(f"/needs/{ids['A']}/done", headers=H).status_code == 200
    assert c.post(f"/needs/{ids['B']}/snooze?hours=2", headers=H).status_code == 200
    assert [i["title"][0] for i in c.get("/needs", headers=H).json()["items"]] == ["C"]
    # a fresh hub on the same data dir still hides them
    c2 = hub(tmp_path, monkeypatch, gh=gh)
    assert [i["title"][0] for i in c2.get("/needs", headers=H).json()["items"]] == ["C"]
    # snooze expires
    state = json.loads((tmp_path / "needs_state.json").read_text())
    state[ids["B"]]["until"] = time.time() - 1
    (tmp_path / "needs_state.json").write_text(json.dumps(state))
    assert [i["title"][0] for i in c2.get("/needs", headers=H).json()["items"]] == ["B", "C"]


def test_gh_failure_is_warning(tmp_path, monkeypatch):
    c = hub(tmp_path, monkeypatch, gh=FileNotFoundError("gh"))
    r = c.get("/needs", headers=H)
    assert r.status_code == 200 and r.json()["count"] == 0 and r.json()["warnings"]


def test_approvals_not_snoozable(tmp_path, monkeypatch):
    c = hub(tmp_path, monkeypatch, repos="")
    c.post("/buddy/request", json={"tool": "Bash"}, headers=H)
    aid = c.get("/needs", headers=H).json()["items"][0]["id"]
    assert c.post(f"/needs/{aid}/snooze?hours=1", headers=H).status_code == 400
    assert c.post(f"/needs/{aid}/done", headers=H).status_code == 400
    assert c.post("/needs/bogus/done", headers=H).status_code == 404
