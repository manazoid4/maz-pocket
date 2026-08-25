# MAZ Pocket v1.0.0 — WORK Consistency — Build Prompt V2

This is the controlling implementation prompt. It supersedes `docs/V100-CONSISTENCY-BUILD-PROMPT.md`, `docs/V100-FEATURE-ARCHITECTURE.md` and `docs/ideas/maz-work.md` as the single source of truth for what to build. Those three docs remain as historical context; where they conflict with this file, this file wins.

Grounded in: repository truth audit (main at `201a914`, v0.8.0 shipped, no WORK code exists yet), `docs/research/v1-consistency-v2-landscape.md` (22 comparable projects), and `docs/audits/V100-PROMPT-V2-REVIEW.md` (8 reviewer passes, 13 prioritized findings, all resolved below).

## What V2 changed from V1

1. Merged three overlapping docs' JOB HUNT/MAZ WORKS template definitions into one canonical block below — no more restating the same event list three times.
2. Added a concrete idempotency contract for event writes (`event_id` uniqueness, repeated POST is a no-op) — V1 never stated this, and duplicate-tap/retry double-counting was an unaddressed failure mode.
3. Replaced "<=2 taps" with a literal, testable click sequence and a named e2e test.
4. Replaced vague "stale/offline state" with a concrete threshold and required UI treatment.
5. Made explicit that `flow`'s default render changes from Focus-tools-directly to the WORK glance dashboard, with existing tools moved one level deeper — V1 implied this but never named it as a behavior change.
6. Replaced the "useful if it fits" maybe-list with an explicit Deferred section — no mid-build scope rationalization.
7. Added an explicit non-goal against derived composite metrics (no "close rate," no productivity score, ever).
8. Added an explicit custom-track boundary: no sub-tasks, no assignees, no cross-track dependencies — a track is flat.
9. Added an explicit v0.8 -> v1.0.0 upgrade-safety test (`.env`, pairing token, new WORK database all survive).
10. Added undo disambiguation: undo reverses the current session's most recent event, not any event by any device.
11. Added a hard payload-size ceiling for the Cardputer WORK JSON (4 pinned tracks max, fixed 7-day array) enforced on both server and firmware sides.
12. Added named crash-safety tests: mid-transaction kill, mid-migration kill, corrupt-DB-file behavior.
13. Added a module-size guideline (~400 lines) for new `work_*.py` files to prevent silent bloat.
14. Added a real physical Cardputer ADV acceptance checklist (Section 10) as a hard release gate, not a suggestion.
15. Added explicit closure criteria for "no unfinished v1 PR remains" and "GitHub Release v1.0.0 verified" (Section 11).
16. Clarified that no code is vendored from research-reviewed projects (several are AGPL/GPL) — patterns only, native implementation.

---

## Mission

Implement, verify, merge and release **MAZ Pocket v1.0.0 — WORK Consistency**. One coherent installable release, not a collection of branches or half-integrated features. Work on branch `release/v1-work-consistency`, off current `main` (`201a914`). Do not push unfinished commits directly to `main`.

## Non-negotiable product shape (unchanged from V1, re-stated once)

Six Home surfaces, stable internal IDs, only the `flow` visible label changes:

```text
CALL      CAPTURE      AGENTS
CONTROL   MEMORY       WORK
```

Stable IDs: `talk`, `capturehub`, `agents`, `desk`, `recall`, `flow`. No seventh tile. All current v0.8 functionality (CALL, CAPTURE, AGENTS workbench, phone authority broker, pairing, Beam, live Cardputer screen, firmware staging, M5Launcher-safe handoff, Windows installer/update, local/cloud/AUTO AI routing) must continue to work exactly as documented in `README.md`/`QUICKSTART.txt`/`RELEASE_NOTES.md` for v0.8.0.

---

## 1. Core feature: WORK Consistency

### Track model

A Track is an ongoing objective. Three modes: `count`, `time`, `checkin`. A Track is flat — no sub-tasks, no assignees, no due-date chains, no dependencies on other tracks. If a request needs any of those, it is out of v1 scope.

