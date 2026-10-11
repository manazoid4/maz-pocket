"""nod Halt: one switch that makes the hub deny every approval request."""
from __future__ import annotations

import importlib.util
import io
import json
import sys
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from mazhost import buddy as buddy_mod
from mazhost.app import create_app
from mazhost.config import Settings

TOKEN = "test-token-that-is-not-default"
H = {"Authorization": f"Bearer {TOKEN}"}
HOOK = Path(__file__).parents[1] / "hooks" / "nod_permission_hook.py"
EVENT = {"session_id": "s1", "hook_event_name": "PermissionRequest",
         "tool_name": "Bash", "tool_input": {"command": "rm x"}}
REASON = "Halted from nod"


@pytest.fixture
def clock(monkeypatch):
    t = [1_000_000.0]
    monkeypatch.setattr(buddy_mod, "_now", lambda: t[0])
    return t


def api():
    return TestClient(create_app(Settings(token=TOKEN, _env_file=None)))


def halt(c, **body):
    return c.post("/buddy/halt", json=body, headers=H)


def req(c, **kw):
    return c.post("/buddy/request", json={"tool": "Bash", "summary": "x", **kw}, headers=H).json()


def test_on_off_and_get(clock):
    c = api()
    assert c.get("/buddy/halt", headers=H).json() == {"ok": True, "halted": False, "until": None}
    on = halt(c, on=True).json()
    assert on["halted"] is True and on["until"].startswith("1970-01-12T14:16:40")  # now + 30 min
    assert c.get("/buddy/halt", headers=H).json() == on
    assert halt(c, on=False).json() == {"ok": True, "halted": False, "until": None}
    assert c.get("/buddy/halt", headers=H).json()["halted"] is False


def test_minutes_extends_from_now(clock):
    c = api()
    halt(c, on=True, minutes=10)
    clock[0] += 300
    until = halt(c, on=True, minutes=10).json()["until"]
    assert until.startswith("1970-01-12T14:01:40")  # now+5 min, +10 min
    events = c.get("/buddy/events", headers=H).json()["events"]
    assert [e["text"] for e in events] == ["Halt on"]  # extending is not a new alert


def test_expiry_auto_resumes_and_chirps(clock):
    c = api()
    halt(c, on=True, minutes=5)
    clock[0] += 299
    assert c.get("/buddy/halt", headers=H).json()["halted"] is True
    assert req(c)["decision"] == "deny"
    clock[0] += 2
    assert c.get("/buddy/halt", headers=H).json() == {"ok": True, "halted": False, "until": None}
    assert req(c)["decision"] is None  # asks the device again
    texts = [(e["kind"], e["text"], e["source"]) for e in c.get("/buddy/events", headers=H).json()["events"]]
    assert texts == [("needs", "Halt on", "halt"), ("needs", "Halt off", "halt")]


def test_pending_denied_when_halt_turns_on(clock):
    c = api()
    a, b = req(c)["id"], req(c, session_id="s")["id"]
    assert len(c.get("/buddy/pending", headers=H).json()["pending"]) == 2
    halt(c, on=True)
    assert c.get("/buddy/pending", headers=H).json()["pending"] == []
    for rid in (a, b):
        st = c.get(f"/buddy/state?id={rid}", headers=H).json()
        assert st["decision"] == "deny" and st["reason"] == REASON


def test_request_while_halted_denied_immediately_even_with_allow_all(clock):
    c = api()
    r1 = req(c, session_id="s")["id"]
    c.post("/buddy/decide", json={"id": r1, "decision": "allow_all"}, headers=H)
    assert req(c, session_id="s")["decision"] == "allow"
    halt(c, on=True)
    got = req(c, session_id="s")
    assert got["decision"] == "deny" and got["reason"] == REASON
    assert c.get("/buddy/pending", headers=H).json()["pending"] == []
    halt(c, on=False)
    assert req(c, session_id="s")["decision"] is None  # old grant was cleared


def test_allow_all_cannot_be_enabled_during_halt(clock):
    c = api()
    halt(c, on=True)
    response = c.post("/buddy/allow-all", json={"session_id": "s", "on": True}, headers=H)
    assert response.status_code == 409
    assert req(c, session_id="s")["decision"] == "deny"
    halt(c, on=False)
    assert req(c, session_id="s")["decision"] is None


def test_halt_never_approves(clock):
    c = api()
    rid = req(c)["id"]
    halt(c, on=True)
    halt(c, on=False)
    assert c.get(f"/buddy/state?id={rid}", headers=H).json()["decision"] == "deny"
    assert c.post("/buddy/decide", json={"id": rid, "decision": "allow"}, headers=H).json()["decision"] == "deny"
    assert c.get(f"/buddy/state?id={rid}", headers=H).json()["decision"] == "deny"


def test_events_on_and_off(clock):
    c = api()
    halt(c, on=True)
    halt(c, on=True)  # no change, no second chirp
    halt(c, on=False)
    halt(c, on=False)
    ev = c.get("/buddy/events", headers=H).json()["events"]
    assert [(e["kind"], e["text"]) for e in ev] == [("needs", "Halt on"), ("needs", "Halt off")]


@pytest.mark.parametrize("bad", [0, -5, 481, "abc", 1.5])
def test_bad_minutes_422(clock, bad):
    c = api()
    assert halt(c, on=True, minutes=bad).status_code == 422
    assert c.get("/buddy/halt", headers=H).json()["halted"] is False


def test_max_minutes_ok_and_missing_on_422(clock):
    c = api()
    assert halt(c, on=True, minutes=480).status_code == 200
    assert halt(c, minutes=5).status_code == 422


def test_auth_required():
    c = api()
    assert c.get("/buddy/halt").status_code == 401
    assert c.post("/buddy/halt", json={"on": True}).status_code == 401
    assert c.post("/buddy/halt", json={"on": True}, headers={"Authorization": "Bearer nope"}).status_code == 401


def test_hook_prints_halt_deny_with_reason(clock, monkeypatch, capsys):
    c = api()
    monkeypatch.setenv("NOD_TOKEN", TOKEN)
    spec = importlib.util.spec_from_file_location("hook_halt", HOOK)
    hook = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(hook)
    hook.call = lambda m, p, body=None, timeout=10: c.request(m, p, json=body, headers=H).json()
    halt(c, on=True)
    monkeypatch.setattr(sys, "stdin", io.StringIO(json.dumps(EVENT)))
    assert hook.main() == 0
    out = capsys.readouterr()
    assert json.loads(out.out)["hookSpecificOutput"] == {
        "hookEventName": "PermissionRequest", "decision": {"behavior": "deny", "message": REASON}}
    assert out.err == ""


def test_hook_pending_request_denied_mid_wait_prints_reason(clock, monkeypatch, capsys):
    c = api()
    monkeypatch.setenv("NOD_TOKEN", TOKEN)
    spec = importlib.util.spec_from_file_location("hook_halt2", HOOK)
    hook = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(hook)
    calls = {"n": 0}

    def call(m, p, body=None, timeout=10):
        out = c.request(m, p, json=body, headers=H).json()
        if p == "/buddy/request":
            halt(c, on=True)  # Halt lands after the request was queued
        return out

    hook.call = call
    monkeypatch.setattr(sys, "stdin", io.StringIO(json.dumps(EVENT)))
    hook.main()
    got = json.loads(capsys.readouterr().out)["hookSpecificOutput"]["decision"]
    assert got == {"behavior": "deny", "message": REASON}
