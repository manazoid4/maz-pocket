# Implementation Plan: MAZ Pocket v1.0.0 — WORK Consistency

## Objective

Turn the current v0.8.0 CONTROL baseline into one coherent **v1.0.0** daily-driver release centered on frictionless consistency tracking while preserving the existing six-surface architecture, phone authority, agent workbench, local/cloud AI routing, firmware safety and installer/release rules.

The v1 loop is:

```text
PHONE WORK -> one-tap progress event -> MAZ Core durable event store
          -> compact summary -> Cardputer WORK glance
          -> seven-day feedback -> repeat
```

The first built-in tracks are JOB HUNT and MAZ WORKS. Custom ongoing tracks are created and managed from the web UI.

## Non-negotiable architecture decisions

- Exactly six Home surfaces remain: CALL / CAPTURE / AGENTS / CONTROL / MEMORY / WORK.
- Keep stable `flow` ID; change only the visible label/landing behavior.
- Track management is web-first. Cardputer is read/glance + optional quick increment only.
- MAZ Core owns SQLite persistence and aggregation.
- Store append-only events; undo uses reversal semantics.
- Existing Tasks are not silently reinterpreted as historical telemetry.
- Existing phone authority security must not regress.
- Existing M5Launcher ownership and firmware-size safeguards remain.
- v1 version bump and release marker happen only after implementation and verification are complete.
- Final state must be merged to `main` with no unfinished v1 PR left open.

## Phase 0 — Repository reconciliation

### Task 0.1: Start from repository truth

- Pull/fetch latest `main`.
- Inspect open PRs, branches, recent merged work, workflows and latest GitHub release.
- Confirm v0.8.0 is the implementation baseline unless repository truth has moved.
- Preserve unrelated user changes.
- Do not resurrect stale closed branches whose useful work has already been superseded.

### Task 0.2: Lock v1 contracts before coding

Define compact fixtures for:

- track definitions;
- event definitions;
- today summary;
- seven-day history;
- stale/offline state;
- reversal/undo;
- custom count/time/check-in tracks.

Acceptance:

- stable IDs and current shortcuts remain valid;
- no seventh Home tile;
- JSON contracts are small enough for Cardputer parsing;
- phone mutations require authenticated control session or existing bearer auth according to the chosen route boundary.

## Phase 1 — Durable Work model

### Task 1: Implement SQLite Track + Event store

Likely module: `host/mazhost/work_store.py`.

Schema must cover:

- schema version/migrations;
- tracks;
- track event types;
- work events;
- reversal link/state;
- ordering/pinning/archive state;
- timestamps;
- optional metadata/note within strict bounds.

Requirements:

- stdlib sqlite3 preferred;
- database under `~/.maz-pocket/work/`;
- WAL + foreign keys;
- idempotent migrations;
- transaction boundaries around mutations;
- UTC storage with correct local-day aggregation;
- bounded reads;
- no prompt/completion/browser/credential storage.

Tests:

- clean bootstrap;
- repeated bootstrap/migration;
- insert/list/update track;
- append event;
- undo/reversal idempotency;
- duplicate event ID rejection/idempotency;
- archived/paused behavior;
- DST/local-midnight aggregation;
- corrupt/invalid input failures are explicit.

### Task 2: Seed built-in templates safely

Built-ins:

**JOB HUNT**
- APPLICATION primary;
- FOLLOW-UP;
- INTERVIEW;
- REJECTION;
- OFFER.

**MAZ WORKS**
- OUTREACH;
- FOLLOW-UP;
- DEMO / AUDIT;
- CONVERSATION;
- CALL BOOKED;
- PROPOSAL;
- CLIENT WON.

Requirements:

- template seeding is idempotent;
- user edits survive restarts/upgrades;
- templates are normal tracks after creation, not special hard-coded UI-only objects;
- event-type IDs are stable.

## Phase 2 — Work service and API

### Task 3: Implement aggregation/service layer

Likely module: `host/mazhost/work_service.py`.

Provide:

