from __future__ import annotations

import concurrent.futures
import sqlite3
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

import pytest

from mazhost.work_store import (
    MIGRATIONS,
    WorkStore,
    WorkStoreError,
    local_day_bounds,
)


def make_store(tmp_path) -> WorkStore:
    return WorkStore(tmp_path / "work")


def test_fresh_bootstrap_creates_schema_and_seeds_templates(tmp_path):
    store = make_store(tmp_path)
    store.bootstrap()
    tracks = {t["id"] for t in store.list_tracks()}
    assert {"job_hunt", "maz_works"}.issubset(tracks)
    job_hunt = store.get_track("job_hunt")
    assert job_hunt["primary_event_type_id"] == "job_hunt.application"
    assert any(et["id"] == "job_hunt.application" for et in job_hunt["event_types"])


def test_repeated_bootstrap_is_noop(tmp_path):
    store = make_store(tmp_path)
    store.bootstrap()
    store.update_track("job_hunt", {"name": "MY RENAMED JOB HUNT"})
    store.bootstrap()
    store.bootstrap()
    tracks = store.list_tracks()
    job_hunt_rows = [t for t in tracks if t["id"] == "job_hunt"]
    assert len(job_hunt_rows) == 1
    assert job_hunt_rows[0]["name"] == "MY RENAMED JOB HUNT"  # never overwritten


def test_migration_crash_mid_step_resumes_cleanly(tmp_path, monkeypatch):
    store = make_store(tmp_path)
    child = r"""
import os
import sys
import mazhost.work_store as module

def crash_mid_migration(conn):
    conn.execute("CREATE TABLE crash_marker (id TEXT PRIMARY KEY)")
    os._exit(23)

module.MIGRATIONS = ((1, crash_mid_migration),)
module.WorkStore(sys.argv[1]).bootstrap()
"""
    result = subprocess.run(
        [sys.executable, "-c", child, str(store.root)],
        cwd=Path(__file__).parents[1],
        check=False,
    )
    assert result.returncode == 23

    with sqlite3.connect(store.db_path) as conn:
        version = conn.execute("PRAGMA user_version").fetchone()[0]
    assert version == 0  # rolled back, never partially applied

    store.bootstrap()  # resumes and completes cleanly
    store.bootstrap()  # idempotent — no double-apply
    with sqlite3.connect(store.db_path) as conn:
        version = conn.execute("PRAGMA user_version").fetchone()[0]
        crash_table = conn.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='crash_marker'"
        ).fetchone()
    assert version == 1
    assert crash_table is None
    assert {t["id"] for t in store.list_tracks()} == {"job_hunt", "maz_works"}


def test_write_crash_mid_transaction_leaves_no_partial_row(tmp_path, monkeypatch):
    store = make_store(tmp_path)
    store.bootstrap()
    child = r"""
import os
import sqlite3
import sys
import time

conn = sqlite3.connect(sys.argv[1], isolation_level=None)
conn.execute("PRAGMA foreign_keys=ON")
conn.execute("BEGIN IMMEDIATE")
conn.execute(
    "INSERT INTO work_events (event_id, track_id, event_type_id, value, occurred_at, "
    "source, note, session_id, created_at, reversal_of, reversed) "
    "VALUES (?,?,?,?,?,?,?,?,?,NULL,0)",
    ("evt_crash", "job_hunt", "job_hunt.application", 1, time.time(),
     "phone_manual", None, "s1", time.time()),
)
os._exit(24)
"""
    result = subprocess.run(
        [sys.executable, "-c", child, str(store.db_path)],
        cwd=Path(__file__).parents[1],
        check=False,
    )
    assert result.returncode == 24
    with sqlite3.connect(store.db_path) as conn:
        count = conn.execute("SELECT COUNT(*) FROM work_events WHERE event_id = ?", ("evt_crash",)).fetchone()[0]
    assert count == 0


def test_wal_and_foreign_keys_are_enabled_on_every_store_connection(tmp_path):
    store = make_store(tmp_path)
    store.bootstrap()
    with store._connect() as conn:
        assert conn.execute("PRAGMA journal_mode").fetchone()[0].lower() == "wal"
        assert conn.execute("PRAGMA foreign_keys").fetchone()[0] == 1


