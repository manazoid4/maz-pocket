# MAZ Pocket v1.0.0 — full build prompt

## Mission

Implement, verify, merge and release **MAZ Pocket v1.0.0 — WORK Consistency** from the current repository.

This is an execution assignment, not another brainstorming pass. Read the repository first, make a short implementation plan, then build the complete release end-to-end without repeatedly stopping for confirmation.

The product goal is simple:

> MAZ Pocket should make it almost effortless to stay consistent with ongoing goals, especially job applications and Maz Works client acquisition, while remaining a fast physical AI/control surface.

The final result must be one coherent installable v1.0.0 release, not a collection of branches, docs or half-integrated features.

## Repository truth first

Before editing:

1. Fetch latest `main`.
2. Inspect `git status`, open PRs, open branches, recent merged PRs, workflows and latest GitHub Release.
3. Read completely:
   - `README.md`
   - `QUICKSTART.txt`
   - `CHANGELOG.md`
   - `RELEASE_NOTES.md`
   - `VERSION`
   - `docs/V100-FEATURE-ARCHITECTURE.md`
   - `docs/ideas/maz-work.md`
   - `tasks/plan.md`
   - `tasks/todo.md`
   - `docs/RELEASE_RULES.md`
   - `docs/VERIFICATION.md`
   - `host/README.md`
   - `host/.env.example`
   - `.github/workflows/firmware.yml`
   - `.github/workflows/release.yml`
   - current code under `host/mazhost/`, `src/apps/`, `src/net/`, `src/core/`, `src/storage/`.
4. Treat current code as truth if it has moved beyond the plan.
5. Preserve unrelated user files and existing working configuration.
6. Work on one clearly named v1 branch, e.g. `release/v1-work-consistency`.
7. Do not push unfinished experimental commits directly to `main`.

## Non-negotiable product shape

The six Home surfaces are:

```text
CALL      CAPTURE      AGENTS
CONTROL   MEMORY       WORK
```

Do not add a seventh Home tile.

Preserve stable IDs:

- `talk`
- `capturehub`
- `agents`
- `desk`
- `recall`
- `flow`

The visible `flow` label becomes **WORK**, but the internal ID remains for compatibility.

The current v0.8 functionality must continue to work:

- Call MAZ;
- voice reply route controls;
- Capture / Brain Dump / Teach / recorder flows;
- Plan / Crew / Prompt Deck / Agent Nudge;
- local/cloud/AUTO model routing;
- phone-approved authority grants;
- pairing/token-ID flow;
- Beam;
- live Cardputer screen/status;
- firmware staging;
- M5Launcher-safe handoff;
- Windows installer/update path.

Do not solve the new feature by breaking or replacing those systems unnecessarily.

# Core v1 feature: WORK Consistency

## Problem

The owner has long-running goals that depend on repeated action, but ordinary Tasks do not provide useful consistency history.

The first two required tracks are:

### JOB HUNT

Headline metric: **applications submitted**.

Default events:

- APPLICATION — primary headline event;
- FOLLOW-UP;
- INTERVIEW;
- REJECTION;
- OFFER.

Only APPLICATION increments the main application count.

### MAZ WORKS

Headline metric: **meaningful client-acquisition actions**.

Default events:

- OUTREACH;
- FOLLOW-UP;
- DEMO / AUDIT;
- CONVERSATION;
- CALL BOOKED;
- PROPOSAL;
- CLIENT WON.

The detail view must show the breakdown. Do not imply seven outreaches equal seven clients or proposals.

## Custom tracks

The authenticated web UI must allow creation of arbitrary ongoing tracks without firmware or code changes.

Support three simple modes:

- `count`
- `time`
- `checkin`

A custom track can define:

- name;
- short label;
- unit;
- cadence: daily / weekly / none;
- optional target;
- event types;
- primary event type;
- pin/order state;
- active / paused / archived state.

Examples include workouts, Quran reading, study sessions, portfolio work, content posts, sales follow-ups, coding practice or any other ongoing goal.

# Friction budget

This feature is successful only if it is faster to use than to ignore.

Required interaction targets:

- Phone WORK is the default authenticated control tab.
- Logging an application takes <=2 taps from WORK.
- Logging common Maz Works activity takes <=2 taps.
- A plain +1 never requires a note, modal or keyboard.
- Optional notes are available after or alongside logging, never required.
- The last accidental manual event can be undone immediately.
- Track configuration is separate from quick logging.
- Mobile layout must work cleanly around 360–430 CSS px width with no horizontal scrolling.

Do not add extra confirmation dialogs for reversible low-risk +1 actions.

# Data architecture

