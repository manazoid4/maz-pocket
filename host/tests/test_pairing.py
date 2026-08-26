from __future__ import annotations

import time
from types import SimpleNamespace

from fastapi.testclient import TestClient

from mazhost.app import create_app
from mazhost.config import Settings
from mazhost.pairing import CODE_LENGTH, MAX_CLAIM_ATTEMPTS_PER_CODE, PairingStore

TOKEN = "test-token-that-is-not-default"


class _FakeStt:
    def available(self):
        return True


class _FakeModels:
    def status(self):
        return {"local": True, "cloud": False, "ai_profile": "smart"}

    def diagnostics(self, preferred_route):
        return {"preferred_route": preferred_route}

    def chat(self, messages, route):
        return "ok", "local"


class _FakeNudge:
    def status(self):
        return {"configured": False, "online": False}

    def summary(self):
        return {"state": "ALL_SYNCED", "agents": []}


class _FakePC:
    available = True

    def perform(self, action: str):
        return SimpleNamespace(action=action, label=f"did {action}")


class _FakeBeam:
    def pull(self):
        return None

    def history(self, limit=20):
        return []


class _FakeTelemetry:
    def snapshot(self):
        return {"ok": True}


def client() -> TestClient:
    settings = Settings(token=TOKEN, _env_file=None)
    return TestClient(
        create_app(
            settings,
            stt=_FakeStt(),
            models=_FakeModels(),
            nudge=_FakeNudge(),
            pc=_FakePC(),
            core=None,
            beam=_FakeBeam(),
            telemetry=_FakeTelemetry(),
        )
    )


# --- PairingStore unit tests -------------------------------------------------


def test_code_is_right_length_and_alphabet():
    store = PairingStore()
    code, ttl = store.start()
    assert len(code) == CODE_LENGTH
    assert ttl > 0
    assert code == code.upper()
    assert set(code) <= set("23456789ABCDEFGHJKMNPQRSTUVWXYZ")


def test_claim_succeeds_exactly_once_then_is_dead():
    store = PairingStore()
    code, _ = store.start()
    assert store.claim(code, "1.2.3.4") is True
    assert store.claim(code, "1.2.3.4") is False, "a used code must never be claimable again"


def test_wrong_code_never_succeeds():
    store = PairingStore()
    store.start()
    assert store.claim("WRONGCODE", "1.2.3.4") is False


def test_expired_code_cannot_be_claimed():
    store = PairingStore()
    code, _ = store.start()
    store._current.expires_at = time.monotonic() - 1
    assert store.claim(code, "1.2.3.4") is False


def test_new_start_invalidates_the_previous_outstanding_code():
    store = PairingStore()
    old_code, _ = store.start()
    store.start()
    assert store.claim(old_code, "1.2.3.4") is False


def test_code_is_burned_after_too_many_wrong_attempts():
    store = PairingStore()
    code, _ = store.start()
    for _ in range(MAX_CLAIM_ATTEMPTS_PER_CODE):
        store.claim("NOPE0000", "1.2.3.4")
    assert store.claim(code, "1.2.3.4") is False, "the correct code must die with its own attempt budget"


def test_claims_are_rate_limited_per_ip():
    store = PairingStore()
    store.start()
    results = [store.claim("WRONGCODE", "9.9.9.9") for _ in range(20)]
    assert not any(results[-5:]), "sustained guessing from one IP must eventually be rejected outright"


# --- HTTP surface tests -------------------------------------------------


def test_pair_start_requires_the_existing_bearer_token():
    response = client().post("/pair/start")
    assert response.status_code == 401


def test_pair_claim_needs_no_bearer_token_but_needs_a_valid_code():
    api = client()
    headers = {"Authorization": f"Bearer {TOKEN}"}
    started = api.post("/pair/start", headers=headers)
    assert started.status_code == 200
    code = started.json()["code"]

    bad = api.post("/pair/claim", json={"code": "WRONGCODE"})
    assert bad.status_code == 400
    assert TOKEN not in bad.text

    good = api.post("/pair/claim", json={"code": code})
    assert good.status_code == 200
    assert good.json()["token"] == TOKEN

    replay = api.post("/pair/claim", json={"code": code})
    assert replay.status_code == 400, "a claimed code must not be usable a second time (replay)"


def test_pair_claim_failure_response_does_not_distinguish_reasons():
    api = client()
    never_started = api.post("/pair/claim", json={"code": "AAAAAAAA"})
    assert never_started.status_code == 400
    assert never_started.json()["detail"] == "invalid_or_expired_code"
