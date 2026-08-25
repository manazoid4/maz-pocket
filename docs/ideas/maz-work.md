# MAZ Work — v1 direction

## Problem statement

MAZ Pocket already has Tasks, Focus, Sprint, Shift, Agent Nudge and a phone control surface, but it does not answer the most important daily question:

> Am I consistently moving the few ongoing things that matter?

The v1 use case is concrete:

- keep a truthful count of job applications;
- keep a truthful count and breakdown of Maz Works client-acquisition activity;
- allow any future ongoing goal to be added from the web UI without changing firmware;
- make logging so fast that the tracker is actually used every day.

## Recommended direction

MAZ Work becomes the private consistency and work surface inside MAZ Pocket.

The Cardputer remains a glance/control device. The authenticated phone UI is the main input and management interface because creating and editing ongoing tracks on a 240x135 screen would add unnecessary friction.

The core primitive is not a Task. It is a **Track + Event**:

- a Track is an ongoing objective such as JOB HUNT, MAZ WORKS, TRAINING or STUDY;
- an Event is one explicit unit of progress such as APPLICATION, OUTREACH, FOLLOW-UP, PROPOSAL or CHECK-IN.

This avoids corrupting the existing one-off task model and gives v1 durable history, targets, streaks and custom categories.

## v1 built-in tracks

### JOB HUNT

Primary metric: applications submitted.

Default event types:

- APPLICATION;
- FOLLOW-UP;
- INTERVIEW;
- REJECTION;
- OFFER.

Only APPLICATION increments the headline application count. Other event types appear in detail/history.

### MAZ WORKS

Primary metric: meaningful client-acquisition actions.

Default event types:

- OUTREACH;
- FOLLOW-UP;
- DEMO / AUDIT;
- CONVERSATION;
- CALL BOOKED;
- PROPOSAL;
- CLIENT WON.

The card shows total activity plus a breakdown. It must never present seven outreaches as seven proposals or clients.

## Custom tracks

The phone UI can create a new ongoing track with:

- name;
- short label;
- type: count, time or check-in;
- unit;
- daily/weekly/no cadence;
- optional target;
- custom quick-log event types;
- primary event type;
- pin/order state.

Examples include workouts, Quran reading, portfolio work, applications for a specific role family, sales follow-ups, content posts, learning sessions or any other ongoing objective.

## Friction budget

The feature fails if logging feels like admin.

Required interaction targets:

- opening WORK on an authenticated phone immediately shows the pinned trackers;
- common progress is one tap on a large `+ APPLICATION`, `+ OUTREACH`, `+ FOLLOW-UP`, etc. action;
- a plain track increment takes at most two taps from the WORK tab;
- the last accidental action can be undone immediately;
- optional notes never block logging;
- targets and track configuration are edited in a separate Manage flow, not during quick logging.

## Cardputer role

The sixth Home surface becomes WORK while preserving the stable internal `flow` ID.

WORK opens directly on progress, not a menu. The first screen should prioritize:

- JOB HUNT today / target;
- MAZ WORKS today / target;
- pinned custom track progress;
- compact seven-day consistency;
- stale/offline indication.

Existing Focus, Sprint, Tasks, Reminders, Shift and Retro remain reachable through WORK tools.

## Data principles

- Persist events, derive totals.
- Never silently turn missing data into zero.
- Use reversal events/tombstones for undo rather than deleting history invisibly.
- Aggregate according to the machine/user local timezone while storing event timestamps in UTC.
- No browser monitoring, keylogging, screenshots, transcripts or prompt bodies are required.
- No cloud account is required.
- No universal productivity score.

## v1 scope

Required:

- local SQLite Track + Event store in MAZ Core;
- built-in JOB HUNT and MAZ WORKS templates;
- custom track CRUD from authenticated phone UI;
- one-tap quick logging;
- undo last event;
- daily and seven-day history;
- compact authenticated Core API;
- direct Cardputer WORK dashboard;
- existing work tools preserved;
- lightweight `mazpocket.local` handoff to WORK;
- tests, migration safety, packaging and physical Cardputer verification;
- final release named **v1.0.0**.

Useful if it fits without destabilising v1:

- explicit Task completion events with completion timestamps;
- persisted Focus/Sprint/Shift completion records;
- Agent Nudge status shown as secondary work context;
- CSV/JSON export/import for the local work database.

## Deferred after v1

- MCP Fix & Activate;
- deep Codex/Claude/OpenCode/Hermes telemetry;
- token/API-value analytics as a headline feature;
- automatic desktop productive-time capture;
- cloud sync;
- complex project management;
- heatmaps and large analytics dashboards.

## v1 continuation test

Carry it for seven days. If logging is consistently used and the glance view changes actual behaviour, expand the telemetry layer. If not, simplify before adding more instrumentation.