## Track + Event, not Task abuse

Do not bolt this onto the existing Task TSV and pretend Tasks are a historical tracker.

Existing Tasks currently have only a small state contract. Build a dedicated Work data model in MAZ Core.

Prefer:

- `host/mazhost/work_store.py`
- `host/mazhost/work_service.py`
- `host/mazhost/work_routes.py`

If another layout better matches repository conventions, use it, but keep one clear owner per responsibility.

## SQLite requirements

Use Python stdlib `sqlite3` unless repository evidence shows a stronger reason not to.

Database location:

`~/.maz-pocket/work/`

Requirements:

- schema version table;
- idempotent migrations;
- WAL;
- foreign keys;
- bounded queries;
- explicit transactions;
- durable event IDs;
- stable track IDs;
- deterministic built-in template IDs;
- no credentials/prompts/completions/screenshots/browser history.

### Track fields

At minimum:

- id;
- name;
- short label;
- mode;
- unit;
- cadence;
- target;
- primary event type;
- pinned;
- sort order;
- state;
- created_at;
- updated_at.

### Event type fields

At minimum:

- id;
- track_id;
- label;
- sort order;
- contributes_to_headline;
- active state.

### Work event fields

At minimum:

- event_id;
- track_id;
- event_type_id;
- value/quantity;
- occurred_at;
- source;
- optional bounded note;
- created_at;
- reversal_of / reversed state.

Store event timestamps in UTC. Aggregate daily/weekly views according to the local machine/user timezone. Add tests around midnight and DST transitions.

## Undo semantics

Do not silently delete history.

Undo should create or mark an explicit reversal/tombstone such that:

- totals recalculate correctly;
- repeated undo is idempotent;
- the audit/history can explain what happened;
- an accidental double-tap can be corrected safely.

# Service behavior

Implement a Work service that owns:

- built-in template seeding;
- custom track validation;
- headline metric semantics;
- event breakdowns;
- today totals;
- weekly totals;
- seven-day history;
- target progress;
- truthful consistency/streak calculations;
- compact Cardputer payload generation;
- freshness timestamps.

Do not create a universal productivity score.

Missing data is unknown, not silently zero, where the distinction matters.

# API

Use existing authentication patterns.

Required behaviors, even if route names are adjusted to match the repository:

- `GET /work/summary?window=today`
- `GET /work/tracks`
- create track
- update track
- reorder/pin/pause/archive track
- `POST /work/events`
- undo event
- `GET /work/history?days=7&track_id=...`

Useful optional endpoint if low-risk:

- local JSON or CSV export/import.

Requirements:

- strict Pydantic bounds;
- no secret exposure;
- clear 4xx failures;
- history cap;
- compact response for firmware;
- one broken track/event does not crash the whole summary.

# Authenticated phone UI

Refactor the existing phone control UI into focused tabs:

```text
WORK | AUTHORITY
```

## WORK tab

Make WORK default after login.

### Today section

Show pinned track cards.

JOB HUNT card:

- large application count;
- optional target;
- `+ APPLICATION` primary button;
- smaller FOLLOW-UP / INTERVIEW / OFFER actions if space allows;
- seven-day mini summary.

MAZ WORKS card:

- meaningful activity total;
- optional target;
- large quick actions such as `+ OUTREACH`, `+ FOLLOW-UP`, `+ PROPOSAL`;
- breakdown visible without ambiguity.

Custom track card:

- current value / target;
- primary +1/check-in/time action;
- secondary event types when defined.

### Undo Last

A clearly visible action reverses only the most recent eligible manual event, then refreshes all affected totals.

### Seven days

Use a compact table/bars/dots that works well on phone width.

Do not build a desktop analytics dashboard.

### Manage Tracks

Allow:

- create;
- edit;
- change target/cadence/unit;
- add/reorder/disable event types;
- select primary event;
- reorder/pin;
- pause/archive;
- restore archived if straightforward.

## AUTHORITY tab

Preserve:

- pending approvals;
- active grants;
- manual session;
- audit feed;
- revoke grant;
- revoke all;
- phone session/logout;
- existing token/session security.

Do not expose the pairing token after login.

# Cardputer WORK

## Home

Change visible FOCUS -> WORK while preserving `flow` ID.

## Landing screen

WORK must open directly on progress.

Prioritize:

1. JOB HUNT today / target.
2. MAZ WORKS today / target.
3. One or two pinned custom tracks where layout allows.
4. Compact seven-day consistency indicator.
5. Core freshness / stale / offline state.

Do not open a submenu first.

## Secondary tools

Keep existing:

- Focus Timer;
- Work Sprint;
- Tasks;
- Reminders;
- Shift Clock;
- Retro.

These can sit behind a Tools key/action.

## Firmware constraints

- Real display is 240x135.
- Use large readable values and short labels.
- Keep strings bounded.
- Keep rendering simple.
- No dense charting.
- No track editor.
- No secret entry.
- Use existing host worker rather than blocking the UI loop.
- Cache last successful Work summary and label it stale when appropriate.
- Throttle refreshes.
- One network failure must not freeze navigation.

# Existing Tasks / Focus integration

## Tasks

The current Task model does not have a trustworthy completion timestamp.

If v1 integrates Tasks into Work history:

- add explicit completed_at or emit a Work event on done transition;
- reopening must reverse the completion fact correctly;
- preserve old TSV data;
- do not invent old completion dates for already-done tasks.

This integration is useful but must not block Track + Event v1.

## Focus / Sprint / Shift

If included in v1:

- persist only bounded start/end/outcome metadata;
- make cancellation explicit;
- do not claim historical focus before persistence exists;
- preserve current timer UX.

Again: useful, not a blocker for the core consistency loop.

# Agent and telemetry integration

Agent Nudge may appear as secondary context in WORK.

Do not let agent uptime, token volume or model usage increment human goal progress.

Deep Codex/Claude/OpenCode/Hermes telemetry and API Value are deferred until after v1 unless they are already effectively complete and require near-zero extra risk.

# Portal

Keep `mazpocket.local` lightweight.

Required changes:

- use WORK wording instead of FOCUS where appropriate;
- open WORK on the Cardputer;
- provide a clear OPEN WORK link to authenticated Core phone control;
- preserve pairing/status/live screen/settings/staging.

Do not embed the full Work database UI into firmware HTML.

# Installation and upgrade experience

v1 must be installable by the owner without repo surgery.

Preserve the current release rule that produces:

- `MAZ-Core-v1.0.0.zip`
- `MAZ-Cardputer-v1.0.0.zip`
- `MAZ-Pocket-v1.0.0-Install.zip`
- raw M5Launcher app `.bin`
- SHA-256 checksums/evidence

The **combined install ZIP is the default user path**.

Requirements:

- one obvious Windows entry point;
- explicit actions before mutation;
- preserve existing MAZ Core `.env` and pairing configuration;
- safe Core update in place;
- no guessing between multiple removable drives;
- firmware staging remains easy;
- M5Launcher remains the only firmware installer/rollback owner;
- entering M5Launcher must not invalidate MAZ Pocket;
- keep firmware within known M5Launcher app slot ceiling;
- preserve existing PowerShell 5.1/encoding package tests;
- do not make the owner manually copy individual Python/C++ files.

If possible, make START-HERE clearly report:

- installed Core version;
- package version;
- whether Core will be installed/updated/skipped;
- whether a removable drive was found;
- next step for M5Launcher;
- URL to `mazpocket.local` / phone WORK.

# Version strategy

The target release is **1.0.0**.

Do not bump `VERSION` to 1.0.0 at the start.

Only promote canonical version identity after the complete implementation is release-candidate quality.

At promotion, update consistently:

- root `VERSION`;
- firmware identity;
- README current release;
- QUICKSTART;
- CHANGELOG;
- RELEASE_NOTES;
- package names;
- any version-contract tests.

Do not add `.release/v1.0.0` until the implementation has merged and release gates have passed according to repository release rules.

# Multi-perspective review requirement

Before release, perform independent passes from each perspective below. Do not collapse them into one generic “looks good” review.

## 1. Product usefulness reviewer

Questions:

- Does this actually help consistency?
- Are JOB HUNT and MAZ WORKS the fastest things to log?
- Does custom-track flexibility stay simple?
- Did we accidentally build a project manager instead of a consistency tool?
- Does every visible metric have an obvious meaning?

Reject features that add admin without changing behavior.

## 2. Friction/UX reviewer

Check:

- tap counts;
- default WORK tab;
- button sizes;
- mobile width;
- empty/loading/error states;
- undo discoverability;
- manage-track complexity;
- Cardputer legibility at 240x135;
- offline/stale clarity;
- no intermediate menus before useful data.

## 3. Data-integrity reviewer

Check:

- migrations;
- duplicate event handling;
- transaction safety;
- undo/reversal;
- archive/pause behavior;
- timezone + DST;
- restart persistence;
- malformed requests;
- corrupt DB/source behavior;
- no silent double counting.

## 4. Security/privacy reviewer

Check:

- authenticated mutations;
- phone session boundary;
- no token/credential leakage;
- no secret display;
- local-first storage;
- bounded notes/input;
- no new arbitrary shell path;
- authority broker behavior unchanged.

