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

from mazhost import buddy as buddy_mod
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


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def wire_notify(monkeypatch, c):
    monkeypatch.setenv("NOD_TOKEN", TOKEN)
    notify = load(NOTIFY, "nod_notify")
    sys.modules["nod_notify"] = notify  # the stop hook imports it by name
    notify.call = lambda p, body, timeout=3: c.post(p, json=body, headers=H).json()
    return notify


def stdin(monkeypatch, text):
    import io
    f = io.StringIO(text)
    f.isatty = lambda: False
    monkeypatch.setattr(sys, "stdin", f)


def test_stop_hook_posts_and_exits_zero(monkeypatch, capsys):
    c = api()
    wire_notify(monkeypatch, c)
    hook = load(STOP_HOOK, "stop_hook")
    stdin(monkeypatch, json.dumps(
        {"hook_event_name": "Stop", "last_assistant_message": "\nAll done here\nmore"}))
    assert hook.main() == 0
    assert capsys.readouterr().out == ""
    e = c.get("/buddy/events", headers=H).json()["events"][0]
    assert (e["kind"], e["text"], e["source"]) == ("done", "All done here", "claude-code")
    stdin(monkeypatch, "{}")
    hook.main()
    assert c.get("/buddy/events", headers=H).json()["events"][1]["text"] == "Claude finished"


def test_stop_hook_dead_hub_silent_exit_zero():
    env = {**os.environ, "NOD_TOKEN": TOKEN, "NOD_CORE_URL": "http://127.0.0.1:1"}
    out = subprocess.run([sys.executable, str(STOP_HOOK)], input="{}", text=True,
                         capture_output=True, env=env, timeout=30)
    assert out.returncode == 0 and out.stdout == "" and out.stderr == ""


# ---- generic CLI: nod_notify.py ----
NOTIFY = Path(__file__).parents[1] / "hooks" / "nod_notify.py"


def test_notify_args_path_stores_source(monkeypatch):
    c = api()
    n = wire_notify(monkeypatch, c)
    assert n.main(["done", "build finished", "--source", "codex"]) == 0
    assert n.main(["usage", "90% used", "--source", "x" * 99]) == 0
    evs = c.get("/buddy/events", headers=H).json()["events"]
    assert [(e["kind"], e["text"], e["source"]) for e in evs] == [
        ("done", "build finished", "codex"), ("usage", "90% used", "x" * 40)]


def test_notify_stdin_json_fields(monkeypatch):
    c = api()
    n = wire_notify(monkeypatch, c)
    stdin(monkeypatch, json.dumps({"message": "\n\nfirst line\nsecond " + "z" * 300}))
    assert n.main(["needs", "--source", "gemini"]) == 0
    stdin(monkeypatch, json.dumps({"summary": "s" * 300}))
    n.main(["done"])
    stdin(monkeypatch, "plain text line\nmore")
    n.main(["error"])
    evs = c.get("/buddy/events", headers=H).json()["events"]
    assert (evs[0]["text"], evs[0]["source"]) == ("first line", "gemini")
    assert evs[1]["text"] == "s" * 160 and evs[2]["text"] == "plain text line"


def test_notify_json_as_argument_codex_style(monkeypatch):
    c = api()
    n = wire_notify(monkeypatch, c)
    payload = json.dumps({"type": "agent-turn-complete", "last-assistant-message": "Fixed it"})
    assert n.main(["done", "--source", "codex", payload]) == 0
    e = c.get("/buddy/events", headers=H).json()["events"][0]
    assert (e["text"], e["source"]) == ("Fixed it", "codex")


def test_notify_nod_env_beats_maz_env(monkeypatch):
    n = load(NOTIFY, "nod_notify_env")
    monkeypatch.setenv("MAZ_TOKEN", "old")
    monkeypatch.setenv("MAZ_CORE_URL", "http://old")
    assert (n.env("TOKEN"), n.env("CORE_URL")) == ("old", "http://old")
    monkeypatch.setenv("NOD_TOKEN", "new")
    monkeypatch.setenv("NOD_CORE_URL", "http://new")
    assert (n.env("TOKEN"), n.env("CORE_URL")) == ("new", "http://new")


def test_notify_dead_hub_and_bad_input_silent_exit_zero():
    env = {**os.environ, "NOD_TOKEN": TOKEN, "NOD_CORE_URL": "http://127.0.0.1:1"}
    for argv in (["done", "hi"], ["bogus", "hi"], []):
        out = subprocess.run([sys.executable, str(NOTIFY), *argv], input="", text=True,
                             capture_output=True, env=env, timeout=30)
        assert out.returncode == 0 and out.stdout == "" and out.stderr == ""


