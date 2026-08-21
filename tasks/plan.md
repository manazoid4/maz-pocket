# Implementation Plan: MAZ Pocket v0.9 WORK

## Overview

Deliver the two v0.9 daily loops defined in
`docs/V090-FEATURE-ARCHITECTURE.md`: glanceable, truthful WORK telemetry and a
safe MCP check/fix flow. Preserve the six Home IDs and all existing app IDs.
Use vertical slices so WORK and MCP foundations can progress independently
after the navigation and API contracts are locked.

## Architecture decisions

- MAZ Core owns parsing, aggregation, SQLite history, subprocesses and config
  mutation; the Cardputer renders compact cached summaries.
- `flow` becomes the visible WORK surface without changing its stable ID.
- Cardputer MCP is read-only. Authenticated phone UI owns repair and approval.
- v0.9 hides duplicate links; it does not merge/delete their implementations.
- Unknown data remains unknown. Tokens, uptime and Git volume are not a
  productivity score; API-priced totals are labelled API Value.

## Dependency graph

```text
Navigation/API contracts
    +-- Work store/collectors -> Work service/routes -> Cardputer WORK
    +-- MCP scan -> safe mutation -> phone MCP -> Cardputer CONTROL
                                              +-> minimal portal handoff
```

## Phase 1: Contracts and foundations

### Task 1: Lock navigation and response contracts

**Description:** Preserve the six stable Home IDs, display WORK in place of
FOCUS, define compact Work/MCP payload fixtures and remove only duplicated
menu discovery paths.

**Acceptance criteria:**

- [ ] Exactly six Home descriptors remain and `flow` resolves as WORK.
- [ ] Existing IDs, shortcuts, palette targets and deep links still resolve.
- [ ] Work and MCP payload fixtures cover ready, unknown, stale and partial failure.

**Verification:** registry/navigation tests, version check, firmware build and
manual palette/deep-link check.

**Dependencies:** None

**Files likely touched:** `src/apps/registry.cpp`, `src/apps/surfaces.cpp`,
`src/apps/apps.h`, focused firmware test/fixture.

**Estimated scope:** Medium

### Task 2: Persist incremental Work telemetry

**Description:** Normalize Codex and Claude local metadata into a versioned,
incremental SQLite store without retaining prompt/completion bodies.

**Acceptance criteria:**

- [ ] Fixtures normalize and deduplicate incrementally.
- [ ] Missing fields remain nullable and corrupt input degrades one source only.
- [ ] Repeated refreshes prove cursor and idempotency behavior.

**Verification:** focused pytest and inspection of synthetic SQLite rows.

**Dependencies:** None

**Files likely touched:** `host/mazhost/work_store.py`,
`host/mazhost/work_collectors.py`, two focused test files.

**Estimated scope:** Medium

### Task 3: Scan MCP clients read-only

**Description:** Normalize Codex, Claude, OpenCode and Hermes MCP state with
bounded concurrency, hard client timeouts and complete secret redaction.

**Acceptance criteria:**

- [ ] Each installed/missing client reports independently using normalized states.
- [ ] Hanging probes are killed and never block the full result indefinitely.
- [ ] No tools are invoked and no credentials appear in responses/logs.

**Verification:** disposable healthy, absent, disabled, malformed,
missing-environment and hanging fixtures plus redaction assertions.

**Dependencies:** Task 1 contract

**Files likely touched:** `host/mazhost/mcp_adapters.py`,
`host/mazhost/mcp_manager.py`, `host/mazhost/mcp_routes.py`, app wiring, tests.

**Estimated scope:** Medium

## Checkpoint: foundations

- [ ] Focused host tests pass.
- [ ] Firmware builds with six Home tiles.
- [ ] Schemas are reviewed before consumer UI work begins.

## Phase 2: Complete WORK loop

### Task 4: Aggregate and serve WORK

**Description:** Add authenticated bounded summary, now, history and refresh
routes using explicit Focus, Done and API Value semantics.

**Acceptance criteria:**

- [ ] Required endpoints return six metrics, freshness and source health.
- [ ] Midnight, unknown model and seven-day comparison cases are deterministic.
- [ ] Partial source failure does not fail the complete dashboard.

**Verification:** focused API tests followed by all host tests.

**Dependencies:** Tasks 1 and 2

**Files likely touched:** `work_service.py`, `work_routes.py`, app wiring,
pricing data/module and one route test.

**Estimated scope:** Medium

### Task 5: Render WORK on Cardputer

**Description:** Make WORK open directly on six readable metrics with compact
detail and a secondary path to the existing work tools.

**Acceptance criteria:**

