from __future__ import annotations

from fastapi.testclient import TestClient

from mazhost.app import create_app
from mazhost.config import Settings

TOKEN = "test-token-that-is-not-default"
FUNNEL = {"X-Forwarded-For": "203.0.113.9", "Tailscale-Funnel-Request": "?1", "X-Forwarded-Proto": "https"}


def _client(tmp_path):
    return TestClient(create_app(Settings(token=TOKEN, work_dir=str(tmp_path / "work"))))


def test_ui_serves_page_without_token(tmp_path):
    r = _client(tmp_path).get("/ui")
    assert r.status_code == 200 and r.headers["content-type"].startswith("text/html")
    assert 'id="app"' in r.text and r.headers["cache-control"] == "no-store"


def test_ui_page_ok_remote_but_data_routes_still_need_token(tmp_path):
    c = _client(tmp_path)
    assert c.get("/ui", headers=FUNNEL).status_code == 200
    for path in ["/models", "/core/projects", "/dumps"]:
        assert c.get(path, headers=FUNNEL).status_code in (401, 404), path
    assert c.get("/models", headers=FUNNEL).status_code == 401
    assert c.post("/braindump/raw", content=b"x", headers=FUNNEL).status_code == 401


def test_ui_public_exemption_is_exact(tmp_path):
    c = _client(tmp_path)
    for path in ["/ui/", "/ui/x", "/uix"]:
        assert c.get(path, headers=FUNNEL, follow_redirects=False).status_code in (401, 404, 307), path
