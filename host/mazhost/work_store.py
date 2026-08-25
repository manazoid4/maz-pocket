"""WORK Consistency SQLite store.

Tracks (JOB HUNT, MAZ WORKS, custom) and the events logged against them.
WAL mode, foreign keys on, schema versioned via ``PRAGMA user_version``.
Timestamps are stored as UTC epoch seconds; day-bucketing uses the host
machine's local timezone (no other timezone source exists elsewhere in
mazhost, so this store is the first and only one)."""

from __future__ import annotations

import sqlite3
import threading
import time
import uuid
from contextlib import contextmanager
from datetime import date, datetime, time as datetime_time, timedelta, tzinfo
from pathlib import Path
from typing import Any, Iterator

from .work_schema import (
    BUILTIN_TEMPLATES,
    MAX_NOTE_LENGTH,
    MIGRATIONS,
    SCHEMA_VERSION,
)


_BOOTSTRAP_LOCK = threading.RLock()


class WorkStoreError(RuntimeError):
    pass


class CorruptDatabaseError(WorkStoreError):
    """Raised instead of silently returning zero/empty data for a bad file."""


class WorkStore:
    def __init__(self, root: str | Path) -> None:
        self.root = Path(root).expanduser()
        self.root.mkdir(parents=True, exist_ok=True)
        self.db_path = self.root / "work.sqlite3"

    @contextmanager
    def _connect(self) -> Iterator[sqlite3.Connection]:
        conn: sqlite3.Connection | None = None
        try:
            conn = sqlite3.connect(self.db_path, timeout=10, isolation_level=None)
            conn.row_factory = sqlite3.Row
            conn.execute("PRAGMA busy_timeout=10000")
            conn.execute("PRAGMA journal_mode=WAL")
            conn.execute("PRAGMA foreign_keys=ON")
            integrity = [row[0] for row in conn.execute("PRAGMA integrity_check").fetchall()]
            if integrity != ["ok"]:
                raise CorruptDatabaseError(
                    "work database integrity check failed: " + "; ".join(map(str, integrity))
                )
        except CorruptDatabaseError:
            if conn is not None:
                conn.close()
            raise
        except sqlite3.DatabaseError as error:
            if conn is not None:
                conn.close()
            raise CorruptDatabaseError(f"work database unreadable: {error}") from error
        try:
            assert conn is not None
            yield conn
        finally:
            conn.close()

    # ------------------------------------------------------------ bootstrap
    def bootstrap(self) -> None:
        """Idempotent: safe on every Core boot, safe to call repeatedly."""
        # The host can construct the phone and API apps concurrently during a
        # fresh start. Serialize same-process bootstrap while SQLite's own
        # BEGIN IMMEDIATE + busy timeout protects independent processes.
        with _BOOTSTRAP_LOCK:
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
            now = time.time()
            conn.execute("BEGIN IMMEDIATE")
            try:
                existing = conn.execute(
                    "SELECT id FROM tracks WHERE id = ?", (template.id,)
                ).fetchone()
                if existing:
                    conn.execute("COMMIT")
                    continue  # never overwrite a user-edited seeded track
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

    def update_track(
        self,
        track_id: str,
        fields: dict[str, Any],
        *,
        event_type_updates: list[dict[str, Any]] | None = None,
    ) -> dict[str, Any]:
        allowed = {
            "name", "short_label", "target", "cadence", "pinned",
            "sort_order", "state", "primary_event_type_id",
        }
        sets = {k: v for k, v in fields.items() if k in allowed}
        if not sets and not event_type_updates:
            existing = self.get_track(track_id)
            if not existing:
                raise WorkStoreError("track_not_found")
            return existing
        with self._connect() as conn:
            conn.execute("BEGIN IMMEDIATE")
            try:
                if not conn.execute(
                    "SELECT 1 FROM tracks WHERE id = ?", (track_id,)
                ).fetchone():
                    raise WorkStoreError("track_not_found")
                if sets:
                    sets["updated_at"] = time.time()
                    assignments = ", ".join(f"{key} = ?" for key in sets)
                    conn.execute(
                        f"UPDATE tracks SET {assignments} WHERE id = ?",
                        (*sets.values(), track_id),
                    )
                for update in event_type_updates or []:
                    cursor = conn.execute(
                        "UPDATE event_types SET label=?, sort_order=?, active=?, "
                        "contributes_to_headline=? WHERE id=? AND track_id=?",
                        (
                            update["label"], update["sort_order"],
                            1 if update["active"] else 0,
                            1 if update["contributes_to_headline"] else 0,
                            update["id"], track_id,
                        ),
                    )
                    if cursor.rowcount != 1:
                        raise WorkStoreError("event_type_not_found")
                primary = sets.get("primary_event_type_id")
                if primary and not conn.execute(
                    "SELECT 1 FROM event_types WHERE id=? AND track_id=?",
                    (primary, track_id),
                ).fetchone():
                    raise WorkStoreError("primary_event_type_not_found")
                conn.execute("COMMIT")
            except WorkStoreError:
                if conn.in_transaction:
                    conn.execute("ROLLBACK")
                raise
            except Exception:
                if conn.in_transaction:
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
        note = " ".join((note or "").replace("\x00", "").splitlines()).strip()
        note = note[:MAX_NOTE_LENGTH] or None
        with self._connect() as conn:
            conn.execute("BEGIN IMMEDIATE")
            try:
                existing = conn.execute(
                    "SELECT * FROM work_events WHERE event_id = ?", (event_id,)
                ).fetchone()
                if existing:
                    conn.execute("COMMIT")
                    return dict(existing), False

                track = conn.execute(
                    "SELECT * FROM tracks WHERE id = ?", (track_id,)
                ).fetchone()
                if not track:
                    raise WorkStoreError("track_not_found")
                if track["state"] == "archived":
                    raise WorkStoreError("track_archived")
                event_type = conn.execute(
                    "SELECT * FROM event_types WHERE id = ? AND track_id = ? AND active = 1",
                    (event_type_id, track_id),
                ).fetchone()
                if not event_type:
                    raise WorkStoreError("event_type_not_found")

                now = time.time()
                conn.execute(
                    "INSERT INTO work_events (event_id, track_id, event_type_id, value, "
                    "occurred_at, source, note, session_id, created_at, reversal_of, reversed) "
                    "VALUES (?,?,?,?,?,?,?,?,?,NULL,0)",
                    (event_id, track_id, event_type_id, value, occurred_at, source, note,
                     session_id, now),
                )
                conn.execute("COMMIT")
            except WorkStoreError:
                if conn.in_transaction:
                    conn.execute("ROLLBACK")
                raise
            except Exception:
                if conn.in_transaction:
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
            conn.execute("BEGIN IMMEDIATE")
            try:
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
                reversal_id = "undo_" + uuid.uuid4().hex
                conn.execute(
                    "UPDATE work_events SET reversed = 1 WHERE event_id = ? AND reversed = 0",
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
            except WorkStoreError:
                if conn.in_transaction:
                    conn.execute("ROLLBACK")
                raise
            except Exception:
                if conn.in_transaction:
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


def local_day_bounds(
    when: float | None = None,
    days_ago: int = 0,
    *,
    timezone: tzinfo | None = None,
) -> tuple[float, float]:
    """UTC epoch [start, end) for the local calendar day, `days_ago` days back."""
    instant = when if when is not None else time.time()
    if timezone is not None:
        local_date = datetime.fromtimestamp(instant, timezone).date() - timedelta(days=days_ago)
        start = datetime.combine(local_date, datetime_time.min, timezone).timestamp()
        end = datetime.combine(local_date + timedelta(days=1), datetime_time.min, timezone).timestamp()
        return start, end

    # ``datetime.astimezone().tzinfo`` can be a fixed-offset snapshot on
    # Windows. Build each local midnight through mktime so the operating
    # system applies the correct DST rule independently to both boundaries.
    local_date = date.fromtimestamp(instant) - timedelta(days=days_ago)
    next_date = local_date + timedelta(days=1)
    start = time.mktime((local_date.year, local_date.month, local_date.day, 0, 0, 0, -1, -1, -1))
    end = time.mktime((next_date.year, next_date.month, next_date.day, 0, 0, 0, -1, -1, -1))
    return start, end