- pinned today summary;
- target progress;
- primary metric count vs all-event breakdown;
- seven-day daily totals;
- current streak/consistency where truthful;
- freshness timestamps;
- time/check-in track semantics;
- safe local timezone handling;
- compact Cardputer response.

No composite productivity score.

### Task 4: Add authenticated Work routes

Likely module: `host/mazhost/work_routes.py`, installed through current app composition rather than further bloating `app.py`.

Required behavior:

- `GET /work/summary?window=today`;
- `GET /work/tracks`;
- create custom track;
- structured update/reorder/pin/pause/archive;
- append event;
- undo event;
- seven-day bounded history;
- optional JSON/CSV export if low risk.

Tests:

- auth required;
- validation bounds;
- unknown IDs;
- paused/archived writes;
- count/time/check-in semantics;
- headline primary count vs event breakdown;
- history caps;
- undo;
- compact payload shape.

## Phase 3 — Phone-first WORK UI

### Task 5: Refactor `/control/` into WORK | AUTHORITY

Preserve login/session behavior and all existing authority functions.

WORK is the default tab.

Today view:

- large JOB HUNT card;
- large MAZ WORKS card;
- pinned custom tracks;
- progress value + optional target;
- large quick-log buttons;
- immediate optimistic-looking feedback only after server success;
- Undo Last;
- compact 7-day history.

Manage Tracks:

- create;
- edit name/short label;
- choose count/time/check-in;
- unit/cadence/target;
- configure event types + primary type;
- reorder/pin;
- pause/archive;
- restore archived where practical.

Friction acceptance:

- application logging <=2 taps from WORK tab;
- Maz Works common event logging <=2 taps;
- no required modal/note for +1;
- phone width around 360-430 CSS px is comfortable;
- no horizontal scrolling;
- touch targets are large;
- error state retains context and never silently increments locally.

Authority acceptance:

- pending approvals, grants, manual session, audit and revoke continue to work;
- no pairing token display regression;
- Revoke All remains prominent within AUTHORITY.

## Phase 4 — Cardputer WORK

### Task 6: Change visible FOCUS surface to WORK

Keep internal `flow` compatibility ID.

WORK landing:

- direct progress dashboard;
- no intermediate menu;
- JOB HUNT and MAZ WORKS prioritized;
- additional pinned track when layout allows;
- seven-day compact indicator;
- stale/offline display using last good cached summary where safe.

Secondary WORK tools retain:

- Focus Timer;
- Work Sprint;
- Tasks;
- Reminders;
- Shift Clock;
- Retro.

Do not build track editor on Cardputer.

### Task 7: Add bounded Work transport

Use the existing single Host worker architecture.

Requirements:

- no blocking HTTP/filesystem work in UI loop;
- compact parser with fixed/bounded strings;
- last-good summary cache;
- busy/offline/error states;
- refresh throttling;
- no new unbounded FreeRTOS worker fleet.

Tests/build checks:

- malformed/missing fields;
- unknown track counts;
- offline cache;
- queue busy;
- screen clipping at 240x135;
- exactly six Home descriptors.

## Phase 5 — Integrate existing work facts where safe

### Task 8: Task completion timestamps/events

Current Task storage lacks an explicit completion timestamp. If Tasks feed WORK:

- add completion timestamp or emit a Work Event when toggled done;
- reopening must reverse/record correctly;
- migration from old task TSV must preserve existing tasks;
- do not guess historical completion dates for old done tasks.

This task is useful but may be cut from v1 if it risks the core loop.

### Task 9: Focus/Sprint/Shift persistence

If included:

- explicit start/end/cancel outcomes;
- persist only summary metadata needed for history;
- do not claim historical focus before this path exists;
- preserve current timing UX.

Also useful but not allowed to block the core Track + Event loop.

## Phase 6 — Portal and install/update friction

### Task 10: Minimal `mazpocket.local` handoff

- change sixth surface wording to WORK;
- add clear OPEN WORK link/button to authenticated Core phone UI;
- keep pairing, live screen, config and firmware staging behavior intact;
- do not duplicate full tracker HTML/data into firmware.

