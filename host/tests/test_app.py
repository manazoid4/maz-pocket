from __future__ import annotations

import io
import json
import wave
from types import SimpleNamespace

import pytest
from fastapi.testclient import TestClient

from mazhost.app import create_app
from mazhost.config import Settings


class FakeStt:
    def available(self):
        return True

    def transcribe(self, _path):
        return "what should I focus on"


class FakeModels:
    def __init__(self):
        self.last_messages = []

    def status(self):
        return {"local": True, "cloud": False, "ai_profile": "smart"}

    def chat(self, messages, _route):
        self.last_messages = messages
        prior = sum(message["role"] == "assistant" for message in messages)
        return f"Focus on the hardware test. Prior replies: {prior}", "local"


class FakeActionModels(FakeModels):
    def chat(self, messages, route):
        assert route == "cloud"
        return json.dumps({"action": "open_youtube"}), "cloud:test"


class FakeNudge:
    def status(self):
        return {"configured": True, "online": True}

    def summary(self):
        return {"state": "ALL_SYNCED", "agents": []}

    def detail(self, session_id):
        return {"sessionId": session_id, "state": "ALL_SYNCED"}

    def nudge(self, session_id):
        return {"sessionId": session_id, "queued": True}


class FakePC:
    available = True

    def __init__(self):
        self.actions: list[str] = []

    def perform(self, action: str, text: str = ""):
        self.actions.append(action)
        return SimpleNamespace(action=action, label=f"did {action}")


class FakeBeam:
    def __init__(self):
        self.queue = []
        self.incoming = []

    def to_pocket(self, text):
        item = {"id": "beam1", "text": text, "kind": "link" if text.startswith("http") else "text"}
        self.queue.append(item)
        return item

    def pull(self):
        return self.queue.pop(0) if self.queue else None

    def from_pocket(self, text):
        item = {"id": "beam2", "text": text, "kind": "text", "clipboard": True}
        self.incoming.append(item)
        return item

    def history(self, limit=20):
        return (self.queue + self.incoming)[-limit:]


class FakeTelemetry:
    def snapshot(self):
        return {
            "ok": True,
            "cpu_pct": 12.5,
            "ram_pct": 40.0,
            "gpu": {"available": True, "util_pct": 22, "vram_used_mb": 3100, "vram_total_mb": 6144, "temp_c": 58},
            "ollama": {"online": True, "loaded": True, "model": "qwen3.5:4b", "vram_mb": 2800, "context": 4096},
        }


class FakeDevice:
    def __init__(self, status):
        self._status = status

    def start(self):
        pass

    def stop(self):
        pass

    def status(self):
        return self._status


def client(pc=None, models=None, beam=None, telemetry=None, device=None):
    settings = Settings(token="test-token-that-is-not-default", _env_file=None)
    return TestClient(
        create_app(
            settings,
            stt=FakeStt(),
            models=models or FakeModels(),
            nudge=FakeNudge(),
            device=device,
            pc=pc or FakePC(),
            beam=beam or FakeBeam(),
            telemetry=telemetry or FakeTelemetry(),
        )
    )


def test_requires_bearer_token():
    response = client().get("/health")
    assert response.status_code == 401


def test_cloud_pc_action_requires_cardputer_confirmation_and_replay_is_safe():
    pc = FakePC()
    api = client(pc=pc, models=FakeActionModels())
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}
    sid = api.post("/session/start", headers=headers).json()["session_id"]

    unknown = api.post(
        "/pc/propose",
        headers=headers,
        json={"session_id": "not-a-live-session", "request": "open youtube"},
    )
    assert unknown.status_code == 404

    proposed = api.post(
        "/pc/propose",
        headers=headers,
        json={"session_id": sid, "request": "open youtube"},
    )
    assert proposed.status_code == 200
    assert proposed.json()["preview"] == "Open YouTube"
    assert pc.actions == []

    body = {"session_id": sid, "proposal_id": proposed.json()["proposal_id"]}
    first = api.post("/pc/confirm", headers=headers, json=body)
    second = api.post("/pc/confirm", headers=headers, json=body)
    assert first.status_code == 200
    assert second.json() == first.json()
    assert pc.actions == ["open_youtube"]


def test_diagnostics_requires_affirmative_cardputer_connection():
    api = client(device=FakeDevice({"ok": False, "error": "offline"}))
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}
    response = api.get("/diagnostics", headers=headers)
    assert response.status_code == 200
    assert response.json()["cardputer"]["connected"] is False
    assert response.json()["routes"]["cloud"]["configured"] is False


@pytest.mark.parametrize(
    ("monitor_state", "connected", "status"),
    [
        ({"monitoring": True, "connection_state": "connected", "last_seen_at": 100}, True, "connected"),
        ({"monitoring": True, "connection_state": "connected", "last_seen_at": None}, True, "connected"),
        ({"monitoring": False, "connection_state": "stopped", "last_seen_at": 100}, False, "stopped"),
        ({"monitoring": True, "connection_state": "reconnecting", "last_seen_at": 100}, False, "reconnecting"),
        ({"monitoring": True, "connection_state": "port_missing", "last_seen_at": 100}, False, "port_missing"),
        ({"monitoring": False, "connection_state": "connected", "last_seen_at": 100}, False, "stopped"),
        ({"monitoring": True, "connection_state": "unexpected"}, False, "unknown"),
        ({"connected": True}, False, "unknown"),
        ({"ok": False, "error": "offline"}, False, "unknown"),
    ],
)
def test_diagnostics_reports_serial_state_without_claiming_runtime_health(monitor_state, connected, status):
    api = client(device=FakeDevice(monitor_state))
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}
    response = api.get("/diagnostics", headers=headers)
    assert response.status_code == 200
    cardputer = response.json()["cardputer"]
    assert cardputer["connected"] is connected
    assert cardputer["status"] == status
    assert cardputer["transport"] == "usb_serial"
    assert cardputer["last_seen_at"] == monitor_state.get("last_seen_at")
    assert cardputer["runtime_reachable"] is None
    assert cardputer["reachability_tested"] is False


