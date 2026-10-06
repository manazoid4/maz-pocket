

def test_prompt_contains_today_and_no_time_refusal():
    from datetime import datetime
    from zoneinfo import ZoneInfo
    import test_app
    models = test_app.FakeModels()
    api = test_app.client(models=models)
    h = {"Authorization": "Bearer test-token-that-is-not-default"}
    sid = api.post("/session/start", headers=h).json()["session_id"]
    api.post("/turn/text", headers=h, json={"text": "what time is it", "session_id": sid, "route": "auto"})
    system = models.last_messages[0]["content"]
    now = datetime.now(ZoneInfo("Europe/London"))
    assert f"{now.day} {now:%B %Y}" in system and "Location: UK" in system
    assert "never say you lack access" in system
from mazhost.version import CORE_VERSION
from test_app import client


def test_health_reports_version_and_name():
    body = client().get("/health", headers={"Authorization": "Bearer test-token-that-is-not-default"}).json()
    assert body["version"] == CORE_VERSION
    assert body["name"] == "nod Core"


def test_health_reports_staged_firmware(tmp_path, monkeypatch):
    monkeypatch.setenv("MAZ_FW_DIR", str(tmp_path))
    hdr = {"Authorization": "Bearer test-token-that-is-not-default"}
    assert client().get("/health", headers=hdr).json()["fw_latest"] is None
    (tmp_path / "manifest.json").write_text('{"version": "1.2.3", "sha": "abc1234"}')
    (tmp_path / "latest.bin").write_bytes(b"x")
    assert client().get("/health", headers=hdr).json()["fw_latest"] == {"version": "1.2.3", "sha": "abc1234"}


def test_fw_endpoints_serve_staged_firmware(tmp_path, monkeypatch):
    monkeypatch.setenv("MAZ_FW_DIR", str(tmp_path))
    hdr = {"Authorization": "Bearer test-token-that-is-not-default"}
    assert client().get("/fw/manifest", headers=hdr).status_code == 404
    (tmp_path / "manifest.json").write_text('{"version": "1.2.3", "sha": "abc", "sha256": "00", "size": 3}')
    (tmp_path / "latest.bin").write_bytes(b"abc")
    assert client().get("/fw/manifest", headers=hdr).json()["size"] == 3
    assert client().get("/fw/latest.bin", headers=hdr).content == b"abc"
    assert client().get("/fw/latest.bin").status_code in (401, 403)