## 5. Firmware/performance reviewer

Check:

- image size;
- heap/stack impact;
- no blocking UI network calls;
- bounded JSON parser;
- host worker behavior;
- refresh cadence;
- stale cache;
- navigation responsiveness;
- no seventh tile;
- no new giant dependency.

## 6. Release-engineering reviewer

Check:

- version contract;
- Windows install path;
- preserved `.env`;
- generated package contents;
- exact artifact names;
- checksums;
- M5Launcher handoff;
- rollback path;
- release trigger rules;
- installer tests against the actual shipped ZIP.

## 7. Regression reviewer

Exercise current v0.8 features:

- CALL;
- CAPTURE;
- AGENTS;
- CONTROL;
- MEMORY;
- phone authority;
- local/cloud routing;
- Beam;
- pairing;
- portal;
- staging;
- Launcher handoff.

Critical/high findings from any reviewer are release blockers.

# Testing requirements

## Work store/service tests

At minimum:

- clean DB creation;
- migration replay;
- built-in template idempotency;
- custom count/time/check-in tracks;
- event append;
- primary vs secondary event counting;
- target math;
- undo/reversal;
- duplicate ID handling;
- pause/archive;
- local-midnight and DST cases;
- seven-day aggregation;
- malformed note/value bounds;
- DB restart persistence.

## API tests

- authentication;
- create/update/list;
- event append;
- undo;
- bounds;
- missing IDs;
- history caps;
- compact summary shape.

## Phone UI tests

Use available browser automation if practical.

Verify:

- login;
- WORK default;
- application +1;
- outreach/follow-up/proposal +1;
- undo;
- custom track create;
- pin/reorder/pause/archive;
- narrow phone viewport;
- AUTHORITY tab still works;
- revoke all still works.

## Firmware tests

- six Home surfaces;
- WORK label/ID compatibility;
- parser fixtures;
- malformed/missing fields;
- busy/offline cache;
- Tools path to old work features;
- compile for Cardputer ADV;
- image below slot ceiling.

# Required commands / gates

Run the repository's actual current gates. At minimum expect:

```powershell
python -m pytest host/tests -q
python scripts/check-version.py
python scripts/check-launcher-handoff.py
pio run -e cardputer-adv
./scripts/package-release.ps1
```

Also run all current CI-specific installer/package checks and any new v1 tests.

Do not substitute a local partial test for exact-head CI.

# Physical Cardputer acceptance

Before publishing v1.0.0, verify on a real M5Stack Cardputer ADV:

1. Install/update through the intended user path.
2. MAZ Pocket boots normally.
3. Home shows exactly six surfaces.
4. WORK opens and is readable.
5. WORK reflects phone logging.
6. Offline/Core-loss leaves a clear stale state, not a freeze.
7. Existing work tools remain reachable.
8. CALL/CAPTURE/AGENTS/CONTROL/MEMORY still open and function.
9. Phone control login/authority still works.
10. Firmware staging works.
11. Entering M5Launcher does not make MAZ Pocket disappear.
12. Re-launching installed MAZ Pocket works.

If hardware is unavailable, leave an honest release candidate and an exact remaining checklist. Do not claim physical verification.

# Git / PR / merge behavior

The final repository should not be left with a scattered v1 implementation.

During implementation:

- use one main v1 implementation branch/PR where practical;
- rebase/merge latest `main` before final validation;
- preserve unrelated user work;
- close/supersede only branches that are truly obsolete;
- do not merge stale conflicting historical PRs merely to increase merge count.

At completion:

1. exact-head CI green;
2. critical/high review findings fixed;
3. required physical verification green;
4. v1 implementation PR ready;
5. merge complete implementation into `main`;
6. verify no unfinished v1 feature PR is left open;
7. create the canonical v1.0.0 release trigger according to repo rules;
8. verify release workflow succeeds;
9. verify GitHub Release contents;
10. hand off the combined install package as the primary artifact.

# Final deliverable

When finished, report:

- what changed;
- architecture decisions made;
- files/modules added;
- data schema/migrations;
- tap-count results;
- Cardputer UX result;
- regression results;
- test results;
- exact CI run/result;
- physical hardware verification result;
- merged PR number/commit;
- v1.0.0 release URL;
- SHA-256 of the firmware app binary;
- list of release artifacts;
- one recommended install file: `MAZ-Pocket-v1.0.0-Install.zip`.

Do not finish with “here is what remains to implement” unless an external hard blocker genuinely prevents completion. If something fails, diagnose it, fix it, rerun the relevant gates and continue.
