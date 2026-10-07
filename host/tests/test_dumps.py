from __future__ import annotations

import io
import json
import wave

import pytest
from fastapi.testclient import TestClient

from mazhost import dumps
from mazhost.app import create_app
from mazhost.config import Settings
import nodinbox

STRUCT = {"summary": "Plan the launch", "ideas": ["a idea"], "actions": ["ship it"], "questions": ["when?"]}


def test_save_round_trip_and_index(tmp_path):
    meta = dumps.save_dump(tmp_path, "full raw words", STRUCT, source="device", project="p")
    got = dumps.get_dump(tmp_path, meta["id"])
    assert got["status"] == "new" and got["source"] == "device" and got["project"] == "p"
    assert got["summary"] == "Plan the launch"
    assert "- [ ] ship it" in got["body"] and "## Transcript\nfull raw words" in got["body"]
    index = (tmp_path / "INDEX.md").read_text(encoding="utf-8")
    assert meta["id"] in index and "### p" in index and "nodinbox.py claim" in index
    assert [m["id"] for m in dumps.list_dumps(tmp_path, status="new", project="p")] == [meta["id"]]


def test_keyword_assignment_and_unassigned(tmp_path):
    (tmp_path / "projects.json").write_text(
        json.dumps([{"name": "alpha", "keywords": ["Widget"]}, {"name": "beta", "keywords": ["gizmo"]}])
    )
    assert dumps.assign_project(tmp_path, "fix the WIDGET and widget", "") == "alpha"
    assert dumps.assign_project(tmp_path, "nothing relevant", "") == "unassigned"
    assert dumps.assign_project(tmp_path, "nothing", "", llm=lambda s, u: "Beta.") == "beta"
    assert dumps.assign_project(tmp_path, "nothing", "", llm=lambda s, u: "made-up") == "unassigned"

    def boom(s, u):
        raise RuntimeError("down")

    assert dumps.assign_project(tmp_path, "nothing", "", llm=boom) == "unassigned"


def test_starter_projects_created(tmp_path):
    assert dumps.assign_project(tmp_path, "hello", "") == "unassigned"
    assert (tmp_path / "projects.json").is_file()


def test_double_claim_rejected_then_done(tmp_path):
    dump_id = dumps.save_dump(tmp_path, "t", STRUCT, source="api")["id"]
    dumps.claim_dump(tmp_path, dump_id, "codex")
    dumps.claim_dump(tmp_path, dump_id, "codex")  # same agent is idempotent
    with pytest.raises(dumps.DumpError, match="already_claimed_by:codex"):
        dumps.claim_dump(tmp_path, dump_id, "hermes")
    with pytest.raises(dumps.DumpError):
        dumps.finish_dump(tmp_path, dump_id, "hermes")
    dumps.finish_dump(tmp_path, dump_id, "codex", "did it")
    got = dumps.get_dump(tmp_path, dump_id)
    assert got["status"] == "done" and "did it" in got["body"]
    assert dump_id not in (tmp_path / "INDEX.md").read_text(encoding="utf-8")


@pytest.mark.parametrize("bad", ["../x", "a/b", "a\\b", "..", "a.b", "", "x" * 200])
def test_id_path_traversal_rejected(tmp_path, bad):
    with pytest.raises(dumps.DumpError, match="invalid_dump_id"):
        dumps.get_dump(tmp_path, bad)


def test_cli_claim_exit_code(tmp_path):
    dump_id = dumps.save_dump(tmp_path, "t", STRUCT, source="api")["id"]
    base = ["--dir", str(tmp_path)]
    assert nodinbox.main(base + ["claim", dump_id, "--agent", "a"]) == 0
    assert nodinbox.main(base + ["claim", dump_id, "--agent", "b"]) == 1
    assert nodinbox.main(base + ["add", "I need to buy milk."]) == 0
    assert nodinbox.main(base + ["list"]) == 0


# ---- routes -------------------------------------------------------------

class Stt:
    def available(self):
        return True

    def transcribe(self, _path):
        return "spoken words for the dump."


class Models:
    def __init__(self, fail=False):
        self.fail = fail

    def status(self):
        return {}

    def chat(self, messages, route):
        if self.fail:
            raise RuntimeError("llm down")
        return json.dumps(STRUCT), "local"


def make(tmp_path, models=None):
    cfg = Settings(token="test-token-that-is-not-default", _env_file=None, dumps_dir=str(tmp_path / "inbox"))
    return TestClient(create_app(cfg, stt=Stt(), models=models or Models())), tmp_path / "inbox"


AUTH = {"Authorization": "Bearer test-token-that-is-not-default"}


def wav() -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(16000)
        w.writeframes(bytes(3200))
    return buf.getvalue()


def post_dump(client):
    return client.post("/braindump", headers=AUTH, files={"audio": ("a.wav", wav(), "audio/wav")})


def test_braindump_returns_dump_id(tmp_path):
    client, inbox = make(tmp_path)
    r = post_dump(client)
    assert r.status_code == 200, r.text
    body = r.json()
    assert body["summary"] == "Plan the launch" and body["provider"] == "local"
    assert body["project"] == "unassigned"
    assert "spoken words" in dumps.get_dump(inbox, body["dump_id"])["body"]


def test_braindump_raw_returns_dump_id(tmp_path):
    client, _ = make(tmp_path)
    r = client.post("/braindump/raw", headers={**AUTH, "Content-Type": "audio/wav"}, content=wav())
    assert r.status_code == 200, r.text
    assert r.json()["dump_id"]


def test_braindump_save_failure_does_not_break_reply(tmp_path, monkeypatch):
    client, _ = make(tmp_path)

    def boom(*a, **k):
        raise OSError("disk full")

    monkeypatch.setattr(dumps, "save_dump", boom)
    r = post_dump(client)
    assert r.status_code == 200 and r.json()["dump_id"] is None


def test_llm_failure_still_saves_transcript(tmp_path):
    client, inbox = make(tmp_path, Models(fail=True))
    assert post_dump(client).status_code == 503
    [m] = dumps.list_dumps(inbox)
    assert "spoken words for the dump." in dumps.get_dump(inbox, m["id"])["body"]


def test_dump_routes(tmp_path):
    client, _ = make(tmp_path)
    assert client.get("/dumps").status_code == 401
    made = client.post("/dumps", headers=AUTH, json={"text": "I need to call the bank."}).json()
    dump_id = made["dump_id"]
    assert client.get("/dumps", headers=AUTH, params={"status": "new"}).json()["dumps"][0]["id"] == dump_id
    assert client.get(f"/dumps/{dump_id}", headers=AUTH).json()["status"] == "new"
    assert client.post(f"/dumps/{dump_id}/claim", headers=AUTH, json={"agent": "a"}).status_code == 200
    assert client.post(f"/dumps/{dump_id}/claim", headers=AUTH, json={"agent": "b"}).status_code == 409
    assert client.post(f"/dumps/{dump_id}/assign", headers=AUTH, json={"project": "personal"}).json()["project"] == "personal"
    assert client.post(f"/dumps/{dump_id}/done", headers=AUTH, json={"agent": "a", "note": "ok"}).json()["status"] == "done"
    assert client.get("/dumps/nope", headers=AUTH).status_code == 404
    assert client.get("/dumps/..%2Fprojects", headers=AUTH).status_code in (400, 404)
