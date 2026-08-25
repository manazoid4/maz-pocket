# MAZ Pocket v1.0.0 feature architecture

This is the controlling product and architecture decision for MAZ Pocket v1.0.0.
The current shipped baseline is v0.8.0 CONTROL. v1 is the consolidation release:
it must make MAZ Pocket more useful every day, preserve the working control/agent
features, and leave one frictionless install/update path rather than another
partially-finished branch.

## v1 release promise

Ship one complete daily loop extremely well:

> Open the authenticated phone UI, log meaningful progress in a few seconds,
> and glance at MAZ Pocket to know whether the important ongoing work is moving.

The first built-in tracks are:

1. **JOB HUNT** — headline metric: job applications submitted.
2. **MAZ WORKS** — headline metric: meaningful client-acquisition actions with
   a truthful breakdown of outreach, follow-up, demo/audit, conversation,
   booked call, proposal and client won.

The owner can create, edit, reorder, pin, pause and archive any additional
ongoing track from the web UI without editing firmware or code.

## Product principles

- **Frictionless first.** A normal progress event is one tap from the WORK tab.
  Optional context must never block a simple increment.
- **Web-first management.** The Cardputer is a glance/control surface. Track
  creation, editing and history management live in the authenticated phone UI.
- **Explicit beats inferred.** A submitted application or sent proposal is a
  stronger fact than browser time, token volume or agent uptime.
- **No universal productivity score.** Goal progress, focus time, agent state
  and model telemetry are distinct signals.
- **Truthful history.** Store append-only events and derive daily/weekly totals.
  Undo uses an explicit reversal rather than silently deleting history.
- **Local-first.** v1 requires no cloud account. Work data lives in MAZ Core.
- **Six Home surfaces remain.** No seventh tile and no stable-ID breakage.
- **One installable release.** v1 is not complete until host, firmware,
  packaging, installer/update flow and release artifacts are all coherent.

## Final information architecture

### Cardputer Home

```text
CALL      CAPTURE      AGENTS
CONTROL   MEMORY       WORK
```

Stable Home IDs remain `talk`, `capturehub`, `agents`, `desk`, `recall` and
`flow`. Only the visible `flow` label becomes **WORK**. Preserve old IDs,
shortcuts, palette targets and deep links.

WORK opens directly on progress, not a tools menu. It should prioritize:

- JOB HUNT today / target;
- MAZ WORKS today / target;
- one or two additional pinned custom tracks when space allows;
- compact seven-day consistency;
- stale/offline state when MAZ Core is unavailable.

The existing Focus Timer, Work Sprint, Tasks, Reminders, Shift Clock and Retro
remain available through a secondary WORK tools action.

### Authenticated phone UI

The existing `/control/` phone application becomes the main WORK management
surface and should use focused tabs:

```text
WORK | AUTHORITY
```

WORK is the default tab. AUTHORITY preserves the existing phone approvals,
grants, manual sessions and audit feed. Do not weaken the security boundary.

The WORK tab contains:

1. **Today** — pinned track cards with large current/target values.
2. **Quick actions** — large one-tap event buttons.
3. **Undo last** — safely reverse the most recent manual event.
4. **Seven days** — compact history/consistency, not a desktop dashboard.
5. **Manage tracks** — create/edit/reorder/pin/pause/archive.
6. **Track detail** — event breakdown, targets and recent history.

`mazpocket.local` stays lightweight. It gets a clear **OPEN WORK** handoff to
the authenticated Core phone UI rather than duplicating the tracker in firmware
HTML.

## Track model

A Track is an ongoing objective, not a one-off task. v1 supports:

- `count` — applications, outreach, workouts, posts;
- `time` — minutes/hours of focused work;
- `checkin` — yes/no completion for the day.

Each Track includes:

- stable ID;
- name and short label;
- mode;
- unit label;
- cadence: daily, weekly or none;
- optional target value;
- ordered event types;
- one primary event type when applicable;
- pinned/order state;
- active/paused/archived state;
- created/updated timestamps.

Each Work Event includes:

- stable event ID;
- track ID;
- event type ID;
- numeric quantity/value;
- occurred-at timestamp;
- source (`phone_manual`, `cardputer_manual`, `import`, `task_sync`, etc.);
- optional short note;
- created-at timestamp;
- optional reversal reference.

Store timestamps in UTC and aggregate by the configured/system local timezone.
DST boundaries must be correct.

## Built-in templates

### JOB HUNT

Mode: `count`.

Default event types:

- **APPLICATION** — contributes to the headline application count;
- FOLLOW-UP;
- INTERVIEW;
- REJECTION;
- OFFER.

