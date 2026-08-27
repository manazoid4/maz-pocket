"""WORK schema and built-in template declarations.

Kept separate from ``work_store.py`` so the transactional store remains small
enough to audit. These are native MAZ Pocket definitions, not vendored code.
"""

from __future__ import annotations

import sqlite3
from dataclasses import dataclass
from typing import Callable

SCHEMA_VERSION = 2
MAX_NOTE_LENGTH = 500

CLIENT_PIPELINE_STAGES = (
    "RESEARCHED", "QUALIFIED", "REJECTED", "READY_TO_SEND", "SENT",
    "FOLLOW_UP_DUE", "WON", "LOST",
)
JOB_PIPELINE_STAGES = (
    "REVIEWED", "QUALIFIED", "SKIPPED", "READY_TO_APPLY", "APPLIED",
    "FOLLOW_UP_DUE", "INTERVIEW", "OFFER", "REJECTED", "WITHDRAWN",
)
PIPELINE_STAGES = {
    "client": frozenset(CLIENT_PIPELINE_STAGES),
    "job": frozenset(JOB_PIPELINE_STAGES),
}
PROOF_STAGES = frozenset(("NOT_STARTED", "IN_PROGRESS", "READY"))


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
        SeedEventType("job_hunt.follow_up", "FOLLOW_UP", 1),
        SeedEventType("job_hunt.interview", "INTERVIEW", 2),
        SeedEventType("job_hunt.rejection", "REJECTION", 3),
        SeedEventType("job_hunt.offer", "OFFER", 4),
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
    # MAZ WORKS shows raw total activity. Every event contributes equally;
    # no close rate, weighting, score, or other composite is derived.
    event_types=(
        SeedEventType("maz_works.outreach", "OUTREACH", 0, True),
        SeedEventType("maz_works.follow_up", "FOLLOW_UP", 1, True),
        SeedEventType("maz_works.demo_audit", "DEMO_AUDIT", 2, True),
        SeedEventType("maz_works.conversation", "CONVERSATION", 3, True),
        SeedEventType("maz_works.call_booked", "CALL_BOOKED", 4, True),
        SeedEventType("maz_works.proposal", "PROPOSAL", 5, True),
        SeedEventType("maz_works.client_won", "CLIENT_WON", 6, True),
    ),
)

BUILTIN_TEMPLATES = (JOB_HUNT, MAZ_WORKS)

SCHEMA_STATEMENTS = """
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
    return [statement.strip() for statement in script.split(";") if statement.strip()]


def _migration_v1(conn: sqlite3.Connection) -> None:
    # ``executescript`` implicitly commits. Individual statements keep the
    # whole migration inside WorkStore.bootstrap's explicit transaction.
    for statement in _split_statements(SCHEMA_STATEMENTS):
        conn.execute(statement)


def _migration_v2(conn: sqlite3.Connection) -> None:
    conn.execute(
        """CREATE TABLE pipeline_items (
            item_id TEXT PRIMARY KEY,
            pipeline TEXT NOT NULL CHECK (pipeline IN ('client', 'job')),
            title TEXT NOT NULL,
            organisation TEXT NOT NULL DEFAULT '',
            stage TEXT NOT NULL,
            score INTEGER CHECK (score IS NULL OR (score >= 0 AND score <= 100)),
            source_url TEXT NOT NULL DEFAULT '',
            evidence TEXT NOT NULL DEFAULT '',
            friction TEXT NOT NULL DEFAULT '',
            rationale TEXT NOT NULL DEFAULT '',
            solution TEXT NOT NULL DEFAULT '',
            contact_role TEXT NOT NULL DEFAULT '',
            outreach_draft TEXT NOT NULL DEFAULT '',
            outreach_status TEXT NOT NULL DEFAULT 'NOT_READY',
            proof_status TEXT NOT NULL DEFAULT 'NOT_STARTED'
                CHECK (proof_status IN ('NOT_STARTED', 'IN_PROGRESS', 'READY')),
            next_action TEXT NOT NULL DEFAULT '',
            due_at REAL,
            last_contact_at REAL,
            created_at REAL NOT NULL,
            updated_at REAL NOT NULL
        )"""
    )
    conn.execute("CREATE INDEX idx_pipeline_stage ON pipeline_items(pipeline, stage)")
    conn.execute("CREATE INDEX idx_pipeline_due ON pipeline_items(due_at)")


MIGRATIONS: tuple[tuple[int, Callable[[sqlite3.Connection], None]], ...] = (
    (1, _migration_v1),
    (2, _migration_v2),
)
