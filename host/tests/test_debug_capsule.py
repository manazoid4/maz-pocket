from __future__ import annotations

from mazhost.config import Settings
from mazhost.debug_capsule import DebugCapsules, redact_text


def test_redacts_common_secrets():
    text = "Authorization: Bearer abcdefghijklmnopqrstuvwxyz api_key=supersecretvalue"
    clean = redact_text(text)
    assert "abcdefghijklmnopqrstuvwxyz" not in clean
    assert "supersecretvalue" not in clean
    assert "[REDACTED]" in clean


def test_capsule_is_bounded_and_persisted(tmp_path):
    cfg = Settings(
        _env_file=None,
        token="configured-token",
        debug_dir=str(tmp_path / "debug"),
        control_dir=str(tmp_path / "control"),
        debug_capsule_limit=5,
    )
    store = DebugCapsules(cfg)
    result = store.collect(
        project="maz-pocket",
        core_status=lambda: {"ok": True, "token": "do-not-store-this-token"},
        project_status=lambda name: {"name": name, "git_status": "clean"},
        jobs_status=lambda: {"jobs": []},
        system_status=lambda: {"cpu": 10},
        model_status=lambda: {"local": True},
        nudge_status=lambda: {"online": True},
        device_status=lambda: {"connected": True},
        authority_status=lambda: {"active": 0},
    )
    assert result["core"]["token"] == "[REDACTED]"
    loaded = store.get(result["capsule_id"])
    assert loaded["capsule_id"] == result["capsule_id"]
    assert store.recent()[0]["project"] == "maz-pocket"
