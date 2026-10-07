from __future__ import annotations

from fastapi.testclient import TestClient

from mazhost import remote
from mazhost.app import create_app
from mazhost.config import Settings

TOKEN = "test-token-that-is-not-default"
STATUS = {"Self": {"DNSName": "box.example-tailnet.ts.net.", "TailscaleIPs": ["100.64.1.2", "fd7a::1"]}}
SERVE = {
    "TCP": {"443": {"HTTPS": True}},
    "Web": {"box.example-tailnet.ts.net:443": {"Handlers": {"/": {"Proxy": "http://127.0.0.1:8787"}}}},
    "AllowFunnel": {"box.example-tailnet.ts.net:443": True},
}


def _fake(status, serve):
    def run(exe, args, timeout=5.0):
        return status if args[0] == "status" else serve
    return run


def test_detect_funneled():
    r = remote.detect(8787, run_json=_fake(STATUS, SERVE))
    assert r == {"remote_url": "https://box.example-tailnet.ts.net", "tailnet_ip": "100.64.1.2"}


def test_detect_not_funneled_or_wrong_port():
    no_funnel = {**SERVE, "AllowFunnel": {}}
    assert remote.detect(8787, run_json=_fake(STATUS, no_funnel))["remote_url"] is None
    assert remote.detect(9999, run_json=_fake(STATUS, SERVE))["remote_url"] is None
    assert remote.detect(8787, run_json=_fake(STATUS, SERVE))["tailnet_ip"] == "100.64.1.2"


def test_detect_missing_tailscale(monkeypatch):
    def boom(*a, **k):
        raise FileNotFoundError("tailscale")
    monkeypatch.setattr(remote.subprocess, "run", boom)
    assert remote.detect(8787) == {"remote_url": None, "tailnet_ip": None}


def test_configured_overrides_detection():
    info = remote.RemoteInfo("https://x.ts.net/", 8787, detector=lambda p: {"remote_url": "https://y.ts.net", "tailnet_ip": "100.64.0.9"})
    info.refresh()
    assert info.get() == {"remote_url": "https://x.ts.net", "tailnet_ip": "100.64.0.9"}


def test_health_exposes_remote_only_when_authed(tmp_path):
    app = create_app(Settings(token=TOKEN, work_dir=str(tmp_path / "w"), remote_url="https://a.b.ts.net", remote_detect=False))
    c = TestClient(app)
    assert "remote_url" not in c.get("/health").json()
    full = c.get("/health", headers={"Authorization": f"Bearer {TOKEN}"}).json()
    assert full["remote_url"] == "https://a.b.ts.net" and "tailnet_ip" in full
