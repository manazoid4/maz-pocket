# MAZ Pocket v0.9 feature architecture

This document is the controlling scope and placement decision for v0.9.0.
The current repository is v0.8.0 CONTROL. v0.9 must solve a daily usefulness
problem before it expands into deeper analytics or configuration management.

## Release promise

Ship one complete loop extremely well:

> Open the authenticated phone UI, log meaningful progress in a few seconds,
> and glance at MAZ Pocket to know whether the important ongoing work is moving.

The first two built-in tracks are:

1. **JOB HUNT** — primary metric: job applications submitted.
2. **MAZ WORKS** — primary metric: client-acquisition actions, with a useful
   breakdown such as outreach, follow-up, demo/audit, conversation, booked
   call, proposal and client won.

The owner can create, edit, reorder, pin, pause and archive additional ongoing
tracks from the web UI without changing firmware or code.

## Product principles

- **Web-first management.** The Cardputer is too small for track creation and
  editing. All configuration must work comfortably from the authenticated
  phone web UI.
- **Fast logging.** A normal progress event should take one or two taps after
  opening WORK. Do not require a form for a plain `+1`.
- **Explicit beats inferred.** A submitted application is stronger evidence
  than browser time. A sent proposal is stronger evidence than token volume.
- **No universal productivity score.** Keep goal progress, agent activity,
  focus time and AI usage as separate facts.
- **Truthful history.** Store events, not mutable daily totals. Daily/weekly
  numbers are derived from immutable event history and can be rebuilt.
- **Local-first.** Work data lives in MAZ Core on the owner's machine. No cloud
  account is required.
- **Six Home surfaces remain.** Do not add a seventh tile or break stable IDs.

## Final information architecture

### Cardputer

```text
CALL      CAPTURE      AGENTS
CONTROL   MEMORY       WORK
```

The stable Home IDs remain `talk`, `capturehub`, `agents`, `desk`, `recall`
and `flow`. The visible `flow` label becomes **WORK**; the internal ID remains
unchanged for shortcut and compatibility safety.

WORK opens directly on a glance view, not a tools menu. At minimum it shows:

- JOB HUNT: today count / target;
- MAZ WORKS: today count / target;
- the next one or two pinned custom tracks if space allows;
- a compact 7-day consistency indicator or aggregate;
- stale/offline state when MAZ Core cannot be reached.

Existing Focus Timer, Work Sprint, Tasks, Reminders and Shift Clock remain
reachable through a secondary WORK tools action. Do not delete them.

### Authenticated phone web UI

The existing `/control/` phone application becomes the primary management
surface. It should be reorganized into focused tabs rather than one long page:

```text
WORK | AUTHORITY
```

WORK is the default tab. AUTHORITY preserves the existing phone approval,
grants, manual session and audit behavior. `REVOKE ALL CONTROL` remains easy
to reach from AUTHORITY and is not weakened by this feature.

The WORK tab contains:

1. **Today** — pinned track cards with large progress values and large quick
   action buttons.
2. **Quick log** — common event-type buttons. One tap logs one event.
3. **Undo last** — reversible accidental logging without deleting arbitrary
   history.
4. **7-day view** — small, phone-readable history; no desktop analytics suite.
5. **Manage tracks** — create/edit/reorder/pin/pause/archive.
6. **Track detail** — event breakdown and recent history.

`mazpocket.local` remains a lightweight firmware portal. It should expose a
clear **OPEN WORK** handoff to the authenticated Core phone UI, not duplicate
the tracker implementation in firmware HTML.

## Track model

A Track is an ongoing objective, not a one-off task. It supports three simple
modes so the feature is genuinely reusable without becoming a project manager:

- `count` — e.g. applications, outreaches, workouts;
- `time` — e.g. 60 minutes of portfolio work;
- `checkin` — e.g. did the important thing today?

Each track contains:

- stable ID;
- name and short label;
- mode;
- unit label;
- cadence: daily, weekly or none;
- optional target value;
- ordered event types;
- one primary event type for the headline count when applicable;
- pinned/order state;
- active/paused/archived state;
- created/updated timestamps.

Each Work Event contains:

- stable event ID;
- track ID;
- event type ID;
- quantity/value;
- occurred-at timestamp;
- source (`phone_manual`, `cardputer_manual`, `import`, etc.);
- optional short note;
- created-at timestamp;
- optional reversal reference instead of destructive deletion.