Only APPLICATION increments the headline application metric.

### MAZ WORKS

Mode: `count`.

Default event types:

- OUTREACH;
- FOLLOW-UP;
- DEMO / AUDIT;
- CONVERSATION;
- CALL BOOKED;
- PROPOSAL;
- CLIENT WON.

The card can show total acquisition activity, but the detail view must always
show the breakdown. Do not imply seven outreaches equal seven proposals or
clients.

Targets are editable. Missing a target is shown plainly; do not add guilt or
moral judgement to the UI.

## Host architecture

Persistence and aggregation belong in MAZ Core. Do not stretch the ESP32 TSV
store into an analytics/event database.

Prefer cohesive modules:

- `host/mazhost/work_store.py` — SQLite schema, migrations and event writes;
- `host/mazhost/work_service.py` — built-in templates, aggregation, targets,
  seven-day summaries and compact Cardputer payloads;
- `host/mazhost/work_routes.py` — authenticated API;
- a small phone-WORK module if `phone_control.py` becomes too large.

Use stdlib `sqlite3` unless the repository proves another dependency is needed.
Store data under `~/.maz-pocket/work/`. Use WAL, foreign keys, bounded queries,
schema versioning and idempotent migrations.

Do not store prompts, completions, screenshots, browser history or credentials
for this feature.

## API behavior

Exact route naming may adapt to existing repository conventions, but v1 needs:

- `GET /work/summary?window=today` — compact pinned progress + freshness +
  seven-day aggregate;
- `GET /work/tracks` — definitions and ordering;
- `POST /work/tracks` — custom track creation;
- structured track update endpoint;
- `POST /work/events` — append progress;
- `POST /work/events/{id}/undo` — safe reversal;
- `GET /work/history?days=7&track_id=...` — bounded history;
- optional export endpoint for a local JSON/CSV backup if it can be done
  without delaying the core loop.

Web mutations are authenticated. Cardputer payloads remain compact. A corrupt
individual track/event must degrade locally rather than crash the entire WORK
summary.

## Existing feature integration

v1 preserves and can improve existing features, but they do not block the
consistency loop.

- Existing Tasks only record `done`, `created`, `bucket` and text. Do not use
  them as historical completion telemetry until completion timestamps/events
  are added explicitly.
- Existing Focus is primarily in-memory on the Cardputer. Do not claim
  historical focus data until sessions are persisted/synced.
- Agent Nudge can appear as secondary context but never increments JOB HUNT or
  MAZ WORKS.
- Existing Call, Capture, Plan, Crew, Prompt Deck, phone authority, Beam,
  installer and M5Launcher safety behavior must remain intact.

## v1 scope boundary

Required for v1:

- Track + Event persistence;
- JOB HUNT and MAZ WORKS templates;
- custom tracks from web UI;
- one-tap logging and undo;
- daily + seven-day history;
- Cardputer WORK dashboard;
- secondary path to existing work tools;
- authenticated phone WORK tab;
- `mazpocket.local` WORK handoff;
- tests, migrations and failure states;
- full host + firmware integration;
- release version bumped to **1.0.0** only when implementation is complete;
- one coherent v1.0.0 install/update package and GitHub Release.

Useful if safe within v1:

- Task completion events with completion timestamps;
- persisted Focus/Sprint/Shift events;
- Agent Nudge secondary card;
- local JSON/CSV export/import.

Deferred until after v1:

- MCP Fix & Activate;
- deep Codex/Claude/OpenCode/Hermes telemetry;
- token/API-value analytics as headline metrics;
- automatic desktop activity surveillance;
- cloud sync/accounts;
- full project-management boards;
- complex heatmaps/analytics;
- track editing on the Cardputer.

## v1 success gates

v1 is done only when all are true:

1. An application can be logged from authenticated WORK in <=2 taps.
2. Maz Works outreach/follow-up/proposal can be logged in <=2 taps.
3. A custom ongoing track can be created without code/firmware changes.
4. Core restart does not lose or duplicate data.
5. Undo is safe, reversible and auditable.
6. The Cardputer shows current progress without configuration interaction.
7. Existing five other primary surfaces and authority/security boundaries work.
8. Full host tests, firmware build, version checks, launcher checks and package
   checks pass.
9. Physical Cardputer ADV acceptance passes.
10. The final v1.0.0 GitHub Release contains the current MAZ Core package,
    Cardputer package, combined install package, raw app `.bin` and checksums.

Do not publish v1 merely because CI compiles. The release number means the
whole daily-driver loop and install path are coherent.