def test_corrupt_db_file_fails_loudly_not_silently_zero(tmp_path):
    root = tmp_path / "work"
    root.mkdir()
    (root / "work.sqlite3").write_bytes(b"not a real sqlite database file at all")
    store = WorkStore(root)
    with pytest.raises(WorkStoreError):
        store.bootstrap()


def test_duplicate_event_id_post_is_noop_not_duplicate(tmp_path):
    store = make_store(tmp_path)
    store.bootstrap()
    first, created1 = store.create_event(
        event_id="evt_dup", track_id="job_hunt", event_type_id="job_hunt.application",
        value=1, occurred_at=time.time(), source="phone_manual", note=None, session_id="s1",
    )
    second, created2 = store.create_event(
        event_id="evt_dup", track_id="job_hunt", event_type_id="job_hunt.application",
        value=1, occurred_at=time.time(), source="phone_manual", note=None, session_id="s1",
    )
    assert created1 is True
    assert created2 is False
    assert first["event_id"] == second["event_id"]
    with sqlite3.connect(store.db_path) as conn:
        count = conn.execute("SELECT COUNT(*) FROM work_events WHERE event_id = ?", ("evt_dup",)).fetchone()[0]
    assert count == 1


def test_concurrent_duplicate_event_id_is_one_create_and_all_other_calls_are_noops(tmp_path):
    store = make_store(tmp_path)
    store.bootstrap()

    def create_once(_index):
        return store.create_event(
            event_id="evt_concurrent", track_id="job_hunt",
            event_type_id="job_hunt.application", value=1,
            occurred_at=time.time(), source="phone_manual", note=None,
            session_id="same_phone_session",
        )

    with concurrent.futures.ThreadPoolExecutor(max_workers=16) as pool:
        results = list(pool.map(create_once, range(32)))

    assert sum(1 for _event, created in results if created) == 1
    assert {event["event_id"] for event, _created in results} == {"evt_concurrent"}
    with sqlite3.connect(store.db_path) as conn:
        assert conn.execute(
            "SELECT COUNT(*) FROM work_events WHERE event_id='evt_concurrent'"
        ).fetchone()[0] == 1


def test_undo_reverses_only_current_session_last_event(tmp_path):
    store = make_store(tmp_path)
    store.bootstrap()
    store.create_event(
        event_id="evt_a", track_id="job_hunt", event_type_id="job_hunt.application",
        value=1, occurred_at=time.time(), source="phone_manual", note=None, session_id="session_1",
    )
    store.create_event(
        event_id="evt_b", track_id="job_hunt", event_type_id="job_hunt.application",
        value=1, occurred_at=time.time(), source="phone_manual", note=None, session_id="session_2",
    )
    reversal = store.undo_last(session_id="session_1")
    assert reversal["reversal_of"] == "evt_a"
    with sqlite3.connect(store.db_path) as conn:
        conn.row_factory = sqlite3.Row
        evt_a = dict(conn.execute("SELECT * FROM work_events WHERE event_id='evt_a'").fetchone())
        evt_b = dict(conn.execute("SELECT * FROM work_events WHERE event_id='evt_b'").fetchone())
    assert evt_a["reversed"] == 1
    assert evt_b["reversed"] == 0  # other device's session untouched


def test_concurrent_undo_creates_exactly_one_reversal(tmp_path):
    store = make_store(tmp_path)
    store.bootstrap()
    store.create_event(
        event_id="evt_racy_undo", track_id="job_hunt",
        event_type_id="job_hunt.application", value=1,
        occurred_at=time.time(), source="phone_manual", note=None,
        session_id="same_session",
    )

    def undo_once(_index):
        try:
            return store.undo_last(
                session_id="same_session", expected_event_id="evt_racy_undo"
            )["reversal_of"]
        except WorkStoreError as exc:
            return str(exc)

    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        results = list(pool.map(undo_once, range(8)))

    assert results.count("evt_racy_undo") == 1
    assert results.count("no_event_to_undo") == 7
    with sqlite3.connect(store.db_path) as conn:
        assert conn.execute(
            "SELECT COUNT(*) FROM work_events WHERE reversal_of='evt_racy_undo'"
        ).fetchone()[0] == 1


