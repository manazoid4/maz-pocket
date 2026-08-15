from __future__ import annotations

from types import SimpleNamespace

import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

from mazhost.comm_stream import FRAME_BYTES, install_comm_stream, validate_start
from mazhost.sessions import SessionStore


class FakeSecurity:
    def require_configured(self):
        return None


class FakeSpeech:
    def transcribe(self, _path):
        return "hello from cardputer"


class FakeModels:
    def stream_chat(self, _messages, _route):
        yield "hello ", "local:test"
        yield "Maz", "local:test"


@pytest.fixture
def api():
    app = FastAPI()
    cfg = SimpleNamespace(token="stream-secret", max_audio_seconds=5)
    sessions = SessionStore(8, 30)

    def grounded(session_id, text):
        assert sessions.has(session_id)
        return [{"role": "user", "content": text}]

    def deterministic(_session_id, _text, _command, _actions):
        raise AssertionError("not expected")

    install_comm_stream(
        app,
        cfg=cfg,
        security=FakeSecurity(),
        speech=FakeSpeech(),
        model_router=FakeModels(),
        sessions=sessions,
        grounded_messages=grounded,
        deterministic_command=deterministic,
    )
    return TestClient(app)


def start_payload(turn="turn-1"):
    return {
        "type": "start",
        "turn_id": turn,
        "route": "local",
        "audio": {
            "format": "pcm_s16le",
            "sample_rate": 16000,
            "channels": 1,
            "frame_ms": 20,
        },
    }


def test_start_contract_is_strict():
    assert validate_start(start_payload())["turn_id"] == "turn-1"
    wrong = start_payload()
    wrong["audio"]["frame_ms"] = 32
    with pytest.raises(ValueError, match="unsupported_audio_format"):
        validate_start(wrong)


def test_ws_streams_pcm_transcript_and_reply_deltas(api):
    with api.websocket_connect(
        "/ws/comm", headers={"Authorization": "Bearer stream-secret"}
    ) as ws:
        ws.send_json(start_payload())
        ready = ws.receive_json()
        assert ready["type"] == "ready"
        assert ready["turn_id"] == "turn-1"
        assert ready["session_id"]

        ws.send_bytes(b"\0" * FRAME_BYTES)
        ws.send_json({"type": "end", "turn_id": "turn-1"})

        transcript = ws.receive_json()
        first = ws.receive_json()
        second = ws.receive_json()
        done = ws.receive_json()
        assert transcript["type"] == "transcript"
        assert transcript["text"] == "hello from cardputer"
        assert [first["type"], second["type"], done["type"]] == ["delta", "delta", "done"]
        assert first["text"] + second["text"] == "hello Maz"
        assert done["reply"] == "hello Maz"
        assert done["provider"] == "local:test"
        assert done["timings"]["total_ms"] >= 0


def test_ws_cancel_is_explicit(api):
    with api.websocket_connect(
        "/ws/comm", headers={"Authorization": "Bearer stream-secret"}
    ) as ws:
        ws.send_json(start_payload("cancel-me"))
        assert ws.receive_json()["type"] == "ready"
        ws.send_json({"type": "cancel", "turn_id": "cancel-me"})
        assert ws.receive_json()["type"] == "cancelled"


def test_ws_rejects_missing_token(api):
    with pytest.raises(WebSocketDisconnect):
        with api.websocket_connect("/ws/comm"):
            pass