Track fields (minimum): `id` (stable), `name`, `short_label`, `mode`, `unit`, `cadence` (`daily`/`weekly`/`none`), `target` (optional), `primary_event_type_id`, `pinned` (bool), `sort_order`, `state` (`active`/`paused`/`archived`), `created_at`, `updated_at`.

Event Type fields: `id` (stable), `track_id`, `label`, `sort_order`, `contributes_to_headline` (bool), `active` (bool). Event types are referenced by ID everywhere internally; renaming a label must never change historical attribution.

Work Event fields: `event_id` (client-or-server UUID, **unique constraint**), `track_id`, `event_type_id`, `value` (numeric quantity), `occurred_at` (UTC), `source` (`phone_manual`/`cardputer_manual`/`import`), `note` (optional, bounded length, sanitized the same way existing Capture/Braindump text fields are), `created_at`, `reversal_of` (nullable event_id), `reversed` (bool).

**Idempotency contract:** a POST with an `event_id` that already exists returns 200 with the existing event, unmodified — never a duplicate row, never a 409. This makes client retries after a dropped response safe by construction.

### Built-in templates (canonical — the only definition; do not restate elsewhere)

**JOB HUNT** — mode `count`. Event types: `APPLICATION` (primary, contributes to headline), `FOLLOW_UP`, `INTERVIEW`, `REJECTION`, `OFFER`. Only APPLICATION increments the headline application count.

**MAZ WORKS** — mode `count`. Event types: `OUTREACH`, `FOLLOW_UP`, `DEMO_AUDIT`, `CONVERSATION`, `CALL_BOOKED`, `PROPOSAL`, `CLIENT_WON`. The card shows total activity; the detail view always shows the breakdown. Never imply N outreaches equal N proposals or clients won. **Non-goal:** v1 never computes a derived composite metric (no "close rate," no weighted score) from this breakdown. Show raw counts only.

Template seeding is idempotent — safe to run on every Core boot, survives restarts/upgrades, user edits to a seeded track are never overwritten by re-seeding.

### Custom tracks

Authenticated web UI creates arbitrary tracks without firmware or code changes: name, short label, mode, unit, cadence, optional target, event types, primary event type, pin/order, active/paused/archived state.

---

## 2. Friction budget (testable, not a vibe)

