# MAZ Pocket v1.0.0 — execution checklist

## Reconcile

- [ ] Start from latest `main`; inspect open PRs/branches/releases/workflows.
- [ ] Preserve current v0.8 CONTROL behavior and stable IDs.
- [ ] Lock compact Track/Event/summary/history contracts.

## Durable Work core

- [ ] Add versioned SQLite Track + Event store under `~/.maz-pocket/work/`.
- [ ] Add idempotent JOB HUNT template.
- [ ] Add idempotent MAZ WORKS template.
- [ ] Add custom count/time/check-in tracks.
- [ ] Add append-only progress events.
- [ ] Add safe undo/reversal semantics.
- [ ] Add correct UTC/local-time aggregation and DST tests.
- [ ] Add bounded seven-day summary/history.

## API

- [ ] Add authenticated Work summary route.
- [ ] Add track list/create/update/reorder/pin/pause/archive routes.
- [ ] Add event append + undo routes.
- [ ] Add bounded history route.
- [ ] Add compact Cardputer payload tests.

## Phone WORK UI

- [ ] Refactor phone shell to `WORK | AUTHORITY`.
- [ ] Make WORK default.
- [ ] JOB HUNT large card + one-tap APPLICATION.
- [ ] MAZ WORKS card + one-tap common acquisition actions.
- [ ] Pinned custom track cards.
- [ ] Undo Last.
- [ ] Seven-day history.
- [ ] Manage Tracks create/edit/reorder/pin/pause/archive.
- [ ] Keep authority approvals/grants/manual session/audit/revoke intact.
- [ ] Validate mobile width/touch/tap-count acceptance.

## Cardputer WORK

- [ ] Keep exactly six Home surfaces.
- [ ] Keep internal `flow` ID; visible label becomes WORK.
- [ ] WORK opens directly on progress dashboard.
- [ ] Prioritize JOB HUNT + MAZ WORKS.
- [ ] Show pinned custom progress when space allows.
- [ ] Show compact seven-day state.
- [ ] Show stale/offline last-good state honestly.
- [ ] Preserve Focus/Sprint/Tasks/Reminders/Shift/Retro through secondary tools.
- [ ] Use existing bounded Host worker; no blocking UI network work.

## Optional safe integration

- [ ] Add explicit Task completion timestamp/event if Tasks feed WORK.
- [ ] Add Focus/Sprint/Shift summary persistence if safe.
- [ ] Add Agent Nudge secondary context if safe.
- [ ] Add local JSON/CSV export/import if safe.

## Portal + installation

- [ ] `mazpocket.local` uses WORK wording and OPEN WORK handoff.
- [ ] Keep pairing/live screen/settings/staging intact.
- [ ] Combined install package remains the default frictionless path.
- [ ] Preserve existing `.env`/pairing config on Core upgrade.
- [ ] Preserve M5Launcher installer/rollback ownership.
- [ ] Preserve Windows PowerShell 5.1 shipped-installer validation.

## v1 release gates

- [ ] Multi-perspective product/UX/data/security/firmware/release/regression audit.
- [ ] Fix all critical/high findings.
- [ ] Full host tests green.
- [ ] Version guard green.
- [ ] Launcher handoff guard green.
- [ ] Cardputer ADV build green and under slot ceiling.
- [ ] Release packaging green.
- [ ] Physical ADV WORK + regression gate passes.
- [ ] Set `VERSION` and all canonical docs/package identities to `1.0.0`.
- [ ] Merge complete v1 implementation to `main`.
- [ ] Ensure no unfinished v1 PR remains open.
- [ ] Trigger `.release/v1.0.0` only after the merge/gates.
- [ ] Verify GitHub Release v1.0.0 contains Core, Cardputer, combined install ZIP, raw bin and SHA-256 evidence.
- [ ] Default handoff to the user is the single combined v1.0.0 install package.

## Deferred after v1

- [ ] MCP Fix & Activate.
- [ ] Deep Codex/Claude/OpenCode/Hermes telemetry.
- [ ] Token/API Value headline analytics.
- [ ] Automatic desktop productivity surveillance.
- [ ] Cloud sync/accounts.
- [ ] Complex project management and heatmaps.