def test_archived_track_rejects_new_events_but_keeps_history(tmp_path):
    store = make_store(tmp_path)
    store.bootstrap()
    store.create_event(
        event_id="evt_before", track_id="job_hunt", event_type_id="job_hunt.application",
        value=1, occurred_at=time.time(), source="phone_manual", note=None, session_id="s1",
    )
    store.update_track("job_hunt", {"state": "archived"})
    with pytest.raises(WorkStoreError, match="track_archived"):
        store.create_event(
            event_id="evt_after", track_id="job_hunt", event_type_id="job_hunt.application",
            value=1, occurred_at=time.time(), source="phone_manual", note=None, session_id="s1",
        )
    start, end = local_day_bounds()
    events = store.events_for_range(start=start - 86400, end=end + 86400)
    assert any(e["event_id"] == "evt_before" for e in events)


def test_headline_count_only_includes_primary_event_type(tmp_path):
    from mazhost.work_service import WorkService

    store = make_store(tmp_path)
    store.bootstrap()
    now = time.time()
    store.create_event(
        event_id="evt_app", track_id="job_hunt", event_type_id="job_hunt.application",
        value=1, occurred_at=now, source="phone_manual", note=None, session_id="s1",
    )
    store.create_event(
        event_id="evt_followup", track_id="job_hunt", event_type_id="job_hunt.follow_up",
        value=1, occurred_at=now, source="phone_manual", note=None, session_id="s1",
    )
    summary = WorkService(store).summary(window="today")
    job_hunt = next(t for t in summary["tracks"] if t["track_id"] == "job_hunt")
    assert job_hunt["today_total"] == 1  # follow_up must not count toward headline


def test_dst_fallback_transition_counts_event_once_correct_local_day(tmp_path):
    # The repeated 01:30 after US/Eastern falls back still belongs to one
    # 25-hour local day, never the adjacent day.
    store = make_store(tmp_path)
    store.bootstrap()
    eastern = ZoneInfo("America/New_York")
    ambiguous_utc = datetime(2025, 11, 2, 1, 30, tzinfo=eastern, fold=1).timestamp()
    store.create_event(
        event_id="evt_dst", track_id="job_hunt", event_type_id="job_hunt.application",
        value=1, occurred_at=ambiguous_utc, source="phone_manual", note=None, session_id="s1",
    )
    start, end = local_day_bounds(when=ambiguous_utc, timezone=eastern)
    assert end - start == 25 * 60 * 60
    matches = store.events_for_range(start=start, end=end)
    assert sum(1 for e in matches if e["event_id"] == "evt_dst") == 1
    prev_start, _ = local_day_bounds(when=ambiguous_utc, days_ago=1, timezone=eastern)
    next_start, next_end = local_day_bounds(when=ambiguous_utc, days_ago=-1, timezone=eastern)
    outside = store.events_for_range(start=prev_start, end=start) + store.events_for_range(start=next_start, end=next_end)
    assert not any(e["event_id"] == "evt_dst" for e in outside)


def test_renamed_event_type_label_does_not_affect_historical_attribution(tmp_path):
    from mazhost.work_service import WorkService

    store = make_store(tmp_path)
    store.bootstrap()
    store.create_event(
        event_id="evt_app2", track_id="job_hunt", event_type_id="job_hunt.application",
        value=1, occurred_at=time.time(), source="phone_manual", note=None, session_id="s1",
    )
    store.update_track(
        "job_hunt",
        {},
        event_type_updates=[
            {
                "id": "job_hunt.application",
                "label": "SUBMITTED",
                "sort_order": 0,
                "active": True,
                "contributes_to_headline": True,
            }
        ],
    )
    summary = WorkService(store).summary(window="today")
    job_hunt = next(t for t in summary["tracks"] if t["track_id"] == "job_hunt")
    assert job_hunt["today_total"] == 1  # attribution keyed by id, survives the rename
    application = next(
        e for e in job_hunt["breakdown"] if e["event_type_id"] == "job_hunt.application"
    )
    assert application["label"] == "SUBMITTED"


def test_notes_are_bounded_and_sanitized_like_other_capture_text(tmp_path):
    store = make_store(tmp_path)
    store.bootstrap()
    event, _created = store.create_event(
        event_id="evt_note", track_id="job_hunt",
        event_type_id="job_hunt.application", value=1,
        occurred_at=time.time(), source="phone_manual",
        note="  first line\x00\nsecond line  ", session_id="s1",
    )
    assert event["note"] == "first line second line"
