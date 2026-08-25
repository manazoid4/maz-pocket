from __future__ import annotations

import sqlite3
import time

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

    calls = {"n": 0}
    real_v1 = MIGRATIONS[0][1]

    def flaky_v1(conn):
        calls["n"] += 1
        if calls["n"] == 1:
            conn.execute(
                "CREATE TABLE tracks (id TEXT PRIMARY KEY, name TEXT NOT NULL, "
                "short_label TEXT NOT NULL, mode TEXT NOT NULL, unit TEXT NOT NULL, "
                "cadence TEXT NOT NULL DEFAULT 'daily', target REAL, "
                "primary_event_type_id TEXT, pinned INTEGER NOT NULL DEFAULT 0, "
                "sort_order INTEGER NOT NULL DEFAULT 0, state TEXT NOT NULL DEFAULT 'active', "
                "created_at REAL NOT NULL, updated_at REAL NOT NULL)"
            )
            raise RuntimeError("simulated crash mid-migration")
        real_v1(conn)

    monkeypatch.setattr("mazhost.work_store.MIGRATIONS", ((1, flaky_v1),))
    with pytest.raises(RuntimeError):
        store.bootstrap()

    with sqlite3.connect(store.db_path) as conn:
        version = conn.execute("PRAGMA user_version").fetchone()[0]
    assert version == 0  # rolled back, never partially applied

    monkeypatch.setattr("mazhost.work_store.MIGRATIONS", MIGRATIONS)
    store.bootstrap()  # resumes and completes cleanly
    store.bootstrap()  # idempotent — no double-apply
    with sqlite3.connect(store.db_path) as conn:
        version = conn.execute("PRAGMA user_version").fetchone()[0]
    assert version == 1
    assert {t["id"] for t in store.list_tracks()} == {"job_hunt", "maz_works"}


class _FlakyConnWrapper:
    """Wraps a real sqlite3.Connection; sqlite3.Connection is an immutable C
    type so its bound methods cannot be monkeypatched directly."""

    def __init__(self, real, calls):
        self._real = real
        self._calls = calls

    def execute(self, sql, *args, **kwargs):
        if "INSERT INTO work_events" in sql:
            self._calls["n"] += 1
            if self._calls["n"] == 1:
                raise sqlite3.OperationalError("simulated crash mid-transaction")
        return self._real.execute(sql, *args, **kwargs)

    def __getattr__(self, name):
        return getattr(self._real, name)

    def __setattr__(self, name, value):
        if name in ("_real", "_calls"):
            object.__setattr__(self, name, value)
        else:
            setattr(self._real, name, value)


def test_write_crash_mid_transaction_leaves_no_partial_row(tmp_path, monkeypatch):
    import mazhost.work_store as work_store_module

    store = make_store(tmp_path)
    store.bootstrap()

    calls = {"n": 0}
    real_connect = work_store_module.sqlite3.connect

    def flaky_connect(*args, **kwargs):
        return _FlakyConnWrapper(real_connect(*args, **kwargs), calls)

    monkeypatch.setattr(work_store_module.sqlite3, "connect", flaky_connect)
    with pytest.raises(sqlite3.OperationalError):
        store.create_event(
            event_id="evt_crash", track_id="job_hunt", event_type_id="job_hunt.application",
            value=1, occurred_at=time.time(), source="phone_manual", note=None, session_id="s1",
        )
    monkeypatch.setattr(work_store_module.sqlite3, "connect", real_connect)
    with sqlite3.connect(store.db_path) as conn:
        count = conn.execute("SELECT COUNT(*) FROM work_events WHERE event_id = ?", ("evt_crash",)).fetchone()[0]
    assert count == 0


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
    # 2025-11-02 02:30 US/Eastern (fallback repeats the 01:00-02:00 hour) —
    # the event still lands in exactly one local calendar day, never both.
    store = make_store(tmp_path)
    store.bootstrap()
    ambiguous_utc = 1762061400.0  # 2025-11-02 06:30 UTC
    store.create_event(
        event_id="evt_dst", track_id="job_hunt", event_type_id="job_hunt.application",
        value=1, occurred_at=ambiguous_utc, source="phone_manual", note=None, session_id="s1",
    )
    start, end = local_day_bounds(when=ambiguous_utc)
    matches = store.events_for_range(start=start, end=end)
    assert sum(1 for e in matches if e["event_id"] == "evt_dst") == 1
    prev_start, _ = local_day_bounds(when=ambiguous_utc, days_ago=1)
    next_start, next_end = local_day_bounds(when=ambiguous_utc, days_ago=-1)
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
    with sqlite3.connect(store.db_path) as conn:
        conn.execute(
            "UPDATE event_types SET label = 'SUBMITTED' WHERE id = 'job_hunt.application'"
        )
        conn.commit()
    summary = WorkService(store).summary(window="today")
    job_hunt = next(t for t in summary["tracks"] if t["track_id"] == "job_hunt")
    assert job_hunt["today_total"] == 1  # attribution keyed by id, survives the rename