- WORK is the default tab of the authenticated `/control/` phone UI.
- **Tap-count acceptance test (must exist as an actual e2e test with these exact steps):** from an already-open WORK tab, tap the `data-testid="quick-log-application"` button once -> event is recorded, UI confirms. That is the 1-tap common case. From Home/closed state: 1 tap to open WORK (if not already open) + 1 tap to log = 2 taps maximum. Write this as a literal Playwright/webapp-testing script, not a description.
- A plain +1 never requires a note, modal, or keyboard.
- **Undo:** "Undo Last" reverses the most recent event created by the current authenticated session, not the most recent event system-wide (protects against undoing a concurrent second device's action). Available immediately after logging, no confirmation dialog (reversal is itself reversible-by-redo-logging, low risk).
- **Duplicate-tap protection:** client-side debounce on quick-log buttons (disable button for ~600ms after tap) plus the server-side idempotency contract above as the real safety net.
- Mobile layout: no horizontal scroll at 360-430 CSS px width, large touch targets (44px minimum per standard mobile a11y guidance).
- Track configuration (Manage Tracks) is a separate flow from quick logging — never interleaved.

---

## 3. Data architecture

New modules under `host/mazhost/`: `work_store.py` (SQLite schema, migrations, writes), `work_service.py` (aggregation, templates, 7-day summaries, compact Cardputer payload), `work_routes.py` (authenticated FastAPI routes). If any of these three exceeds ~400 lines, split by concern (e.g. `work_routes.py` -> separate template/CRUD/history route groups) rather than letting one file grow indefinitely — a guideline for judgment, not a CI-enforced limit.

Database: stdlib `sqlite3`, WAL mode, foreign keys enabled, under `~/.maz-pocket/work/`. Schema versioned via `PRAGMA user_version`, ordered idempotent migration functions applied in a transaction each (mirrors the `sqlite-utils` migration pattern from research). Store timestamps in UTC; aggregate daily/weekly by the host machine's local timezone (verify and reuse whatever timezone source the rest of MAZ Core already uses — do not invent a second one).

**Required tests (name these exactly, do not substitute vaguer equivalents):**
- `test_fresh_bootstrap_creates_schema_and_seeds_templates`
- `test_repeated_bootstrap_is_noop`
- `test_migration_crash_mid_step_resumes_cleanly` (kill between migration steps, restart, assert no double-apply)
- `test_write_crash_mid_transaction_leaves_no_partial_row`
- `test_corrupt_db_file_fails_loudly_not_silently_zero`
- `test_duplicate_event_id_post_is_noop_not_duplicate`
- `test_undo_reverses_only_current_session_last_event`
- `test_archived_track_rejects_new_events_but_keeps_history`
- `test_headline_count_only_includes_primary_event_type`
- `test_dst_fallback_transition_counts_event_once_correct_local_day`
- `test_renamed_event_type_label_does_not_affect_historical_attribution`

---

## 4. API (authenticated, same session cookie as existing `phone_control.py` `require_session`)

- `GET /work/summary?window=today` — pinned tracks, headline counts, target progress, freshness timestamp.
- `GET /work/tracks` — full track list including paused/archived.
- `POST /work/tracks` — create custom track.
- `PATCH /work/tracks/{id}` — update/reorder/pin/pause/archive.
- `POST /work/tracks/{id}/events` — append event (idempotent on `event_id`).
- `POST /work/events/{event_id}/undo` — reversal, scoped to current session's own last event.
- `GET /work/history?days=7` — bounded seven-day daily totals.

All request/response bodies use Pydantic models; malformed input returns 422, never silently coerced. No new auth mechanism — reuse the existing control-session boundary exactly.

---

## 5. Phone-first WORK UI

Refactor `/control/` into two tabs: `WORK | AUTHORITY`. WORK is default. AUTHORITY is the existing approvals/grants/manual-session/audit/revoke UI, untouched.

WORK tab contents: Today (pinned cards with current/target), Quick actions (large one-tap buttons per primary + common event types), Undo Last, Seven days (compact history), Manage Tracks (create/edit/reorder/pin/pause/archive).

Server is the source of truth: a browser refresh during any mutation must re-fetch `GET /work/summary`, never trust client-cached optimistic state as final.

---

## 6. Cardputer WORK

`flow`'s default render **changes** (this is a stated behavior change, not an implicit one): it now opens directly on the WORK glance — JOB HUNT today/target, MAZ WORKS today/target, up to 4 pinned custom tracks (hard cap), compact 7-day strip (fixed-length array), stale/offline indicator. The existing Focus Timer, Work Sprint, Tasks, Reminders, Shift Clock, Retro move one level deeper behind an explicit "WORK TOOLS" action from the same `flow` app — still reachable, no longer the default view.

**Stale/offline contract:** determine the existing Host worker's poll interval from `src/net/host_worker.cpp`/`src/net/field_host.cpp` before choosing a threshold; if the last successful WORK poll exceeds roughly 3x that interval, the Cardputer shows the last-known values labeled "STALE," never a blank screen and never a fabricated zero.

**Payload contract:** the WORK summary JSON sent to the Cardputer is hard-capped (4 pinned tracks, fixed 7-length day array) and the firmware parser uses a bounded buffer that rejects/ignores an oversized payload rather than overflowing.

Uses the existing bounded Host worker; no new blocking UI network call, no new tighter polling loop than what already exists.

---

## 7. Preserved surfaces (regression, must not break)

CALL, CAPTURE (Teach Demo/Brain Dump/Voice Recorder), AGENTS (Status/Plan/Crew/Retro/Projects), phone-approved authority (READ ONLY/PROJECT FULL/PC FULL/ADMIN grants, Revoke All), pairing/Token ID, Beam, live Cardputer LCD mirror, firmware staging, M5Launcher-safe boot-partition handoff (`esp_ota_set_boot_partition()`, no destructive running-app flash write), local/cloud/AUTO AI routing, Windows installer opt-in flow (`START-HERE.cmd`).

`mazpocket.local` stays lightweight: update its wording to WORK and add a clear "OPEN WORK" handoff link to the authenticated Core WORK UI. Do not duplicate the tracker in firmware-served HTML.

---

## 8. Explicitly deferred (post-v1, do not build now)

- Task-completion-as-WORK-event integration.
- Focus/Sprint/Shift completion persistence into WORK.
- Agent Nudge status as WORK secondary context.
- CSV/JSON import/export of the WORK database.
- Drag-to-reorder tracks (pin/unpin + stable sort order is sufficient for v1).
- Any plugin/extension system for additional track modes beyond `count`/`time`/`checkin`.
- Deep Codex/Claude/OpenCode/Hermes agent telemetry.
- Cloud sync/accounts.
- Any derived composite metric (close rate, productivity score) — permanent non-goal, not just deferred.

---

## 9. Upgrade, migration and installer safety

- `.env`, pairing token, and the new `~/.maz-pocket/work/` database must all survive a v0.8.0 -> v1.0.0 update. Write an explicit test: seed v0.8.0 fixture state, run the update path, assert all three survive.
- No new PowerShell/installer script surface should be needed (WORK is host/firmware/UI only). If any installer script changes at all, validate it the same way CI already validates `START-HERE.cmd`/`setup-all.ps1` (both PowerShell editions, dry-run parse).
- Firmware stays an app-only M5Launcher image; `0x180000` slot ceiling enforced in CI, unchanged.
- Artifact contract from `docs/RELEASE_RULES.md` applies unchanged: `MAZ-Core-v1.0.0.zip`, `MAZ-Cardputer-v1.0.0.zip`, `MAZ-Pocket-v1.0.0-Install.zip`, `Maz-Pocket-v1.0.0-M5Launcher.bin`, `SHA256SUMS.txt`.

---

## 10. Physical Cardputer ADV acceptance gate (hard release gate, not a suggestion)

CI green proves source/tests/package integrity, not physical behavior. Before calling v1.0.0 verified, on the real Cardputer ADV:

1. Home shows exactly six tiles: CALL / CAPTURE / AGENTS / CONTROL / MEMORY / WORK.
2. WORK opens directly on the JOB HUNT + MAZ WORKS glance, not a menu.
3. Log one APPLICATION and one MAZ WORKS event from the phone; confirm the Cardputer glance updates on next poll.
4. Undo Last from the phone; confirm the reversal reflects on the Cardputer.
5. Disconnect Core; confirm the Cardputer shows STALE with last-known values, not blank/zero.
6. WORK TOOLS still reaches Focus/Sprint/Tasks/Reminders/Shift/Retro.
7. CALL, CAPTURE -> Teach Demo, AGENTS -> Plan/Crew/Retro, phone authority approve/deny/revoke, pairing/Token ID reveal, `mazpocket.local` OPEN WORK handoff, Ctrl+L return-to-Launcher-without-invalidating-MAZ all still work exactly as in v0.8.0.
8. Stage and install the v1.0.0 `.bin` via M5Launcher from a fresh v0.8.0 install; confirm `.env`/pairing survive and WORK data persists.

---

## 11. Release closure criteria

v1.0.0 is not complete until all of the following are literally true, each independently verifiable:

- Full host pytest suite green, including every named test in Section 3.
- Firmware PlatformIO build green, under the `0x180000` slot ceiling.
- `scripts/check-version.py` passes with `VERSION` = `1.0.0` and `CHANGELOG.md`/`RELEASE_NOTES.md`/`README.md`/`QUICKSTART.txt` updated to match.
- Release packaging produces exactly the five artifacts listed in Section 9, with SHA-256 evidence.
- Physical Cardputer ADV gate (Section 10) passed and its results recorded in the implementation notes.
- `release/v1-work-consistency` (or its follow-on branches) fully merged to `main` via PR; `gh pr list --state open` shows zero open PRs touching WORK/v1 scope.
- `.release/v1.0.0` marker pushed to `main` per `docs/RELEASE_RULES.md`, triggering `.github/workflows/release.yml`.
- `gh release view v1.0.0` confirms the release exists and contains all five artifacts.
- The single recommended user-facing install artifact is `MAZ-Pocket-v1.0.0-Install.zip`.

Do not call the task finished because code compiles or tests pass. It is finished when a daily-driver user can install once, log an application in one tap, glance at the Cardputer and trust what it shows.
