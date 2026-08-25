# v1 Consistency — Comparable Systems Landscape

Research pass for `docs/V100-CONSISTENCY-BUILD-PROMPT-V2-UPGRADER.md` Stage 1. Live web research conducted 2026-08-25. 22 projects reviewed across six categories. None of this code is being vendored; MAZ Pocket's Work Track/Event store is implemented natively in `host/mazhost/`. Entries are patterns to imitate or avoid, not dependencies to add.

## A. Habit / consistency / streak tracking

| Project | URL | License | Freshness | Pattern worth studying | Avoid | Verdict |
|---|---|---|---|---|---|---|
| Loop Habit Tracker (uhabits) | github.com/iSoron/uhabits | GPLv3 | active | Non-punishing streak scoring (a missed day doesn't zero a long streak); CSV/SQLite export as the trust mechanism | Android-only widget/reminder stack irrelevant to a phone-web + Cardputer split | v1: adopt the "streak survives one miss" scoring philosophy for the 7-day view language, not a formula |
| Beaver Habit Tracker | github.com/daya0576/beaverhabits | MIT | active | Deliberately has **no goals/streak pressure**, single `habits.db` SQLite file, Docker self-host | Habit-only vocabulary doesn't map to "applications submitted" as a headline count | v1: confirms single-file SQLite + no-guilt UI copy is the right default tone |
| OpenHabitTracker | github.com/jinjinov (OpenHabitTracker) | MIT-ish, verify at repo | active | EF-Core/SQLite local-first across 5 platforms from one schema | Cross-platform sync layer is out of scope | not for v1 |
| Habo | habo.space | source-available, verify license before any reuse | active | Minimal single-purpose habit UI, no account required | — | not for v1 (reference only) |
| Habitica / HabitRPG | github.com/HabitRPG/habitica | GPLv3 | active | Dailies vs. Habits vs. To-Dos as three distinct event shapes | Gamification (HP/gold/avatar) is exactly the "universal productivity score" the architecture doc forbids | do not copy; useful as a negative example |
| habitica-lite | github.com/haxejs/habitica-lite | MIT | low activity | Shows how small a Habitica-style tracker can be stripped down to | Same gamification anti-pattern | not for v1 |
| Beeminder (data model, not code) | beeminder.com/api | proprietary, no code reviewed | active | Target-vs-actual line is a good mental model for target-with-grace, not a UI to copy | Financial penalty mechanic is wrong for this product | pattern only, no license exposure |

## B. Time / activity tracking

| Project | URL | License | Freshness | Pattern worth studying | Avoid | Verdict |
|---|---|---|---|---|---|---|
| ActivityWatch | github.com/ActivityWatch/activitywatch | MPL-2.0 | active | "Bucket" = append-only event stream per source, local SQLite, REST events API for read/write | Its automatic desktop-activity capture is exactly the passive surveillance the upgrader doc forbids | v1: adopt the bucket-of-append-only-events shape for `work_events`, never adopt automatic capture |
| Kimai | github.com/kimai/kimai | AGPL-3.0 | active | Punch-in/punch-out + multi-timer as a `time` mode reference; mature timezone/invoice handling | Full multi-user Symfony/Doctrine stack is far heavier than one local Core process needs | not for v1; AGPL means no source reuse anyway |
| Toggl Track (public docs only) | toggl.com/track, no code reviewed | proprietary | active | Single big "Start/Stop" button as the friction bar for `time` mode | — | pattern only |

## C. Local-first personal data systems

| Project | URL | License | Freshness | Pattern worth studying | Avoid | Verdict |
|---|---|---|---|---|---|---|
| sqlite-utils 4.0 | github.com/simonw/sqlite-utils | Apache-2.0 | active (2026-07 release) | `migrations.apply(db)`-style ordered, idempotent migration runner keyed off `PRAGMA user_version` | Full CLI/library is more than needed; only the migration pattern is relevant | v1: `work_store.py` migration runner should mirror this shape (ordered version functions, transactional, `PRAGMA user_version` gate) |
| sql-event-store | github.com/mattbishop/sql-event-store | MIT | reference | De-duplication + guaranteed ordering via a dedicated SQLite DDL, directly portable | Postgres/serverless framing doesn't apply | v1: borrow the idempotency-key + ordering column idea for `event_id` uniqueness |
| eventsourcing_sqlite (Gleam) | github.com/renatillas/eventsourcing_sqlite | Apache-2.0 | active | Confirms "compensating event, never delete" as the standard undo pattern | Gleam/BEAM runtime irrelevant | v1: confirms the reversal-event design already chosen in `docs/ideas/maz-work.md` is industry-standard, not novel risk |
| Declarative schema migration for SQLite (D. Rothlis) | david.rothlis.net/declarative-schema-migration-for-sqlite | article, no code license | reference | Diff-current-schema-against-target as an alternative to versioned migration scripts | More complex than needed for a single-app schema that only grows | not for v1; versioned migrations remain simpler and match repo convention |

## D. Small-device / embedded dashboards

| Project | URL | License | Freshness | Pattern worth studying | Avoid | Verdict |
|---|---|---|---|---|---|---|
| awesome-m5stack-cardputer (curated list) | github.com/terremoth/awesome-m5stack-cardputer | CC0/list | active | Survey of what other Cardputer firmware projects prioritize on a 240x135 screen (large numerals, 1-2 line summaries) | — | pattern only |
| Bruce firmware | github.com/IncursioHack/Bruce | GPL-3.0 | active | Menu/app-registry pattern for many small tools on one Cardputer binary, similar shape to `src/apps/registry.cpp` | Its feature set (RF/BLE offensive tooling) is irrelevant and must not influence WORK scope | confirms existing registry-table approach is already the right pattern; no code borrowed |
| Cardputer local-LLM pocket dashboard ("pocket home-server dashboard for M5Stack Cardputer") | found via GitHub topic search, exact repo URL unverified — re-verify before citing publicly | unknown, verify before reuse | unknown | Validates the product category (glance dashboard + local companion service over LAN) already matches MAZ Pocket's Core+Cardputer split | Cannot verify license/activity — do not borrow code | pattern-existence only |
| M5Unified / ESP-IDF docs (M5Stack) | docs.m5stack.com/en/core/Cardputer | vendor docs | authoritative | Confirms 240x135, heap/flash ceilings the firmware reviewer must respect | — | authoritative reference for Stage-3 firmware decisions |

## E. Mobile web control panels

| Project | URL | License | Freshness | Pattern worth studying | Avoid | Verdict |
|---|---|---|---|---|---|---|
| pwa-with-vanilla-js | github.com/ibrahima92/pwa-with-vanilla-js | MIT | reference | No-framework PWA structure matches this repo's existing `phone_control.py`-served HTML approach | — | v1: confirms staying vanilla HTML/JS for `/control/` is consistent with the broader ecosystem, not an outdated choice |
| hnpwa-vanilla | github.com/cristianbote/hnpwa-vanilla | MIT | reference | Minimal service-worker + no-framework rendering for a list-heavy mobile UI | — | pattern only |
| meraki-dashboard-pwa-admin | github.com/dexterlabora/meraki-dashboard-pwa-admin | MIT | reference | Card-based mobile admin panel layout, large tap targets | Full Vue app scaffolding unnecessary for this repo's vanilla-JS convention | pattern only |

## F. Agent / developer work telemetry (secondary ideas only)

| Project | URL | License | Freshness | Pattern worth studying | Avoid | Verdict |
|---|---|---|---|---|---|---|
| opentelemetry-hooks | github.com/o11y-dev/opentelemetry-hooks | Apache-2.0 | active (2026) | "Explicit hook event -> span" model is a clean example of provenance/source tagging (`source: phone_manual` vs `source: import`) | Full OTLP export pipeline is unnecessary; deep agent telemetry is already deferred post-v1 in `docs/ideas/maz-work.md` | v1: borrow only the `source` provenance-tagging idea already planned for `work_events.source`; do not build an OTel pipeline |
| OpenTelemetry GenAI semantic conventions | opentelemetry.io | Apache-2.0 | authoritative | Standard vocabulary for "explicit vs inferred" event classification | — | reference only, confirms "explicit beats inferred" principle already in `docs/ideas/maz-work.md` |

## Extra: adjacent CRM pattern for MAZ WORKS breakdown modeling

| Project | URL | License | Freshness | Pattern worth studying | Avoid | Verdict |
|---|---|---|---|---|---|---|
| Twenty CRM | github.com/twentyhq/twenty | AGPL-3.0 | active | Pipeline stage vocabulary (outreach -> conversation -> proposal -> won) validates the MAZ WORKS event-type breakdown already specified in the v1 docs | Full CRM object model (companies, opportunities, custom objects) is overkill; do not build a CRM | v1: confirms the fixed 7-event-type MAZ WORKS template already scoped is the right size, not under-scoped |
| EspoCRM | github.com/espocrm/espocrm | GPL-3.0 | active | Same pipeline-stage confirmation | Same over-scope risk | not for v1 |

## What NOT to copy, summarized

- No gamification/scoring layer (Habitica) — the architecture doc explicitly forbids a "universal productivity score."
- No automatic desktop/browser activity capture (ActivityWatch's watchers) — forbidden by the safety rules (no surveillance).
- No multi-user/invoicing time-tracker stack (Kimai) — wrong scale, AGPL besides.
- No full CRM object model (Twenty/EspoCRM) — MAZ WORKS stays a fixed template, not a pipeline builder.
- No code is vendored from any AGPL/GPL project (Kimai, Vikunja, Habitica, Bruce, Twenty, EspoCRM) into this repo; only structural/UX patterns are borrowed and re-implemented natively.

## Ideas carried into V2 (v1 scope)

1. Ordered, transactional, `PRAGMA user_version`-gated migrations (sqlite-utils pattern).
2. Compensating/reversal event for undo, never row deletion (sql-event-store / eventsourcing_sqlite pattern) — already the plan, now confirmed as industry standard.
3. `source` provenance field on every event (opentelemetry-hooks pattern) — already scoped, confirmed necessary.
4. Non-punishing streak language: a truthful 7-day count, not a chain that "breaks" (Loop Habit Tracker philosophy).
5. Vanilla HTML/JS phone UI stays correct and current, not a legacy choice (pwa-with-vanilla-js confirms).

## Ideas explicitly deferred or rejected

- Gamification/streak-chain UI (Habitica) — rejected, contradicts "no guilt/moral judgement."
- Automatic activity capture (ActivityWatch watchers) — rejected, contradicts safety rules.
- Full CRM/pipeline builder (Twenty) — deferred indefinitely; MAZ WORKS stays a fixed template.
- Multi-user/invoicing (Kimai) — not applicable, single-owner local product.

## Vikunja (task manager, referenced for completeness)

| Project | URL | License | Freshness | Pattern worth studying | Avoid | Verdict |
|---|---|---|---|---|---|---|
| Vikunja | github.com/go-vikunja/vikunja | AGPL-3.0 | active | Confirms Task and ongoing-Track are legitimately separate data models in comparable products (Vikunja itself doesn't try to be a streak tracker) | AGPL, full Kanban/Gantt scope | not for v1; confirms "do not bolt onto Tasks" decision already made in the V1 prompt |
