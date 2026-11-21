from __future__ import annotations

import json
from pathlib import Path
from types import SimpleNamespace

import pytest

from mazhost.pc_proposals import ProposalError, ProposalStore


class FakeModels:
    def __init__(self, payload: dict):
        self.payload = payload
        self.routes: list[str] = []

    def chat(self, messages, route):
        self.routes.append(route)
        assert "open_youtube" in messages[0]["content"]
        return json.dumps(self.payload), "cloud:test"


class FakePC:
    def __init__(self):
        self.calls: list[tuple[str, str]] = []

    def perform(self, action: str, text: str = ""):
        self.calls.append((action, text))
        return SimpleNamespace(action=action, label="launch requested", path="")


def test_proposal_uses_cloud_and_requires_confirmation_before_execution():
    models = FakeModels({"action": "open_youtube"})
    pc = FakePC()
    store = ProposalStore(models, pc, clock=lambda: 100.0)

    proposal = store.propose("client-a", "open youtube")

    assert models.routes == ["cloud"]
    assert proposal["action"] == "open_youtube"
    assert proposal["preview"] == "Open YouTube"
    assert pc.calls == []

    receipt = store.confirm("client-a", proposal["proposal_id"])
    assert receipt["reply"] == "launch requested"
    assert pc.calls == [("open_youtube", "")]


def test_confirmation_is_bound_and_idempotent():
    pc = FakePC()
    store = ProposalStore(FakeModels({"action": "open_youtube"}), pc, clock=lambda: 100.0)
    proposal = store.propose("client-a", "open youtube")

    with pytest.raises(ProposalError, match="proposal_not_found"):
        store.confirm("client-b", proposal["proposal_id"])

    first = store.confirm("client-a", proposal["proposal_id"])
    second = store.confirm("client-a", proposal["proposal_id"])
    assert second == first
    assert pc.calls == [("open_youtube", "")]


@pytest.mark.parametrize(
    "payload",
    [
        {"action": "open_youtube", "url": "https://example.com"},
        {"action": "run_shell", "text": "calc.exe"},
        {"action": "create_notepad_note", "text": ""},
        {"action": "create_notepad_note", "text": "x\x00y"},
    ],
)
def test_invalid_model_output_fails_closed(payload):
    pc = FakePC()
    store = ProposalStore(FakeModels(payload), pc, clock=lambda: 100.0)
    with pytest.raises(ProposalError, match="invalid_proposal"):
        store.propose("client-a", "do something")
    assert pc.calls == []


def test_expired_proposal_cannot_execute():
    now = [100.0]
    pc = FakePC()
    store = ProposalStore(FakeModels({"action": "open_youtube"}), pc, clock=lambda: now[0])
    proposal = store.propose("client-a", "open youtube")
    now[0] = 221.0
    with pytest.raises(ProposalError, match="proposal_expired"):
        store.confirm("client-a", proposal["proposal_id"])
    assert pc.calls == []


def test_notepad_note_uses_new_file_and_never_overwrites(tmp_path: Path, monkeypatch):
    from mazhost.pc import PCController

    opened: list[Path] = []
    controller = PCController(notes_dir=tmp_path)
    monkeypatch.setattr(controller, "_launch_notepad", opened.append)

    first = controller.perform("create_notepad_note", "first")
    second = controller.perform("create_notepad_note", "second")

    assert Path(first.path).read_text(encoding="utf-8") == "first"
    assert Path(second.path).read_text(encoding="utf-8") == "second"
    assert first.path != second.path
    assert opened == [Path(first.path), Path(second.path)]


def test_youtube_action_opens_only_the_fixed_homepage(monkeypatch):
    from mazhost.pc import PCController

    opened: list[str] = []
    controller = PCController()
    monkeypatch.setattr(controller, "_launch_url", opened.append)
    result = controller.perform("open_youtube")
    assert result.label == "YouTube launch requested"
    assert opened == ["https://www.youtube.com/"]