- [ ] Six cells are readable at 240x135 and no menu precedes them.
- [ ] Unknown, stale, offline-cache and error states are distinct.
- [ ] Focus, Sprint, Tasks, Reminders, Shift and Retro remain reachable.

**Verification:** firmware build, parser fixtures and physical key/clipping/
offline/refresh tests.

**Dependencies:** Tasks 1 and 4

**Files likely touched:** Core client pair, `src/apps/work.cpp`, apps header and
registry.

**Estimated scope:** Medium

## Phase 3: Complete MCP loop

### Task 6: Implement transactional Fix & Activate

**Description:** Repair selected, existing deterministic MCP configuration
problems using prepare/back up/atomic commit/re-read/rescan with full rollback.

**Acceptance criteria:**

- [ ] One structured request repairs only selected deterministic cases.
- [ ] Injected mid-commit failure fully restores every target.
- [ ] A second successful run is a semantic no-op with a redacted audit proof.

**Verification:** disposable acceptance matrix, backup/restore inspection,
rollback, idempotency, audit and redaction tests.

**Dependencies:** Task 3

**Files likely touched:** `mcp_mutation.py`, `mcp_manager.py`, existing audit
owner and two focused test files.

**Estimated scope:** Medium

### Task 7: Add authenticated phone tabs

**Description:** Present WORK, MCP and AUTHORITY as focused tabs while keeping
Revoke All visible and preserving the existing authority behavior.

**Acceptance criteria:**

- [ ] All three tabs work at phone width without one long flat page.
- [ ] Fix & Activate sends one batch and reports unresolved OAuth/approval honestly.
- [ ] Revoke All is always visible and pending approvals are badged.

**Verification:** auth route tests, phone-width browser check and repair/
partial/pending flows.

**Dependencies:** Tasks 4 and 6

**Files likely touched:** `phone_control.py`, Work/MCP routes and focused tests.

**Estimated scope:** Medium

### Task 8: Put MCP readiness on Cardputer CONTROL

**Description:** Open CONTROL on a compact readiness view and expose redacted
MCP aggregate/rescan without allowing device-side mutation.

**Acceptance criteria:**

- [ ] Core, laptop, Wi-Fi and MCP readiness are visible immediately.
- [ ] MCP problems are redacted and a rescan reflects phone repairs.
- [ ] No configuration mutation or secret entry exists on the device.

**Verification:** payload fixtures, firmware build and physical scrolling/
key/rescan tests.

**Dependencies:** Tasks 1 and 3

**Files likely touched:** Core client pair, control app, surfaces and focused test.

**Estimated scope:** Medium

## Phase 4: Handoff and release

### Task 9: Add minimal portal handoff

**Description:** Update wording and links only; keep the firmware portal a
small device companion and preserve pairing/update.

**Acceptance criteria:**

- [ ] Portal links to authenticated Work and MCP.
- [ ] It embeds no analytics database or MCP mutation logic.
- [ ] Existing pairing/update flows and firmware size remain acceptable.

**Verification:** content assertions, size comparison and physical link check.

**Dependencies:** Tasks 5 and 7

**Files likely touched:** `src/net/portal_v3.cpp` and one focused test.

**Estimated scope:** Small

### Task 10: Pass release gates

**Description:** Run the complete automated, disposable-config and physical
Cardputer gates; publish only through the existing release workflow.

**Acceptance criteria:**

- [ ] All host, firmware, version, launcher and package checks pass.
- [ ] Disposable MCP matrix proves backup, rollback, timeout and redaction.
- [ ] Physical WORK/CONTROL/phone/portal flows pass before release marker.

**Verification:** commands and physical checklist in the v0.9 build prompt.

**Dependencies:** Tasks 1-9

**Files likely touched:** version/release files only as required.

**Estimated scope:** Small

## Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Client commands hang | High | Per-client timeout, bounded concurrency, process-tree kill, partial result |
| Config repair corrupts files | High | Parse in memory, timestamped backups, atomic commit, journal, rollback, re-read |
| Telemetry implies productivity | High | Separate metric domains, explicit unknowns, no composite score |
| UI becomes another menu maze | Medium | Direct WORK/CONTROL landings, six Home tiles, remove duplicate links only |
| Scope expands into app rewrite | High | Defer wrappers/mergers and full portal redesign to v0.9.x |

## Definition of done

- [ ] Every task's acceptance and verification items pass.
- [ ] Stable navigation compatibility is proven.
- [ ] No secrets or transcript bodies are stored or returned.
- [ ] User completes both daily loops with minimal interaction.
- [ ] Physical gate passes before publishing v0.9.0.
