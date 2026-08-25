# V1 -> V2 Prompt Review — Multi-Perspective Critique

Stage 2 of `docs/V100-CONSISTENCY-BUILD-PROMPT-V2-UPGRADER.md`. Eight independent reviewer passes over `docs/V100-CONSISTENCY-BUILD-PROMPT.md` + `docs/V100-FEATURE-ARCHITECTURE.md` + `docs/ideas/maz-work.md` + `tasks/plan.md` + `tasks/todo.md`, informed by repository truth (Stage 0) and the research landscape (Stage 1, `docs/research/v1-consistency-v2-landscape.md`). Every finding is tagged KEEP / CHANGE / CUT / ADD / DEFER and prioritized by impact x implementation risk.

Repository truth used by every reviewer: v0.8.0 is the shipped baseline (latest GitHub Release). No `work_store.py`/`work_service.py`/`work_routes.py` exist yet. `phone_control.py` is 154 lines, auth/approve/deny/revoke only — no WORK tab. `home.cpp` confirms stable IDs `talk/capturehub/agents/desk/recall/flow` map to icons C/+/A/#/M/F; `flow`'s current label is "Focus" (`registry.cpp` line 16: `{"flow","Focus","FOCUS",...}`). No open PRs, no stale v1 branches to reconcile — `main` is clean at `201a914`.

---

## 1. Product / behavior-change reviewer

- **KEEP** — JOB HUNT (applications-primary) and MAZ WORKS (breakdown, no false equivalence) as the two built-in templates. Both are well-modeled; the "only APPLICATION increments the headline count" rule is correct and should stay literal, not softened.
- **CHANGE** — The V1 prompt says custom tracks must avoid "becoming project-management software" but gives no concrete boundary. V2 must state the boundary explicitly: no sub-tasks, no assignees, no due-date chains, no dependencies between tracks. A track has events, not children.
- **CUT** — "Useful if it fits" list in `docs/ideas/maz-work.md` (Task-completion-as-event, Focus/Sprint/Shift persistence, Agent Nudge secondary context, CSV/JSON export) reads as in-scope-if-convenient. Research landscape confirms scope discipline matters more than breadth (Beaver Habit Tracker's whole pitch is doing less). V2 must move these to an explicit Deferred section, not a maybe-list, so an implementation agent doesn't rationalize scope creep mid-build.
- **ADD** — An explicit non-goal: MAZ WORKS breakdown counts are informational, never used to compute a derived "close rate" or similar composite metric in v1. This closes a gap the V1 prompt leaves implicit ("do not imply seven outreaches equal seven clients") but doesn't generalize into a rule against future derived metrics.

## 2. Friction / mobile UX reviewer

- **KEEP** — <=2 taps for APPLICATION and common MAZ WORKS events; Undo Last always available; no required modal for a plain +1.
- **CHANGE** — "<=2 taps" is unverifiable as written — taps from where, counting what as a tap? V2 must define the exact interaction: tap 1 = open WORK tab (already the default authenticated landing tab, so this is often tap 0 if WORK is already open), tap 2 = tap the specific quick-log button. Define the acceptance test as a literal click sequence with a DOM/test-id, not a vibe.
- **CHANGE** — Undo semantics need a stated window and disambiguation rule: undo reverses "the most recent event this session created," not "the most recent event of any kind," to avoid a user undoing someone else's concurrent phone action. V1 doesn't address multi-device concurrency at all.
- **ADD** — Double-tap/rapid-repeat protection: a client-side debounce plus server-side idempotency key (research: sql-event-store pattern) so a flaky mobile network retry cannot silently double-count an application.
- **DEFER** — Reordering/drag-to-reorder tracks is UI polish, not core to the consistency loop. v1 needs pin/unpin and a stable sort (pinned first, then by creation), not drag-and-drop.

## 3. Data-model / local-first reviewer

- **KEEP** — Track + Event (not Task abuse); UTC storage with local-day aggregation; append-only events with reversal, never delete.
- **CHANGE** — V1 lists fields per table but never states the uniqueness/idempotency contract. V2 must require: `event_id` is a client-or-server-generated UUID, unique constraint enforced at the DB level, and a repeated POST with the same `event_id` is a no-op (200, not 409) — this is what makes retries after a dropped mobile response safe, per the research landscape's dedup-key pattern.
- **CHANGE** — DST/timezone handling is stated as a requirement but not testable as written. V2 must specify: aggregation uses the host machine's local timezone (already MAZ Core's existing convention elsewhere in the repo — verify against existing code before inventing a new timezone source), and the test suite must include a fixture event logged in the hour that repeats during a fall-back DST transition, asserting it is counted exactly once on the correct local day.
- **ADD** — Explicit migration-from-nothing test: fresh `~/.maz-pocket/work/` directory does not exist -> first Core boot creates schema + seeds templates idempotently -> second boot is a no-op. This is the exact sqlite-utils/`PRAGMA user_version` pattern surfaced in research and must be a named test, not implied by "idempotent migrations."
- **DEFER** — Import/export (CSV/JSON) stays deferred per the existing "Useful if it fits" list, now formalized as post-v1 in the Product reviewer's CUT above.

