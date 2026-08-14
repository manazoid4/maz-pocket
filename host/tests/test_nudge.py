from __future__ import annotations

import httpx

from mazhost.config import Settings
from mazhost.nudge import NudgeClient


def test_uses_agent_nudge_local_credential_file(tmp_path):
    credential = tmp_path / "control-plane.key"
    credential.write_text("local-secret\n", encoding="utf-8")
    seen = {}

    def handler(request: httpx.Request):
        seen["authorization"] = request.headers["Authorization"]
        return httpx.Response(200, json={"state": "ALL_SYNCED", "agents": []})

    settings = Settings(nudge_token_file=str(credential), _env_file=None)
    client = NudgeClient(settings, httpx.Client(transport=httpx.MockTransport(handler)))

    assert client.summary()["state"] == "ALL_SYNCED"
    assert seen["authorization"] == "Bearer local-secret"


def test_explicit_nudge_token_takes_precedence(tmp_path):
    missing = tmp_path / "missing.key"
    settings = Settings(
        nudge_token="explicit-secret",
        nudge_token_file=str(missing),
        _env_file=None,
    )
    client = NudgeClient(settings)

    assert client.token == "explicit-secret"