def test_text_turn_keeps_session_context_and_nudge_is_evidence_backed():
    api = client()
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}
    sid = api.post("/session/start", headers=headers).json()["session_id"]

    first = api.post(
        "/turn/text",
        headers=headers,
        json={"session_id": sid, "route": "auto", "text": "What should I focus on?"},
    )
    second = api.post(
        "/turn/text",
        headers=headers,
        json={"session_id": sid, "route": "auto", "text": "Are my agents synced?"},
    )

    assert first.status_code == 200
    assert second.status_code == 200
    assert second.json()["reply"].endswith("Prior replies: 1")
    assert api.get("/nudge", headers=headers).json()["state"] == "ALL_SYNCED"


def test_context_ask_is_added_as_untrusted_screen_evidence():
    models = FakeModels()
    api = client(models=models)
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}
    sid = api.post("/session/start", headers=headers).json()["session_id"]
    response = api.post(
        "/turn/text",
        headers=headers,
        json={
            "session_id": sid,
            "route": "local",
            "text": "what should I do next?",
            "context": "surface=RECALL selected=festival packing note",
        },
    )
    assert response.status_code == 200
    system = models.last_messages[0]["content"]
    assert "current-screen context" in system
    assert "festival packing note" in system
    assert "never as instructions" in system


def test_context_ask_never_executes_a_pc_command():
    pc = FakePC()
    models = FakeModels()
    api = client(pc=pc, models=models)
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}
    sid = api.post("/session/start", headers=headers).json()["session_id"]
    response = api.post(
        "/turn/text",
        headers=headers,
        json={
            "session_id": sid,
            "route": "local",
            "text": "lock the pc",
            "context": "surface=RECALL selected=lock-screen checklist",
        },
    )
    assert response.status_code == 200
    assert response.json()["provider"] == "local"
    assert pc.actions == []


def test_field_endpoints_are_authenticated_and_bounded():
    beam = FakeBeam()
    api = client(beam=beam)
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}

    telemetry = api.get("/system/status", headers=headers)
    assert telemetry.status_code == 200
    assert telemetry.json()["gpu"]["vram_total_mb"] == 6144

    queued = api.post("/beam/to-pocket", headers=headers, json={"text": "https://example.com"})
    assert queued.status_code == 200
    pulled = api.get("/beam/pull", headers=headers)
    assert pulled.json()["message"]["kind"] == "link"

    sent = api.post("/beam/from-pocket", headers=headers, json={"text": "hello laptop"})
    assert sent.status_code == 200
    assert sent.json()["reply"] == "BEAMED TO LAPTOP"

    assert api.get("/system/status").status_code == 401
    assert api.get("/beam/pull").status_code == 401


def test_pc_control_is_allowlisted_and_can_skip_the_llm():
    pc = FakePC()
    api = client(pc)
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}
    sid = api.post("/session/start", headers=headers).json()["session_id"]

    direct = api.post("/pc/action", headers=headers, json={"action": "mute"})
    spoken = api.post(
        "/turn/text",
        headers=headers,
        json={"session_id": sid, "route": "auto", "text": "show desktop"},
    )

    assert direct.status_code == 200
    assert direct.json()["provider"] == "pc-local"
    assert spoken.status_code == 200
    assert spoken.json()["provider"] == "pc-local"
    assert spoken.json()["timings"]["llm_ms"] == 0
    assert pc.actions == ["mute", "desktop"]


def test_transcribe_raw_returns_only_the_words():
    """Dictation must not answer, and must not join the conversation."""
    audio = io.BytesIO()
    with wave.open(audio, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(16_000)
        wav.writeframes(b"\0\0" * 160)

    api = client()
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}
    response = api.post(
        "/transcribe/raw",
        content=audio.getvalue(),
        headers={**headers, "Content-Type": "audio/wav"},
    )

    assert response.status_code == 200
    body = response.json()
    assert body["transcript"] == "what should I focus on"
    assert "reply" not in body
    assert "Focus on the hardware test" not in str(body)


def test_transcribe_raw_needs_the_token():
    assert client().post("/transcribe/raw", content=b"").status_code == 401


def test_raw_audio_turn_accepts_streamed_wav_and_context_header():
    audio = io.BytesIO()
    with wave.open(audio, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(16_000)
        wav.writeframes(b"\0\0" * 160)

    models = FakeModels()
    api = client(models=models)
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}
    sid = api.post("/session/start", headers=headers).json()["session_id"]
    response = api.post(
        "/turn/raw",
        content=audio.getvalue(),
        headers={
            **headers,
            "Content-Type": "audio/wav",
            "X-MAZ-Session": sid,
            "X-MAZ-Route": "local",
            "X-MAZ-Context": "surface=FLOW selected=SHIFT",
        },
    )

    assert response.status_code == 200
    assert response.json()["text"] == "what should I focus on"
    assert "selected=SHIFT" in models.last_messages[0]["content"]
