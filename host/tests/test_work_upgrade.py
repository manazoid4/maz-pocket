"""v0.8.0 -> v1.0.0 upgrade safety (spec Section 9): .env-equivalent settings,
the pairing token/authority secret, and the new WORK database must all survive
booting Core with WORK Consistency wired in. MAZ Core has no destructive
install step of its own (the Windows installer only stages firmware/ZIPs), so
the real risk is Core startup silently resetting or overwriting existing
`~/.maz-pocket/*` state the first time it boots WORK-aware code."""

from __future__ import annotations

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
