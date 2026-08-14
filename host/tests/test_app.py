from __future__ import annotations

import io
import wave

from fastapi.testclient import TestClient

from mazhost.app import create_app
from mazhost.config import Settings


class FakeStt:
    def available(self):
        return True

    def transcribe(self, _path):
        return "what should I focus on"


class FakeModels:
    def status(self):
        return {"local": True, "cloud": False}

    def chat(self, messages, _route):
        prior = sum(message["role"] == "assistant" for message in messages)
        return f"Focus on the hardware test. Prior replies: {prior}", "local"


class FakeNudge:
    def status(self):
        return {"configured": True, "online": True}

    def summary(self):
        return {"state": "ALL_SYNCED", "agents": []}

    def detail(self, session_id):
        return {"sessionId": session_id, "state": "ALL_SYNCED"}

    def nudge(self, session_id):
        return {"sessionId": session_id, "queued": True}


def client():
    settings = Settings(token="test-token-that-is-not-default", _env_file=None)
    return TestClient(create_app(settings, stt=FakeStt(), models=FakeModels(), nudge=FakeNudge()))


def test_requires_bearer_token():
    response = client().get("/health")
    assert response.status_code == 401


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


def test_raw_audio_turn_accepts_streamed_wav():
    audio = io.BytesIO()
    with wave.open(audio, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(16_000)
        wav.writeframes(b"\0\0" * 160)

    api = client()
    headers = {"Authorization": "Bearer test-token-that-is-not-default"}
    sid = api.post("/session/start", headers=headers).json()["session_id"]
    response = api.post(
        "/turn/raw",
        content=audio.getvalue(),
        headers={**headers, "Content-Type": "audio/wav", "X-MAZ-Session": sid, "X-MAZ-Route": "local"},
    )

    assert response.status_code == 200
    assert response.json()["text"] == "what should I focus on"