## 4. Firmware / embedded reviewer

- **KEEP** — Cardputer stays a glance/control surface; track creation/editing lives only in the phone UI; existing bounded Host worker (`src/net/host_worker.h`) is reused rather than adding a second network path.
- **CHANGE** — V1 says "WORK opens directly on progress" but the current `flow` app (`registry.cpp` line 16, `makeFlow`) currently serves Focus/Sprint/Tasks/Reminders/Shift directly, not a menu. V2 must be explicit that this is a **behavior change**: `flow`'s primary render becomes the WORK glance (JOB HUNT + MAZ WORKS + pinned custom cards + 7-day strip + stale state), and the current Focus/Sprint/Tasks/Reminders/Shift tools move one level deeper behind an explicit "WORK TOOLS" action — not silently removed, not silently left as the default.
- **ADD** — A named stale/offline contract: if the last successful Core WORK poll is older than a defined threshold (align with existing Host worker polling cadence — inspect `host_worker.cpp`/`field_host.cpp` before choosing a number), the Cardputer must show last-known values labeled stale, not blank and not a fabricated zero.
- **ADD** — A parser-bounds requirement: the Cardputer JSON payload for WORK summary must have a hard size ceiling enforced both by `work_service.py` (server truncates/limits pinned tracks and event-type counts) and by the firmware parser (bounded buffer, reject/ignore oversized payload rather than overflow). V1 says "compact Cardputer payload" without a number; V2 must pick one (e.g. hard cap of 4 pinned tracks + fixed 7-day array) so it is testable.
- **KEEP** — Six Home surfaces, no seventh tile, stable IDs preserved with `flow` visible label becoming WORK.

## 5. Security / privacy reviewer

- **KEEP** — No credentials/prompts/completions/screenshots/browser history in Work events; existing phone-authority broker/grant boundary untouched by this feature.
- **CHANGE** — V1's route list (`GET /work/summary`, track CRUD, event append/undo) doesn't state an auth model explicitly. V2 must state plainly: every WORK mutation route requires the same authenticated control-session cookie already enforced by `phone_control.py`'s `require_session`, and the Cardputer's read-only summary fetch goes through the existing bounded Host worker channel, not a new unauthenticated endpoint. No new auth mechanism is introduced.
- **ADD** — Note bounds must be enforced server-side (length cap, no HTML/script stored verbatim — confirm existing repo convention for user text fields, e.g. how Braindump/Capture already sanitize, and reuse it rather than inventing new sanitization).
- **KEEP** — Nothing in this feature can mint or widen an authority grant; WORK is fully separate from the `AUTHORITY` tab's approval mechanism.

## 6. Release / install reviewer

