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


def test_allow_all_session():
    c = api()
    r1 = c.post("/buddy/request", json={"tool": "Bash", "session_id": "s"}, headers=H).json()["id"]
    c.post("/buddy/decide", json={"id": r1, "decision": "allow_all"}, headers=H)
    assert c.post("/buddy/request", json={"tool": "Bash", "session_id": "s"}, headers=H).json()["decision"] == "allow"
    assert c.post("/buddy/request", json={"tool": "Bash", "session_id": "x"}, headers=H).json()["decision"] is None


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


# ---- agent alerts (/buddy/event) ----
STOP_HOOK = Path(__file__).parents[1] / "hooks" / "nod_stop_hook.py"


def test_event_post_poll_since():
    c = api()
    assert c.get("/buddy/events", headers=H).json() == {"ok": True, "events": [], "last": 0}
    a = c.post("/buddy/event", json={"kind": "done", "text": "hi", "source": "t"}, headers=H).json()
    b = c.post("/buddy/event", json={"kind": "usage", "text": "90%"}, headers=H).json()
    assert (a["id"], b["id"]) == (1, 2)
    got = c.get("/buddy/events?since=1", headers=H).json()
    assert got["last"] == 2 and [e["id"] for e in got["events"]] == [2]
    assert set(got["events"][0]) == {"id", "kind", "text", "source", "ts"}


def test_event_ring_cap_bad_kind_truncate_auth():
    c = api()
    for i in range(25):
        c.post("/buddy/event", json={"kind": "needs", "text": str(i)}, headers=H)
    ev = c.get("/buddy/events", headers=H).json()
    assert len(ev["events"]) == 20 and ev["events"][0]["id"] == 6 and ev["last"] == 25
    assert c.post("/buddy/event", json={"kind": "nope", "text": "x"}, headers=H).status_code == 422
    long = c.post("/buddy/event", json={"kind": "error", "text": "x" * 500}, headers=H).json()
    assert len(long["text"]) == 160
    assert c.post("/buddy/event", json={"kind": "done"}).status_code == 401
    assert c.get("/buddy/events").status_code == 401


def test_stop_hook_posts_and_exits_zero(monkeypatch, capsys):
    import io
    c = api()
    monkeypatch.setenv("MAZ_TOKEN", TOKEN)
    spec = importlib.util.spec_from_file_location("stop_hook", STOP_HOOK)
    hook = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(hook)
    hook.call = lambda m, p, body, timeout=3: c.request(m, p, json=body, headers=H).json()
    monkeypatch.setattr(sys, "stdin", io.StringIO(json.dumps(
        {"hook_event_name": "Stop", "last_assistant_message": "\nAll done here\nmore"})))
    assert hook.main() == 0
    assert capsys.readouterr().out == ""
    e = c.get("/buddy/events", headers=H).json()["events"][0]
    assert (e["kind"], e["text"], e["source"]) == ("done", "All done here", "claude-code")
    monkeypatch.setattr(sys, "stdin", io.StringIO("{}"))
    hook.main()
    assert c.get("/buddy/events", headers=H).json()["events"][1]["text"] == "Claude finished"


def test_stop_hook_dead_hub_silent_exit_zero():
    env = {**os.environ, "MAZ_TOKEN": TOKEN, "MAZ_CORE_URL": "http://127.0.0.1:1"}
    out = subprocess.run([sys.executable, str(STOP_HOOK)], input="{}", text=True,
                         capture_output=True, env=env, timeout=30)
    assert out.returncode == 0 and out.stdout == "" and out.stderr == ""