### Task 11: Make v1 installation a single coherent path

The build/release must retain the existing split artifacts and combined convenience installer:

- `MAZ-Core-v1.0.0.zip`;
- `MAZ-Cardputer-v1.0.0.zip`;
- `MAZ-Pocket-v1.0.0-Install.zip`;
- raw M5Launcher app `.bin`;
- SHA-256 evidence.

Combined install package must:

- have one obvious Windows entry point;
- install/update MAZ Core without destroying existing `.env`/pairing config;
- explain what it will do before mutating;
- detect ambiguity rather than guessing removable drive;
- make firmware staging easy;
- keep M5Launcher as firmware installer/rollback owner;
- preserve the already-fixed Windows PowerShell 5.1/encoding gates;
- never require the user to manually copy random individual repo files.

## Phase 7 — Full audit and regression

### Task 12: Multi-perspective audit

Run independent reviews from these perspectives:

1. **Product/usefulness** — does it reduce inconsistency or add admin?
2. **UX/friction** — tap count, discoverability, mobile layout, 240x135 clarity.
3. **Data integrity** — migrations, reversal, timezone, idempotency, corruption.
4. **Security/privacy** — auth boundaries, no secret leakage, local-first data.
5. **Firmware/performance** — image size, heap, worker behavior, no blocking UI.
6. **Release engineering** — version contract, installer, package contents, rollback.
7. **Regression** — CALL/CAPTURE/AGENTS/CONTROL/MEMORY and existing v0.8 behavior.

Any critical/high finding must be fixed before release.

## Phase 8 — Version, merge and release

### Task 13: Promote to v1.0.0 only when release candidate is complete

Update all canonical version surfaces consistently:

- `VERSION` -> `1.0.0`;
- firmware version identity;
- README current release;
- QUICKSTART;
- CHANGELOG;
- RELEASE_NOTES;
- package naming;
- any tests/version guards.

Do not create the `.release/v1.0.0` marker early.

### Task 14: Automated gates

At minimum:

- `python -m pytest host/tests -q`;
- `python scripts/check-version.py`;
- `python scripts/check-launcher-handoff.py`;
- PlatformIO Cardputer ADV build;
- firmware slot/image ceiling check;
- release packaging;
- shipped Windows PowerShell installer parse/dry-run gates;
- any new Work-specific tests;
- no secrets in generated fixtures/artifacts.

### Task 15: Physical Cardputer gate

Verify on real ADV:

- boot/update;
- Home still has six surfaces;
- WORK renders/readable;
- refresh + stale/offline behavior;
- existing CALL/CAPTURE/AGENTS/CONTROL/MEMORY flows;
- phone WORK logging;
- Core restart persistence;
- firmware staging;
- M5Launcher handoff and installed MAZ image persistence.

If real hardware is unavailable, do not claim physical verification and do not publish v1 as fully verified. Leave a clear RC with the exact remaining gate.

### Task 16: Merge everything and publish

Once exact-head CI and required physical gates pass:

- ensure implementation branch is up to date with `main`;
- resolve all review findings;
- merge the complete v1 implementation PR to `main`;
- verify no separate v1 feature PR remains open/unmerged;
- add the canonical `.release/v1.0.0` trigger on a release PR/commit as repository rules require;
- wait for release workflow to build/publish from `main`;
- verify GitHub Release v1.0.0 contains every required artifact and checksum;
- verify README/release identity now points to v1.0.0;
- provide the user the single combined install package as the default install choice.

## Definition of done

v1 is not done because a tracker endpoint exists. It is done when:

- the daily consistency loop works end-to-end;
- phone logging is genuinely low-friction;
- custom tracks need no code changes;
- data survives upgrades/restarts;
- Cardputer glance view is useful;
- v0.8 security/control features still work;
- exact-head automated + physical gates pass;
- all v1 code is merged;
- one v1.0.0 GitHub Release is published with a frictionless combined installer.