Store timestamps in UTC and aggregate by the configured/system local timezone.
DST day boundaries must be correct. Add a timezone setting only if the existing
host settings do not already provide a reliable local timezone.

## Built-in templates

### JOB HUNT

Mode: `count`.

Suggested event types:

- **APPLICATION** — contributes to the primary headline count;
- FOLLOW-UP;
- INTERVIEW;
- REJECTION;
- OFFER.

The headline is applications submitted, not the sum of every event.

### MAZ WORKS

Mode: `count`.

Suggested event types:

- OUTREACH;
- FOLLOW-UP;
- DEMO / AUDIT;
- CONVERSATION;
- CALL BOOKED;
- PROPOSAL;
- CLIENT WON.

The headline is meaningful acquisition actions. The detail view must still
show the breakdown so seven low-value actions cannot masquerade as seven
clients or proposals.

Targets are editable. Do not hard-code a moral judgement such as "you failed"
when a target is missed. Show progress and history plainly.

## Host architecture

Heavy persistence and aggregation live in MAZ Core. Do not extend the ESP32
storage TSV format into a second analytics database.

Prefer cohesive modules such as:

- `host/mazhost/work_store.py` — SQLite schema, migrations and event writes;
- `host/mazhost/work_service.py` — templates, aggregation, target logic and
  compact summaries;
- `host/mazhost/work_routes.py` — authenticated Core API;
- phone UI integration in `phone_control.py` or a small adjacent module if the
  file is becoming unwieldy.

Use Python stdlib `sqlite3` unless repository evidence justifies another
dependency. Store the database beneath `~/.maz-pocket/work/`. Use WAL, foreign
keys, bounded queries and schema versioning.

Do not store prompts, completions, screenshots, browser history or credentials
for this feature.

## Core API contract

Exact naming may adapt to existing conventions, but v0.9 needs these behaviors:

- `GET /work/summary?window=today` — compact pinned-track progress, freshness
  and seven-day aggregate;
- `GET /work/tracks` — active/manageable track definitions;
- `POST /work/tracks` — create a custom track;
- `PATCH /work/tracks/{id}` or equivalent structured update;
- `POST /work/events` — append one progress event;
- `POST /work/events/{id}/undo` — append a reversal/tombstone safely;
- `GET /work/history?days=7&track_id=...` — bounded history.

Cardputer reads must use compact payloads. Web mutations must be authenticated.
A missing/corrupt individual track must not crash the entire WORK summary.

## Existing feature integration

v0.9 may surface existing data, but must not let integration work block the
core consistency loop.

- Existing Tasks currently store `done`, `created`, `bucket` and text. Do not
  pretend that this is sufficient historical completion telemetry. If Tasks
  feed WORK, add an explicit completion timestamp/event path first.
- Existing Focus runs in-memory on the Cardputer. Do not claim seven-day focus
  history until completed/cancelled sessions are explicitly persisted/synced.
- Agent Nudge state can appear as a secondary card or detail, but it does not
  increment JOB HUNT or MAZ WORKS.
- Codex/Claude token telemetry and API Value from the earlier v0.9 proposal are
  useful follow-ups, not blockers for this release.

## What v0.9 explicitly defers

- MCP Fix & Activate and multi-client MCP mutation work;
- deep Codex/Claude/OpenCode/Hermes telemetry ingestion;
- universal productivity scoring;
- automatic desktop activity surveillance;
- cloud sync/accounts;
- full project-management boards, subtasks and dependencies;
- complex charts/heatmaps;
- editing tracks on the Cardputer;
- a complete rewrite of the firmware portal.

Those ideas may return in v0.9.x after the owner has used the consistency loop
for a week and the data model has proved useful.

## Success tests

v0.9 earns continuation when all of these are true:

1. From a phone, an application can be logged in <=2 taps from the WORK tab.
2. From a phone, a Maz Works outreach/follow-up/proposal can be logged in <=2
   taps from the relevant track card.
3. A custom ongoing track can be created without touching code or firmware.
4. Reloading/restarting MAZ Core does not lose or double-count progress.
5. Undoing an accidental log is safe and auditable.
6. The Cardputer WORK screen shows useful current progress without requiring
   track-management interaction on the 240x135 display.
7. Existing CALL/CAPTURE/AGENTS/CONTROL/MEMORY functions and stable IDs remain
   intact.
8. Full host tests, firmware build, version/release guards and the physical
   Cardputer verification gate pass before a release is published.
