from __future__ import annotations

import io
import wave

import pytest
from fastapi.testclient import TestClient

from mazhost.app import create_app
from mazhost.config import Settings
from mazhost.flow import parse_prefix, rule_cleanup, load_dictionary


def _wav() -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000)
        w.writeframes(b"\x00\x00" * 16000)
    return buf.getvalue()


@pytest.mark.parametrize("raw,want", [
    ("um so uh this is the the plan", "So this is the plan."),
    ("i want a fix, uh, for flowlens", "I want a fix, for flowlens."),
    ("Already fine.", "Already fine."),
])
def test_rule_cleanup(raw, want):
    assert rule_cleanup(raw) == want


@pytest.mark.parametrize("text,want", [
    ("Note: buy cables", ("note", "buy cables")),
    ("note, buy cables.", ("note", "buy cables.")),
    ("Remind me to call Sam in 20 minutes", ("remind", "call Sam in 20 minutes")),
    ("Claude, fix the failing test.", ("claude", "fix the failing test.")),
    ("notebook sizes are odd", ("paste", "notebook sizes are odd")),
    ("Claudette said hi", ("paste", "Claudette said hi")),
])
def test_parse_prefix(text, want):
    assert parse_prefix(text) == want


def test_dictionary_default(tmp_path):
    words = load_dictionary(tmp_path / "dict.txt")
    assert "MAZos" in words and "Groq" in words and (tmp_path / "dict.txt").exists()


class Stt:
    prompts: list = []
    def available(self): return True
    def transcribe(self, p): return ""
    def transcribe_prompted(self, path, prompt):
        Stt.prompts.append(prompt)
        return "um so note uh buy the the cables for cardputer"


class Models:
    def chat(self, messages, route):
        assert "cardputer" in messages[-1]["content"].lower()
        assert "MAZos" in messages[0]["content"]  # dictionary reaches cleanup
        return "Note: buy the cables for Cardputer.", "fake"


def test_dictate_endpoint(tmp_path):
    cfg = Settings(token="t" * 40, core_enabled=False, flow_dir=str(tmp_path), groq_api_key="")
    client = TestClient(create_app(cfg, stt=Stt(), models=Models()))
    r = client.post("/dictate?target=text", content=_wav(),
                    headers={"Authorization": "Bearer " + "t" * 40, "Content-Type": "audio/wav"})
    assert r.status_code == 200, r.text
    j = r.json()
    assert j["intent"] == "note" and j["text"] == "buy the cables for Cardputer."
    assert "Cardputer" in Stt.prompts[0] and "nod" in Stt.prompts[0]
    assert j["ms"]["total"] >= 0
    assert "buy the cables" in (tmp_path / "inbox.md").read_text()
    assert "dictate" in (tmp_path / "latency.log").read_text()


def test_one_release_one_paste(monkeypatch):
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    import nodflow

    class R:
        def raise_for_status(self): pass
        def json(self): return {"text": "hello", "intent": "claude", "ms": {}}

    class C:
        def post(self, *a, **k): return R()

    calls = []
    nodflow.send(C(), b"x", 0.0, paste_fn=lambda t, enter=False: calls.append((t, enter)))
    assert calls == [("hello", True)]
