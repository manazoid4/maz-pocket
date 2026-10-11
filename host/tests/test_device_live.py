"""Device tab: live screen proxy, remote key/update actions, version comparison."""
import httpx
import pytest

from mazhost.config import Settings
from mazhost.core import CoreError, MazCore


def _service(handler, **kw):
    settings = Settings(token="t" * 32, cardputer_url="http://mazpocket.local", **kw)
    return MazCore(settings, client=httpx.Client(transport=httpx.MockTransport(handler)))


def test_known_device_ip_is_tried_before_mdns():
    seen = []

    def handler(request):
        seen.append(request.url.host)
        return httpx.Response(200, content=b"\0" * (240 * 135 * 2))

    svc = _service(handler)
    svc.note_device("192.168.1.42")
    assert len(svc.cardputer_screen()) == 64800
    assert seen == ["192.168.1.42"]


def test_falls_back_to_configured_url_and_ignores_public_ips():
    seen = []

    def handler(request):
        seen.append(request.url.host)
        if request.url.host == "10.0.0.9":
            raise httpx.ConnectError("down")
        return httpx.Response(200, json={"version": "1.0.2"})

    svc = _service(handler)
    svc.note_device("8.8.8.8")
    assert svc.device_ip is None
    svc.note_device("10.0.0.9")
    assert svc.cardputer_status()["version"] == "1.0.2"
    assert seen == ["10.0.0.9", "mazpocket.local"]


def test_action_forwards_form_and_token():
    got = {}

    def handler(request):
        got["body"] = request.content.decode()
        got["token"] = request.headers.get("X-MAZ-Token")
        return httpx.Response(423, text="Approvals are decided on the device itself.")

    out = _service(handler).cardputer_action("key:40,0,0")
    assert got["body"] == "action=key%3A40%2C0%2C0"
    assert got["token"] == "t" * 32
    assert out == {"ok": False, "status": 423, "text": "Approvals are decided on the device itself."}


def test_unreachable_device_raises_core_error():
    def handler(request):
        raise httpx.ConnectError("down")

    with pytest.raises(CoreError):
        _service(handler).cardputer_screen()


def test_action_whitelist():
    from mazhost.app import _DEVICE_ACTION
    ok = ["update", "reboot", "key:4,97,0", "key:82,0,1", "open:inbox"]
    bad = ["launcher", "pc:lock", "key:4,97", "update;reboot", "open:../x", "key:4,97,0,1", "reconnect"]
    assert all(_DEVICE_ACTION.fullmatch(a) for a in ok)
    assert not any(_DEVICE_ACTION.fullmatch(a) for a in bad)


def test_last_seen_is_unknown_until_the_device_proves_it_is_alive():
    svc = _service(lambda request: httpx.Response(200, json={"version": "1.0.2"}))
    assert svc.device_seen_at is None
    svc.cardputer_status()
    assert svc.device_seen_at is not None


def test_offline_device_never_refreshes_last_seen():
    def handler(request):
        raise httpx.ConnectError("down")

    svc = _service(handler)
    assert svc.cardputer_status()["ok"] is False
    assert svc.device_seen_at is None


def test_core_device_endpoint_reports_offline_honestly(tmp_path):
    from fastapi.testclient import TestClient
    from mazhost.app import create_app

    token = "test-token-that-is-not-default"
    client = TestClient(create_app(Settings(token=token, cardputer_url="http://127.0.0.1:9", work_dir=str(tmp_path / "w"), _env_file=None)))
    assert client.get("/core/device").status_code == 401
    body = client.get("/core/device", headers={"Authorization": f"Bearer {token}"}).json()
    assert body["online"] is False and body["last_seen_ago"] is None and body["device_ip"] is None
    assert body["update_available"] is False  # offline device must never read as "up to date" or "outdated"