def test_permission_hook_reads_nod_env(monkeypatch):
    monkeypatch.delenv("MAZ_TOKEN", raising=False)
    monkeypatch.setenv("NOD_TOKEN", "t")
    assert load(HOOK, "perm").TOKEN == "t"


# ---- Today counters (/buddy/today) ----
def today_api(monkeypatch, day="2026-10-11"):
    clock = {"d": day}
    monkeypatch.setattr(buddy_mod, "_local_today", lambda: clock["d"])
    return api(), clock


def ev(c, kind, source="", text="x"):
    return c.post("/buddy/event", json={"kind": kind, "text": text, "source": source}, headers=H)


def today(c):
    return c.get("/buddy/today", headers=H).json()


def test_today_empty_day_and_auth(monkeypatch):
    c, _ = today_api(monkeypatch)
    assert today(c) == {"ok": True, "date": "2026-10-11", "done": 0, "needs": 0, "errors": 0,
                        "usage": 0, "approved": 0, "denied": 0, "agents": [], "last": None}
    assert c.get("/buddy/today").status_code == 401


def test_today_counts_by_kind_and_last(monkeypatch):
    c, _ = today_api(monkeypatch)
    for k in ("done", "done", "needs", "error", "usage", "usage", "usage"):
        ev(c, k, "codex", text=k)
    got = today(c)
    assert (got["done"], got["needs"], got["errors"], got["usage"]) == (2, 1, 1, 3)
    assert set(got["last"]) == {"kind", "text", "source", "ts"}
    assert (got["last"]["kind"], got["last"]["source"]) == ("usage", "codex")


def test_today_per_agent_and_cap(monkeypatch):
    c, _ = today_api(monkeypatch)
    for _ in range(3):
        ev(c, "done", "claude")
    ev(c, "done", "codex")
    ev(c, "needs", "ghost")  # not a done: no agent row
    ev(c, "done")  # no source
    assert today(c)["agents"] == [{"source": "claude", "done": 3}, {"source": "codex", "done": 1},
                                  {"source": "other", "done": 1}]
    c, _ = today_api(monkeypatch)
    for i in range(10):
        ev(c, "done", f"a{i}")
    ev(c, "done", "a0")  # an already-listed agent keeps counting by name
    agents = today(c)["agents"]
    assert len(agents) == 9 and agents[0] == {"source": "a0", "done": 2}
    assert agents[-1] == {"source": "other", "done": 2} and today(c)["done"] == 11


def test_today_approved_denied_through_decide(monkeypatch):
    c, _ = today_api(monkeypatch)

    def ask(sid=""):
        return c.post("/buddy/request", json={"tool": "Bash", "session_id": sid}, headers=H).json()["id"]

    def decide(rid, d):
        c.post("/buddy/decide", json={"id": rid, "decision": d}, headers=H)

    a, b, x = ask(), ask(), ask()
    decide(a, "allow")
    decide(a, "deny")  # already decided: not counted twice
    decide(b, "deny")
    decide(x, "cancel")  # a hook timeout is neither
    assert (today(c)["approved"], today(c)["denied"]) == (1, 1)
    s1, s2 = ask("s"), ask("s")
    decide(s1, "allow_all")  # s1 and the waiting s2 are both approved
    ask("s")  # auto-allowed afterwards
    assert (today(c)["approved"], today(c)["denied"]) == (4, 1)
    assert c.post("/buddy/decide", json={"id": "nope", "decision": "allow"}, headers=H).status_code == 404


def test_today_midnight_rollover(monkeypatch):
    c, clock = today_api(monkeypatch)
    ev(c, "done", "claude")
    rid = c.post("/buddy/request", json={"tool": "Bash"}, headers=H).json()["id"]
    c.post("/buddy/decide", json={"id": rid, "decision": "allow"}, headers=H)
    assert (today(c)["done"], today(c)["approved"]) == (1, 1)
    clock["d"] = "2026-10-12"  # read after midnight: fresh day, no write needed
    got = today(c)
    assert got["date"] == "2026-10-12" and got["done"] == 0 and got["approved"] == 0
    assert got["agents"] == [] and got["last"] is None
    ev(c, "needs", "claude")
    assert today(c)["needs"] == 1
    clock["d"] = "2026-10-13"  # write after midnight also starts clean
    ev(c, "done", "codex")
    got = today(c)
    assert (got["done"], got["needs"]) == (1, 0) and got["agents"] == [{"source": "codex", "done": 1}]
    assert c.get("/buddy/events", headers=H).json()["last"] == 3  # event ring is untouched
