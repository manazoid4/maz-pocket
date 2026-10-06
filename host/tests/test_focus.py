from __future__ import annotations

import json

from fastapi import FastAPI
from fastapi.testclient import TestClient

from mazhost.focus import Focus, install_focus_routes


class Clock:
    t = 1000.0

    def __call__(self):
        return self.t


def setup(tmp_path):
    now = tmp_path / "NOW.md"
    now.write_text("# NOW\n- [x] done thing\n- [ ] Ship the widget\n- [ ] later\n", encoding="utf-8")
    clock = Clock()
    focus = Focus(str(now), str(tmp_path / "f.jsonl"), clock)
    api = FastAPI()
    install_focus_routes(api, focus)
    return TestClient(api), focus, clock


def test_default_task_from_now_md(tmp_path):
    c, _, _ = setup(tmp_path)
    s = c.post("/focus/start").json()
    assert s["task"] == "Ship the widget" and s["minutes"] == 25 and s["muted"] and s["remaining_s"] == 1500


def test_explicit_task_and_state(tmp_path):
    c, _, clock = setup(tmp_path)
    c.post("/focus/start", json={"minutes": 10, "task": "write"})
    clock.t += 60
    s = c.get("/focus/state").json()
    assert s["task"] == "write" and s["remaining_s"] == 540 and s["state"] == "running"


def test_end_event_and_log(tmp_path):
    c, focus, clock = setup(tmp_path)
    c.post("/focus/start", json={"minutes": 1})
    assert c.get("/focus/events").json()["events"] == []
    clock.t += 61
    assert c.get("/focus/state").json()["state"] == "awaiting_done"
    assert focus.muted  # still muted until answered
    ev = c.get("/focus/events").json()["events"]
    assert [e["text"] for e in ev] == ["Done?"]
    assert c.get("/focus/events").json()["events"] == []  # drained
    r = c.post("/focus/end", json={"done": True}).json()["logged"]
    assert r["done"] is True and r["task"] == "Ship the widget"
    rows = [json.loads(x) for x in (tmp_path / "f.jsonl").read_text().splitlines()]
    assert len(rows) == 1 and rows[0]["minutes"] == 1
    assert c.get("/focus/state").json() == {"state": "idle", "muted": False, "remaining_s": 0}
    assert c.post("/focus/end", json={"done": False}).status_code == 409


def test_nudge_muted_in_app(tmp_path, monkeypatch):
    from mazhost.app import create_app
    from mazhost.config import Settings

    tok = "test-token-that-is-not-default"
    monkeypatch.setenv("MAZ_FOCUS_LOG", str(tmp_path / "f.jsonl"))
    monkeypatch.setenv("MAZ_NOW_FILE", str(tmp_path / "none.md"))
    c = TestClient(create_app(Settings(token=tok, _env_file=None)))
    h = {"Authorization": f"Bearer {tok}"}
    assert c.post("/focus/start", json={"task": "x"}, headers=h).json()["muted"] is True
    r = c.post("/nudge/abc/nudge", headers=h)
    assert r.status_code == 200 and r.json() == {"ok": False, "muted": True, "reason": "focus_sprint"}
