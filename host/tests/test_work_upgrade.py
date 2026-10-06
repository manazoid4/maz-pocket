"""v0.8.0 -> v1.0.0 upgrade safety (spec Section 9): .env-equivalent settings,
the pairing token/authority secret, and the new WORK database must all survive
booting Core with WORK Consistency wired in. MAZ Core has no destructive
install step of its own (the Windows installer only stages firmware/ZIPs), so
the real risk is Core startup silently resetting or overwriting existing
`~/.maz-pocket/*` state the first time it boots WORK-aware code."""

from __future__ import annotations

import os
import shutil
import subprocess
import zipfile
from pathlib import Path

from mazhost.authority import AuthorityBroker
from mazhost.config import Settings
from mazhost.phone_control import build_phone_app
from mazhost.work_store import WorkStore


def _v080_fixture_settings(tmp_path: Path) -> Settings:
    return Settings(
        _env_file=None,
        token="pre-upgrade-pairing-token-abc123",
        control_dir=str(tmp_path / "control"),
        debug_dir=str(tmp_path / "debug"),
        work_dir=str(tmp_path / "work"),
        project_roots=str(tmp_path),
    )


def test_v080_pairing_and_settings_survive_v100_boot(tmp_path):
    # Seed v0.8.0-equivalent state: an issued authority secret + a pending
    # approval, exactly what a real installation would have on disk pre-upgrade.
    old_settings = _v080_fixture_settings(tmp_path)
    old_broker = AuthorityBroker(old_settings)
    old_broker.request(task="pre-upgrade task", agent="claude", scope="pc_full")
    authority_key_before = (Path(old_settings.control_dir).expanduser() / "authority.key").read_bytes()
    token_id_before = old_broker.token_id

    assert not (Path(old_settings.work_dir).expanduser() / "work.sqlite3").exists()

    # v1.0.0 boot: same settings, WORK-aware phone app built on top.
    new_settings = _v080_fixture_settings(tmp_path)
    new_broker = AuthorityBroker(new_settings)
    build_phone_app(new_settings, new_broker)

    authority_key_after = (Path(new_settings.control_dir).expanduser() / "authority.key").read_bytes()
    assert authority_key_after == authority_key_before  # signing secret untouched
    assert new_broker.token_id == token_id_before  # pairing token fingerprint unchanged
    assert new_settings.token == old_settings.token  # pairing token itself unchanged

    # WORK database created fresh, seeded, without touching authority state.
    work_db = Path(new_settings.work_dir).expanduser() / "work.sqlite3"
    assert work_db.exists()
    store = WorkStore(new_settings.work_dir)
    ids = {t["id"] for t in store.list_tracks()}
    assert {"job_hunt", "maz_works"}.issubset(ids)


def test_work_data_survives_repeated_boots_across_the_upgrade_boundary(tmp_path):
    settings = _v080_fixture_settings(tmp_path)
    broker = AuthorityBroker(settings)
    build_phone_app(settings, broker)
    store = WorkStore(settings.work_dir)
    store.create_event(
        event_id="evt_pre_upgrade", track_id="job_hunt", event_type_id="job_hunt.application",
        value=1, occurred_at=1_700_000_000.0, source="phone_manual", note=None, session_id="s1",
    )

    # Simulate a second boot (e.g. after the v1.0.0 binary swap) with a fresh
    # Settings/broker/app instance, same on-disk directories.
    settings_after = _v080_fixture_settings(tmp_path)
    broker_after = AuthorityBroker(settings_after)
    build_phone_app(settings_after, broker_after)

    store_after = WorkStore(settings_after.work_dir)
    events = store_after.events_for_range(start=1_699_999_000.0, end=1_700_001_000.0)
    assert any(e["event_id"] == "evt_pre_upgrade" for e in events)


def test_real_windows_update_path_preserves_env_pairing_and_work_database(tmp_path):
    shell = shutil.which("pwsh") or shutil.which("powershell")
    if shell is None:
        raise AssertionError("PowerShell is required to verify the supported Windows update path")

    package = tmp_path / "release-package"
    package.mkdir()
    source_setup = Path(__file__).parents[2] / "scripts" / "setup-all.ps1"
    shutil.copy2(source_setup, package / "setup-all.ps1")
    (package / "VERSION").write_text("1.0.0\n", encoding="utf-8")

    core_payload = tmp_path / "core-payload"
    core_payload.mkdir()
    (core_payload / "install-core.ps1").write_text(
        "param([switch]$NoBrowser)\nWrite-Host 'fixture core installed'\n",
        encoding="utf-8-sig",
    )
    (core_payload / "v100-marker.txt").write_text("new core payload", encoding="utf-8")
    with zipfile.ZipFile(package / "MAZ-Core-v1.0.0.zip", "w") as archive:
        for path in core_payload.iterdir():
            archive.write(path, path.name)

    local_app_data = tmp_path / "local-app-data"
    core_home = local_app_data / "MAZ Core"
    core_home.mkdir(parents=True)
    env_text = "MAZ_TOKEN=pre-upgrade-pairing-token-abc123\nMAZ_DEFAULT_ROUTE=local\n"
    (core_home / ".env").write_text(env_text, encoding="utf-8")

    fake_profile = tmp_path / "profile"
    control_dir = fake_profile / ".maz-pocket" / "control"
    control_dir.mkdir(parents=True)
    authority_key = control_dir / "authority.key"
    authority_key.write_bytes(b"pre-upgrade-authority-key" * 2)
    work_store = WorkStore(fake_profile / ".maz-pocket" / "work")
    work_store.bootstrap()
    work_store.create_event(
        event_id="evt_update_fixture", track_id="job_hunt",
        event_type_id="job_hunt.application", value=1,
        occurred_at=1_700_000_000.0, source="phone_manual",
        note=None, session_id="fixture-session",
    )

    process_env = os.environ.copy()
    process_env.update(
        {
            "LOCALAPPDATA": str(local_app_data),
            "TEMP": str(tmp_path / "temp"),
            "TMP": str(tmp_path / "temp"),
            "USERPROFILE": str(fake_profile),
        }
    )
    (tmp_path / "temp").mkdir()
    result = subprocess.run(
        [
            shell, "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", str(package / "setup-all.ps1"),
            "-SkipSd", "-NoBrowser",
        ],
        input="Y\n",
        text=True,
        capture_output=True,
        env=process_env,
        check=False,
    )
    assert result.returncode == 0, result.stdout + "\n" + result.stderr
    assert (core_home / ".env").read_text(encoding="utf-8-sig") == env_text
    assert authority_key.read_bytes() == b"pre-upgrade-authority-key" * 2
    assert (core_home / "v100-marker.txt").read_text(encoding="utf-8") == "new core payload"
    surviving = work_store.events_for_range(start=1_699_999_000.0, end=1_700_001_000.0)
    assert any(event["event_id"] == "evt_update_fixture" for event in surviving)