- **KEEP** — `docs/RELEASE_RULES.md` artifact contract (Core zip, Cardputer zip, combined Install zip, raw `.bin`, SHA256SUMS) applies unchanged to v1.0.0; `.release/v1.0.0` marker is the only trigger; version bump lands on `main` before the marker.
- **CHANGE** — V1 doesn't explicitly require `.env`/pairing-token preservation testing across the v0.8 -> v1.0.0 upgrade path, even though `docs/RELEASE_RULES.md` and prior CHANGELOG entries show this has broken before (v0.7.1 installer hotfix, M5Launcher self-deletion bug). V2 must add an explicit upgrade-safety test: install v0.8.0 fixture state, run the v1.0.0 update path, assert `.env`, pairing token and the new `~/.maz-pocket/work/` database all survive.
- **ADD** — Windows PowerShell 5.1 is a known-recurring failure class (v0.7.1 CHANGELOG entry: BOM/encoding issue). Any new installer-facing script V2 introduces (there shouldn't be one — WORK is host/firmware/UI only) must be confirmed to add zero new PowerShell surface; if any script changes at all, it must be validated the same way CI already validates `START-HERE.cmd`/`setup-all.ps1`.
- **KEEP** — Combined `MAZ-Pocket-v1.0.0-Install.zip` remains the single default user handoff artifact.

## 7. Skeptical maintainer reviewer

- **CHANGE** — `work_routes.py` is named as "likely" in three separate docs but the V1 prompt never states a line-count or complexity ceiling, while it does explicitly worry about `phone_control.py`/`app.py` bloat elsewhere. V2 should set the same discipline for the new modules: if `work_routes.py` or `work_service.py` exceeds roughly 400 lines, split by concern (e.g. templates vs. custom-track CRUD vs. history) rather than growing one file indefinitely — a guideline, not a hard CI gate.
- **CUT** — Speculative "future extensibility without premature abstraction" language in `docs/V100-FEATURE-ARCHITECTURE.md` is vague enough to justify either over- or under-building. V2 should replace it with one concrete rule: add a `mode` enum (`count`/`time`/`checkin`) now because it's required for v1; do not add a plugin/extension system for future modes.
- **ADD** — Tests that catch real regressions, named explicitly: (a) headline count only includes the primary event type, (b) undo reverses exactly one event and is idempotent against double-submit, (c) archived track cannot accept new events, (d) six Home surfaces still render after the label change.
- **KEEP** — V2 should in fact end up shorter than the combined V1 (`V100-CONSISTENCY-BUILD-PROMPT.md` + `V100-FEATURE-ARCHITECTURE.md` + `maz-work.md`, currently ~3 overlapping docs) by merging redundant restatements of the same JOB HUNT/MAZ WORKS template definitions into one canonical block.

## 8. Adversarial failure reviewer

Failure modes checked against the current V1 prompt; each either already has a stated mitigation (KEEP) or needs one added (ADD):

| Failure mode | V1 coverage | Verdict |
|---|---|---|
| Duplicate taps / double POST | not addressed | ADD — idempotency key required (see reviewer 3) |
| Interrupted write mid-transaction | implied by "explicit transactions" | KEEP, but ADD an explicit crash-mid-write test (kill process between `BEGIN` and `COMMIT`, assert no partial row on restart) |
| Database locked/corrupt | not addressed | ADD — WAL mode reduces lock contention but V2 must state the behavior when the DB file itself is corrupt: Core must fail loudly on WORK routes with a clear error, never silently return zeros |
| Core unavailable (Cardputer polling) | "stale/offline state" mentioned | CHANGE into a concrete contract (see reviewer 4 ADD) |
| Clock/timezone change mid-day | "DST tests" mentioned | KEEP, formalized as a named test in reviewer 3 |
| DST transition | mentioned | KEEP, formalized |
| Archived track referenced by old events | not addressed | ADD — historical events on an archived track must still render correctly in 7-day history; archiving hides a track from quick-log, it does not hide its history |
| Renamed event type | not addressed | ADD — event types are referenced by stable ID, not label; renaming a label must not affect historical event attribution |
| Partial upgrade (process killed mid-migration) | not addressed | ADD — migration test: kill process between migration steps, assert restart resumes cleanly from `PRAGMA user_version`, never double-applies |
| Old v0.8 user data | covered by release reviewer's upgrade-safety ADD | KEEP |
| Browser refresh during mutation | not addressed | ADD — server is source of truth; a refreshed WORK tab must always re-fetch from `GET /work/summary`, never trust client-cached optimistic state as final |
| Cardputer repeatedly polling | "existing bounded Host worker" implies rate control | KEEP, confirm existing worker's poll interval is reused, not a new tighter loop added |
| Malformed API payload | not addressed | ADD — Pydantic validation on all WORK routes (repo already uses FastAPI/Pydantic elsewhere per `docs/V100-CONSISTENCY-BUILD-PROMPT.md` "Mandatory external research" section); reject with 422, never coerce |
| Failed firmware upgrade | covered by existing `RELEASE_RULES.md`/M5Launcher boundary | KEEP unchanged, this feature adds zero new firmware-flash surface |

---

## Priority summary (impact x risk)

**Must fix before V2 is written (high impact, would cause real bugs or scope drift if left as-is):**
1. Idempotency key for event POST (double-tap/retry safety).
2. Explicit stale/offline Cardputer contract with a real threshold.
3. Explicit tap-count acceptance test definition (not a vibe).
4. Explicit "flow becomes WORK glance, tools move one level deeper" behavior change, stated as a change not an assumption.
5. Explicit Deferred section replacing the "useful if it fits" maybe-list.
6. Upgrade-safety test for `.env`/pairing/new DB across v0.8 -> v1.0.0.

**Should fix (real risk, lower urgency):**
7. Undo disambiguation for multi-device concurrency.
8. Payload size ceiling for Cardputer JSON.
9. Migration crash-safety test.
10. Archived-track history correctness.

**Nice to formalize (low risk, improves clarity):**
11. Module size discipline for `work_routes.py`/`work_service.py`.
12. Merge the three overlapping planning docs' template definitions into one canonical block in V2.
13. Explicit non-goal against derived composite metrics.

All thirteen are addressed in `docs/V100-CONSISTENCY-BUILD-PROMPT-V2.md`.