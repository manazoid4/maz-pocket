from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import threading
import time
from pathlib import Path

from fastapi.testclient import TestClient

from mazhost.app import create_app
from mazhost.config import Settings

TOKEN = "test-token-that-is-not-default"
H = {"Authorization": f"Bearer {TOKEN}"}
HOOK = Path(__file__).parents[1] / "hooks" / "nod_permission_hook.py"
EVENT = {"session_id": "s1", "hook_event_name": "PermissionRequest",
         "tool_name": "Bash", "tool_input": {"command": "npm test"}}


def api():
    return TestClient(create_app(Settings(token=TOKEN, _env_file=None)))


def test_request_pending_decide():
    c = api()
    rid = c.post("/buddy/request", json={"tool": "Bash", "summary": "ls"}, headers=H).json()["id"]
    assert [p["id"] for p in c.get("/buddy/pending", headers=H).json()["pending"]] == [rid]
    assert c.get(f"/buddy/state?id={rid}", headers=H).json()["decision"] is None
    c.post("/buddy/decide", json={"id": rid, "decision": "deny"}, headers=H)
    assert c.get(f"/buddy/state?id={rid}", headers=H).json()["decision"] == "deny"
    assert c.get("/buddy/pending", headers=H).json()["pending"] == []


def test_allow_all_is_rejected_without_granting_other_requests():
    c = api()
    r1 = c.post("/buddy/request", json={"tool": "Bash", "session_id": "s"}, headers=H).json()["id"]
    r2 = c.post("/buddy/request", json={"tool": "Edit", "session_id": "s"}, headers=H).json()["id"]
    assert c.post("/buddy/decide", json={"id": r1, "decision": "allow_all"}, headers=H).status_code == 400
    assert c.get(f"/buddy/state?id={r1}", headers=H).json()["decision"] is None
    assert c.get(f"/buddy/state?id={r2}", headers=H).json()["decision"] is None
    assert c.post("/buddy/allow-all", json={"session_id": "s", "on": True}, headers=H).status_code == 400
    assert c.post("/buddy/request", json={"tool": "Bash", "session_id": "s"}, headers=H).json()["decision"] is None


def test_decisions_are_bound_to_one_request():
    c = api()
    r1 = c.post("/buddy/request", json={"tool": "Bash", "session_id": "s"}, headers=H).json()["id"]
    r2 = c.post("/buddy/request", json={"tool": "Bash", "session_id": "s"}, headers=H).json()["id"]
    assert c.post("/buddy/decide", json={"id": r1, "decision": "allow"}, headers=H).json()["decision"] == "allow"
    assert c.get(f"/buddy/state?id={r2}", headers=H).json()["decision"] is None
    assert c.post("/buddy/decide", json={"id": r1, "decision": "deny"}, headers=H).json()["decision"] == "allow"


def test_expired_requests_cannot_be_approved(monkeypatch):
    c = api()
    rid = c.post("/buddy/request", json={"tool": "Bash"}, headers=H).json()["id"]
    import mazhost.buddy as bm
    real = bm.time.monotonic
    monkeypatch.setattr(bm.time, "monotonic", lambda: real() + bm.TIMEOUT_S + 10)
    assert c.get("/buddy/summary", headers=H).json()["count"] == 0
    assert c.get("/buddy/pending", headers=H).json()["pending"] == []
    response = c.post("/buddy/decide", json={"id": rid, "decision": "allow"}, headers=H)
    assert response.status_code == 409
    assert response.json()["detail"] == "approval_expired"
    assert c.get(f"/buddy/state?id={rid}", headers=H).json()["decision"] == "cancel"


def test_auth_required():
    assert api().get("/buddy/pending").status_code == 401


def load_hook(monkeypatch, client, timeout):
    monkeypatch.setenv("MAZ_TOKEN", TOKEN)
    monkeypatch.setenv("MAZ_BUDDY_TIMEOUT", timeout)
    spec = importlib.util.spec_from_file_location("hook", HOOK)
    hook = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(hook)
    hook.call = lambda m, p, body=None, timeout=10: client.request(m, p, json=body, headers=H).json()
    return hook


