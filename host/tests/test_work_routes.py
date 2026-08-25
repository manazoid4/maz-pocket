from __future__ import annotations

from fastapi import FastAPI
from fastapi.testclient import TestClient

from mazhost.authority import AuthorityBroker
from mazhost.config import Settings
from mazhost.phone_control import build_phone_app

# The session cookie phone_control.py sets is scoped Path=/control (matching
# how control_routes.py mounts it in production), so tests must mount the
# phone app the same way or the cookie is never sent back.


def _mounted_app(tmp_path) -> tuple[FastAPI, Settings]:
    settings = Settings(
        _env_file=None,
        token="test-token-123456789",
        control_dir=str(tmp_path / "control"),
        debug_dir=str(tmp_path / "debug"),
        work_dir=str(tmp_path / "work"),
        project_roots=str(tmp_path),
    )
    broker = AuthorityBroker(settings)
    root = FastAPI()
    root.mount("/control", build_phone_app(settings, broker))
    return root, settings


def make_client(tmp_path) -> TestClient:
    root, settings = _mounted_app(tmp_path)
    client = TestClient(root, base_url="http://testserver/control/")
    client.post("session/login", data={"token": settings.token})
    client.headers.update({"X-MAZ-Control": "1"})
    return client


def test_summary_requires_authenticated_session(tmp_path):
    root, _settings = _mounted_app(tmp_path)
    unauth = TestClient(root, base_url="http://testserver/control/")
    unauth.headers.update({"X-MAZ-Control": "1"})
    resp = unauth.get("work/summary")
    assert resp.status_code == 401


def test_summary_seeds_job_hunt_and_maz_works(tmp_path):
    client = make_client(tmp_path)
    resp = client.get("work/summary")
    assert resp.status_code == 200
    ids = {t["track_id"] for t in resp.json()["tracks"]}
    assert {"job_hunt", "maz_works"}.issubset(ids)


def test_quick_log_application_is_one_call_and_confirms(tmp_path):
    """Literal tap-count acceptance test (spec Section 2): from an already-open
    WORK tab, POSTing the quick-log-application event once records it and the
    server confirms — the one-tap common case."""
    client = make_client(tmp_path)
    resp = client.post(
        "work/tracks/job_hunt/events",
        json={"event_id": "evt_tap1", "event_type_id": "job_hunt.application"},
    )
    assert resp.status_code == 200
    body = resp.json()
    assert body["ok"] is True
    assert body["created"] is True
    summary = client.get("work/summary").json()
    job_hunt = next(t for t in summary["tracks"] if t["track_id"] == "job_hunt")
    assert job_hunt["today_total"] == 1


def test_duplicate_post_is_200_noop_never_409(tmp_path):
    client = make_client(tmp_path)
    body = {"event_id": "evt_retry", "event_type_id": "job_hunt.application"}
    first = client.post("work/tracks/job_hunt/events", json=body)
    second = client.post("work/tracks/job_hunt/events", json=body)
    assert first.status_code == 200
    assert second.status_code == 200
    assert second.json()["created"] is False
    summary = client.get("work/summary").json()
    job_hunt = next(t for t in summary["tracks"] if t["track_id"] == "job_hunt")
    assert job_hunt["today_total"] == 1


def test_malformed_event_body_returns_422(tmp_path):
    client = make_client(tmp_path)
    resp = client.post("work/tracks/job_hunt/events", json={"event_type_id": ""})
    assert resp.status_code == 422


def test_undo_reverses_last_logged_event_via_api(tmp_path):
    client = make_client(tmp_path)
    logged = client.post(
        "work/tracks/job_hunt/events",
        json={"event_id": "evt_undo1", "event_type_id": "job_hunt.application"},
    ).json()["event"]
    undo = client.post(f"work/events/{logged['event_id']}/undo")
    assert undo.status_code == 200
    summary = client.get("work/summary").json()
    job_hunt = next(t for t in summary["tracks"] if t["track_id"] == "job_hunt")
    assert job_hunt["today_total"] == 0


def test_maz_works_breakdown_never_implies_close_rate(tmp_path):
    client = make_client(tmp_path)
    client.post("work/tracks/maz_works/events", json={"event_type_id": "maz_works.outreach"})
    client.post("work/tracks/maz_works/events", json={"event_type_id": "maz_works.client_won"})
    summary = client.get("work/summary").json()
    maz_works = next(t for t in summary["tracks"] if t["track_id"] == "maz_works")
    # Raw counts only — no derived field like close_rate/score anywhere in the payload.
    assert "close_rate" not in maz_works and "score" not in maz_works
    outreach = next(e for e in maz_works["breakdown"] if e["event_type_id"] == "maz_works.outreach")
    won = next(e for e in maz_works["breakdown"] if e["event_type_id"] == "maz_works.client_won")
    assert outreach["total"] == 1
    assert won["total"] == 1


def test_create_custom_track_from_web_ui(tmp_path):
    client = make_client(tmp_path)
    resp = client.post(
        "work/tracks",
        json={
            "name": "PUSH-UPS", "short_label": "PUSHUPS", "mode": "count", "unit": "reps",
            "event_types": ["REP_SET"], "primary_event_type_index": 0, "pinned": True,
        },
    )
    assert resp.status_code == 200
    track = resp.json()["track"]
    assert track["mode"] == "count"
    log = client.post(f"work/tracks/{track['id']}/events", json={"event_type_id": track["event_types"][0]["id"]})
    assert log.status_code == 200


def test_cardputer_payload_caps_pinned_tracks_and_history_length(tmp_path):
    # The Cardputer-facing summary is a separate, bearer-token-authenticated
    # route on the main api app (mazhost.app.create_app) — the firmware never
    # holds the phone-session cookie used by /control/work/*.
    from mazhost.app import create_app

    client = make_client(tmp_path)
    for i in range(5):
        client.post(
            "work/tracks",
            json={
                "name": f"CUSTOM {i}", "short_label": f"C{i}", "mode": "count", "unit": "x",
                "event_types": ["DONE"], "pinned": True,
            },
        )
    root, settings = _mounted_app(tmp_path)
    api = create_app(settings)
    bearer_client = TestClient(api)
    resp = bearer_client.get("/work/cardputer", headers={"Authorization": f"Bearer {settings.token}"})
    assert resp.status_code == 200
    payload = resp.json()
    assert len(payload["tracks"]) <= 4
    assert len(payload["seven_day"]) == 7


def test_history_bounded_to_seven_days(tmp_path):
    client = make_client(tmp_path)
    resp = client.get("work/history?days=7")
    assert resp.status_code == 200
    assert len(resp.json()["history"]) == 7
