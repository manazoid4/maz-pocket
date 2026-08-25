"""WORK Consistency SQLite store.

Tracks (JOB HUNT, MAZ WORKS, custom) and the events logged against them.
WAL mode, foreign keys on, schema versioned via ``PRAGMA user_version``.
Timestamps are stored as UTC epoch seconds; day-bucketing uses the host
machine's local timezone (no other timezone source exists elsewhere in
mazhost, so this store is the first and only one)."""

from __future__ import annotations

import sqlite3
import time
from contextlib import contextmanager
from dataclasses import dataclass
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any, Callable, Iterator

SCHEMA_VERSION = 1


class WorkStoreError(RuntimeError):
    pass


class CorruptDatabaseError(WorkStoreError):
    """Raised instead of silently returning zero/empty data for a bad file."""


@dataclass(frozen=True)
class SeedEventType:
    id: str
    label: str
    sort_order: int
    contributes_to_headline: bool = False


@dataclass(frozen=True)
class SeedTrack:
    id: str
    name: str
    short_label: str
    mode: str
    unit: str
    primary_event_type_id: str
    pinned: bool
    sort_order: int
    event_types: tuple[SeedEventType, ...]


JOB_HUNT = SeedTrack(
    id="job_hunt",
    name="JOB HUNT",
    short_label="JOB HUNT",
    mode="count",
    unit="applications",
    primary_event_type_id="job_hunt.application",
    pinned=True,
    sort_order=0,
    event_types=(
        SeedEventType("job_hunt.application", "APPLICATION", 0, True),
        SeedEventType("job_hunt.follow_up", "FOLLOW_UP", 1, False),
        SeedEventType("job_hunt.interview", "INTERVIEW", 2, False),
        SeedEventType("job_hunt.rejection", "REJECTION", 3, False),
        SeedEventType("job_hunt.offer", "OFFER", 4, False),
    ),
)

MAZ_WORKS = SeedTrack(
    id="maz_works",
    name="MAZ WORKS",
    short_label="MAZ WORKS",
    mode="count",
    unit="events",
    primary_event_type_id="maz_works.outreach",
    pinned=True,
    sort_order=1,
    event_types=(
        SeedEventType("maz_works.outreach", "OUTREACH", 0, True),
        SeedEventType("maz_works.follow_up", "FOLLOW_UP", 1, False),
        SeedEventType("maz_works.demo_audit", "DEMO_AUDIT", 2, False),
        SeedEventType("maz_works.conversation", "CONVERSATION", 3, False),
        SeedEventType("maz_works.call_booked", "CALL_BOOKED", 4, False),
        SeedEventType("maz_works.proposal", "PROPOSAL", 5, False),
        SeedEventType("maz_works.client_won", "CLIENT_WON", 6, False),
    ),
)

BUILTIN_TEMPLATES = (JOB_HUNT, MAZ_WORKS)

MAX_NOTE_LENGTH = 500

_SCHEMA_STATEMENTS = """
CREATE TABLE tracks (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    short_label TEXT NOT NULL,
    mode TEXT NOT NULL CHECK (mode IN ('count', 'time', 'checkin')),
    unit TEXT NOT NULL,
    cadence TEXT NOT NULL DEFAULT 'daily' CHECK (cadence IN ('daily', 'weekly', 'none')),
    target REAL,
    primary_event_type_id TEXT,
    pinned INTEGER NOT NULL DEFAULT 0,
    sort_order INTEGER NOT NULL DEFAULT 0,
    state TEXT NOT NULL DEFAULT 'active' CHECK (state IN ('active', 'paused', 'archived')),
    created_at REAL NOT NULL,
    updated_at REAL NOT NULL
);

CREATE TABLE event_types (
    id TEXT PRIMARY KEY,
    track_id TEXT NOT NULL REFERENCES tracks(id),
    label TEXT NOT NULL,
    sort_order INTEGER NOT NULL DEFAULT 0,
    contributes_to_headline INTEGER NOT NULL DEFAULT 0,
    active INTEGER NOT NULL DEFAULT 1
);
CREATE INDEX idx_event_types_track ON event_types(track_id);

CREATE TABLE work_events (
    event_id TEXT PRIMARY KEY,
    track_id TEXT NOT NULL REFERENCES tracks(id),
    event_type_id TEXT NOT NULL REFERENCES event_types(id),
    value REAL NOT NULL DEFAULT 1,
    occurred_at REAL NOT NULL,
    source TEXT NOT NULL CHECK (source IN ('phone_manual', 'cardputer_manual', 'import')),
    note TEXT,
    session_id TEXT,
    created_at REAL NOT NULL,
    reversal_of TEXT REFERENCES work_events(event_id),
    reversed INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX idx_events_track_time ON work_events(track_id, occurred_at);
CREATE INDEX idx_events_session_time ON work_events(session_id, created_at);
"""