def run_main(hook, monkeypatch, capsys):
    import io
    monkeypatch.setattr(sys, "stdin", io.StringIO(json.dumps(EVENT)))
    assert hook.main() == 0
    return capsys.readouterr()


def test_acceptance_request_decide_hook_prints_allow(monkeypatch, capsys):
    c = api()
    hook = load_hook(monkeypatch, c, "60")

    def device():
        for _ in range(200):
            p = c.get("/buddy/pending", headers=H).json()["pending"]
            if p:
                assert p[0]["tool"] == "Bash" and "npm test" in p[0]["summary"]
                c.post("/buddy/decide", json={"id": p[0]["id"], "decision": "allow"}, headers=H)
                return
            time.sleep(0.05)

    t = threading.Thread(target=device)
    t.start()
    out = run_main(hook, monkeypatch, capsys)
    t.join()
    got = json.loads(out.out)["hookSpecificOutput"]
    assert got == {"hookEventName": "PermissionRequest", "decision": {"behavior": "allow"}}


def test_acceptance_timeout_prints_ask(monkeypatch, capsys):
    c = api()
    hook = load_hook(monkeypatch, c, "0.3")
    out = run_main(hook, monkeypatch, capsys)
    assert out.out == "" and out.err.strip() == "ask"  # no stdout = terminal prompt
    assert c.get("/buddy/pending", headers=H).json()["pending"] == []  # cancelled


def test_core_down_asks():
    env = {**os.environ, "MAZ_TOKEN": TOKEN, "MAZ_CORE_URL": "http://127.0.0.1:1"}
    out = subprocess.run([sys.executable, str(HOOK)], input=json.dumps(EVENT), text=True,
                         capture_output=True, env=env, timeout=30)
    assert out.returncode == 0 and out.stdout == "" and out.stderr.strip() == "ask"


def test_summary_states_and_fields():
    c = api()
    s = c.get("/buddy/summary", headers=H).json()
    assert s["agent"] == "idle" and s["count"] == 0 and s["items"] == []
    a = c.post("/buddy/request", json={"tool": "Edit", "summary": "a.py", "project": "maz"}, headers=H).json()["id"]
    b = c.post("/buddy/request", json={"tool": "Bash", "summary": "ls"}, headers=H).json()["id"]
    s = c.get("/buddy/summary", headers=H).json()
    assert s["agent"] == "needs_you" and s["count"] == 2
    assert [i["id"] for i in s["items"]] == [a, b]  # oldest first
    assert s["items"][0]["project"] == "maz" and 55 <= s["items"][0]["left"] <= 60
    c.post("/buddy/decide", json={"id": a, "decision": "allow"}, headers=H)
    c.post("/buddy/decide", json={"id": b, "decision": "cancel"}, headers=H)
    s = c.get("/buddy/summary", headers=H).json()
    assert s["agent"] == "working" and s["count"] == 0  # recent activity, nothing pending


def test_summary_requires_auth_and_hides_expired():
    c = api()
    assert c.get("/buddy/summary").status_code == 401
    rid = c.post("/buddy/request", json={"tool": "Bash"}, headers=H).json()["id"]
    import mazhost.buddy as bm
    real = bm.time.monotonic
    bm.time.monotonic = lambda: real() + 100
    try:
        assert c.get("/buddy/summary", headers=H).json()["count"] == 0
        assert c.get("/buddy/pending", headers=H).json()["pending"] == []
    finally:
        bm.time.monotonic = real
    assert rid


def test_hook_sends_project(monkeypatch, capsys):
    c = api()
    hook = load_hook(monkeypatch, c, "0.2")
    import io
    ev = {**EVENT, "cwd": "C:\\Users\\x\\maz-pocket"}
    monkeypatch.setattr(sys, "stdin", io.StringIO(json.dumps(ev)))
    sent = []
    orig = hook.call
    hook.call = lambda m, p, body=None, timeout=10: (sent.append(body), orig(m, p, body, timeout))[1]
    hook.main()
    assert sent[0]["project"] == "maz-pocket"