def _split_statements(script: str) -> list[str]:
    return [s.strip() for s in script.split(";") if s.strip()]


def _migration_v1(conn: sqlite3.Connection) -> None:
    # Individual execute() calls (not executescript(), which implicitly
    # commits) so the whole migration stays inside the caller's transaction
    # and a mid-step crash leaves nothing half-applied outside it.
    for statement in _split_statements(_SCHEMA_STATEMENTS):
        conn.execute(statement)


MIGRATIONS: tuple[tuple[int, Callable[[sqlite3.Connection], None]], ...] = (
    (1, _migration_v1),
)


class WorkStore:
    def __init__(self, root: str | Path) -> None:
        self.root = Path(root).expanduser()
        self.root.mkdir(parents=True, exist_ok=True)
        self.db_path = self.root / "work.sqlite3"

    @contextmanager
    def _connect(self) -> Iterator[sqlite3.Connection]:
        try:
            conn = sqlite3.connect(self.db_path, timeout=10, isolation_level=None)
            conn.row_factory = sqlite3.Row
            conn.execute("PRAGMA journal_mode=WAL")
            conn.execute("PRAGMA foreign_keys=ON")
            conn.execute("PRAGMA integrity_check")
        except sqlite3.DatabaseError as error:
            raise CorruptDatabaseError(f"work database unreadable: {error}") from error
        try:
            yield conn
        finally:
            conn.close()

    # ------------------------------------------------------------ bootstrap
    def bootstrap(self) -> None:
        """Idempotent: safe on every Core boot, safe to call repeatedly."""
        with self._connect() as conn:
            current = conn.execute("PRAGMA user_version").fetchone()[0]
            for target_version, migrate in MIGRATIONS:
                if current >= target_version:
                    continue
                conn.execute("BEGIN IMMEDIATE")
                try:
                    migrate(conn)
                    conn.execute(f"PRAGMA user_version = {target_version}")
                    conn.execute("COMMIT")
                    current = target_version
                except Exception:
                    conn.execute("ROLLBACK")
                    raise
            self._seed_templates(conn)

    def _seed_templates(self, conn: sqlite3.Connection) -> None:
        for template in BUILTIN_TEMPLATES:
            existing = conn.execute(
                "SELECT id FROM tracks WHERE id = ?", (template.id,)
            ).fetchone()
            if existing:
                continue  # never overwrite a user-edited seeded track
            now = time.time()
            conn.execute("BEGIN IMMEDIATE")
            try:
                conn.execute(
                    "INSERT INTO tracks (id, name, short_label, mode, unit, cadence, "
                    "target, primary_event_type_id, pinned, sort_order, state, "
                    "created_at, updated_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                    (
                        template.id, template.name, template.short_label, template.mode,
                        template.unit, "daily", None, template.primary_event_type_id,
                        1 if template.pinned else 0, template.sort_order, "active", now, now,
                    ),
                )
                for et in template.event_types:
                    conn.execute(
                        "INSERT INTO event_types (id, track_id, label, sort_order, "
                        "contributes_to_headline, active) VALUES (?,?,?,?,?,1)",
                        (et.id, template.id, et.label, et.sort_order, 1 if et.contributes_to_headline else 0),
                    )
                conn.execute("COMMIT")
            except Exception:
                conn.execute("ROLLBACK")
                raise

    # ---------------------------------------------------------------- tracks
    def list_tracks(self, *, include_archived: bool = True) -> list[dict[str, Any]]:
        with self._connect() as conn:
            rows = conn.execute(
                "SELECT * FROM tracks ORDER BY sort_order, created_at"
            ).fetchall()
            tracks = [dict(r) for r in rows]
            if not include_archived:
                tracks = [t for t in tracks if t["state"] != "archived"]
            for track in tracks:
                et_rows = conn.execute(
                    "SELECT * FROM event_types WHERE track_id = ? ORDER BY sort_order",
                    (track["id"],),
                ).fetchall()
                track["event_types"] = [dict(e) for e in et_rows]
            return tracks

    def get_track(self, track_id: str) -> dict[str, Any] | None:
        with self._connect() as conn:
            row = conn.execute("SELECT * FROM tracks WHERE id = ?", (track_id,)).fetchone()
            if not row:
                return None
            track = dict(row)
            et_rows = conn.execute(
                "SELECT * FROM event_types WHERE track_id = ? ORDER BY sort_order", (track_id,)
            ).fetchall()
            track["event_types"] = [dict(e) for e in et_rows]
            return track

    def create_track(
        self,
        *,
        track_id: str,
        name: str,
        short_label: str,
        mode: str,
        unit: str,
        cadence: str,
        target: float | None,
        event_types: list[tuple[str, str, bool]],
        primary_event_type_id: str,
        pinned: bool,
        sort_order: int,
    ) -> dict[str, Any]:
        now = time.time()
        with self._connect() as conn:
            conn.execute("BEGIN IMMEDIATE")
            try:
                conn.execute(
                    "INSERT INTO tracks (id, name, short_label, mode, unit, cadence, target, "
                    "primary_event_type_id, pinned, sort_order, state, created_at, updated_at) "
                    "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                    (track_id, name, short_label, mode, unit, cadence, target,
                     primary_event_type_id, 1 if pinned else 0, sort_order, "active", now, now),
                )
                for idx, (et_id, label, contributes) in enumerate(event_types):
                    conn.execute(
                        "INSERT INTO event_types (id, track_id, label, sort_order, "
                        "contributes_to_headline, active) VALUES (?,?,?,?,?,1)",
                        (et_id, track_id, label, idx, 1 if contributes else 0),
                    )
                conn.execute("COMMIT")
            except Exception:
                conn.execute("ROLLBACK")
                raise
        return self.get_track(track_id)  # type: ignore[return-value]

    def update_track(self, track_id: str, fields: dict[str, Any]) -> dict[str, Any]:
        allowed = {"name", "short_label", "target", "cadence", "pinned", "sort_order", "state"}
        sets = {k: v for k, v in fields.items() if k in allowed}
        if not sets:
            existing = self.get_track(track_id)
            if not existing:
                raise WorkStoreError("track_not_found")
            return existing
        sets["updated_at"] = time.time()
        assignments = ", ".join(f"{k} = ?" for k in sets)
        with self._connect() as conn:
            conn.execute("BEGIN IMMEDIATE")
            try:
                cursor = conn.execute(
                    f"UPDATE tracks SET {assignments} WHERE id = ?",
                    (*sets.values(), track_id),
                )
                if cursor.rowcount == 0:
                    conn.execute("ROLLBACK")
                    raise WorkStoreError("track_not_found")
                conn.execute("COMMIT")
            except WorkStoreError:
                raise
            except Exception:
                conn.execute("ROLLBACK")
                raise
        return self.get_track(track_id)  # type: ignore[return-value]

    # ---------------------------------------------------------------- events
    def create_event(
        self,
        *,
        event_id: str,
        track_id: str,
        event_type_id: str,
        value: float,
        occurred_at: float,
        source: str,
        note: str | None,
        session_id: str,
    ) -> tuple[dict[str, Any], bool]:
        """Returns (event, created). Idempotent on event_id."""
        note = (note or "")[:MAX_NOTE_LENGTH] or None
        with self._connect() as conn:
            existing = conn.execute(
                "SELECT * FROM work_events WHERE event_id = ?", (event_id,)
            ).fetchone()
            if existing:
                return dict(existing), False

            track = conn.execute("SELECT * FROM tracks WHERE id = ?", (track_id,)).fetchone()
            if not track:
                raise WorkStoreError("track_not_found")
            if track["state"] == "archived":
                raise WorkStoreError("track_archived")
            event_type = conn.execute(
                "SELECT * FROM event_types WHERE id = ? AND track_id = ?",
                (event_type_id, track_id),
            ).fetchone()
            if not event_type:
                raise WorkStoreError("event_type_not_found")

            now = time.time()
            conn.execute("BEGIN IMMEDIATE")
            try:
                conn.execute(
                    "INSERT INTO work_events (event_id, track_id, event_type_id, value, "
                    "occurred_at, source, note, session_id, created_at, reversal_of, reversed) "
                    "VALUES (?,?,?,?,?,?,?,?,?,NULL,0)",
                    (event_id, track_id, event_type_id, value, occurred_at, source, note,
                     session_id, now),
                )
                conn.execute("COMMIT")
            except Exception:
                conn.execute("ROLLBACK")
                raise
            row = conn.execute(
                "SELECT * FROM work_events WHERE event_id = ?", (event_id,)
            ).fetchone()
            return dict(row), True

    def undo_last(self, *, session_id: str, expected_event_id: str | None = None) -> dict[str, Any]:
        """Reverses the current session's most recent non-reversed event.

        If `expected_event_id` is given (the client's last-known event id), it
        must match the actual latest event or the undo is refused — guards
        against a stale phone UI undoing a different event than the user saw.
        """
        with self._connect() as conn:
            row = conn.execute(
                "SELECT * FROM work_events WHERE session_id = ? AND reversed = 0 "
                "AND reversal_of IS NULL ORDER BY created_at DESC LIMIT 1",
                (session_id,),
            ).fetchone()
            if not row:
                raise WorkStoreError("no_event_to_undo")
            target = dict(row)
            if expected_event_id and target["event_id"] != expected_event_id:
                raise WorkStoreError("stale_undo_target")
            now = time.time()
            reversal_id = f"undo_{target['event_id']}_{int(now * 1000)}"
            conn.execute("BEGIN IMMEDIATE")
            try:
                conn.execute(
                    "UPDATE work_events SET reversed = 1 WHERE event_id = ?",
                    (target["event_id"],),
                )
                conn.execute(
                    "INSERT INTO work_events (event_id, track_id, event_type_id, value, "
                    "occurred_at, source, note, session_id, created_at, reversal_of, reversed) "
                    "VALUES (?,?,?,?,?,?,?,?,?,?,0)",
                    (reversal_id, target["track_id"], target["event_type_id"], -target["value"],
                     now, target["source"], "undo", session_id, now, target["event_id"]),
                )
                conn.execute("COMMIT")
            except Exception:
                conn.execute("ROLLBACK")
                raise
            return dict(conn.execute(
                "SELECT * FROM work_events WHERE event_id = ?", (reversal_id,)
            ).fetchone())

    # ------------------------------------------------------------ aggregation
    def events_for_range(self, *, start: float, end: float) -> list[dict[str, Any]]:
        # Excludes both a reversed original event AND its own reversal row —
        # a successful undo nets to zero contribution, not -1 (the reversal
        # row is bookkeeping, not a displayed event in its own right).
        with self._connect() as conn:
            rows = conn.execute(
                "SELECT * FROM work_events WHERE occurred_at >= ? AND occurred_at < ? "
                "AND reversed = 0 AND reversal_of IS NULL ORDER BY occurred_at",
                (start, end),
            ).fetchall()
            return [dict(r) for r in rows]


def local_day_bounds(when: float | None = None, days_ago: int = 0) -> tuple[float, float]:
    """UTC epoch [start, end) for the local calendar day, `days_ago` days back."""
    local = datetime.fromtimestamp(when if when is not None else time.time()).astimezone()
    day = (local - timedelta(days=days_ago)).replace(hour=0, minute=0, second=0, microsecond=0)
    start = day.timestamp()
    end = (day + timedelta(days=1)).timestamp()
    return start, end
