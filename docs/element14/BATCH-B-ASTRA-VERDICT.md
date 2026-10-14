# Batch B — Astra verdict, 6 September 2026

## Decision

**Approve bounded Batch C implementation; do not approve release yet.** Continue `agents/element14-competition` at `ad062fa` (PR #36). CI passed, including firmware build. Keep PR #35 (`0d1872f`) as a selective patch source. Do not merge it wholesale, downgrade the attached device as a shortcut, or remove its stored data. Build the complete candidate before any guarded app-only installation.

Home is CALL / CAPTURE / AGENTS / CONTROL. Keep stable IDs and existing features accessible through the palette/deeper menus. Brain Dump is the first CAPTURE entry; STATUS and PLAN lead AGENTS. Existing WORK and later architecture remain deferred. The newly requested two PC actions are explicitly in scope, after the connection chain is reliable.

## Findings and exact actions

| Finding / evidence | Batch C action | Required result |
|---|---|---|
| `registry.cpp` maps `talk` to `makeComm`, not legacy `CallApp` in `voice_apps.cpp`. `comm.cpp` already uses `host_worker`. USB acknowledgement does not establish recording. | Inspect `CommApp::beginTake`, worker busy state, `voice::start` error and actual state immediately after Space DOWN. Reproduce once with a physical key if USB remains inconclusive. Fix only the evidenced boundary; do not rewrite audio. | Observe recording, saved WAV, transcription, actual provider reply and displayed result for the same turn. |
| `AgentToolApp::onEnter` calls `refreshProjects` -> `host::coreProjects` synchronously. Recorded 7,833 ms loop maximum is correlated, not a proven timing attribution. | Move project discovery to existing worker with loading/error state and safe ESC. PLAN must accept a task without requiring project discovery. Reuse the existing project picker once loaded. | Opening PLAN with Core offline remains responsive; record before/after timing and request behavior. |
| `WorkflowService.plan` only generates a plan, but blindly merges model JSON over server metadata. Model fields can override `ok`, `kind`, `provider` and authority presentation. | Validate/allowlist the plan fields, then set server-owned metadata last. Display PLAN as read-only; suggested authority is descriptive and never a grant. | Malicious model output cannot forge provider/status or invoke PCController, executor, jobs or approval grants. |
| Batch A `/nudge` returned 503. | Check the configured existing Agent Nudge service once; restore its normal launch/config if available. Otherwise show unavailable with last checked time, never fabricated agents. | Live status has a timestamp/source; unavailable is truthful. PLAN remains usable independently. |
| PR #35 diagnostics sets connected for any dictionary lacking `error`, including `{"ok": false}`. | Do not port this expression. Require affirmative current connection evidence; distinguish unknown, unavailable and stale status. | A failed/disabled status probe cannot display connected. |

## PR #35 port boundary

Port coherent hunks plus their tests, using fresh commits without importing session URLs. Preserve required licence/author attribution; AI metadata is not itself a secret-scan finding.

1. `errors.py`, `validation.py`, routing portions of `llm.py`, `config.py`, and the corresponding `app.py` exception wiring. Include sanitized upstream failures and bounded timeouts. Keep existing local Ollama and configured cloud/MAZLATEST compatibility; no new backend. Avoid copying duplicated dispatch as a second route engine. No WORK imports, bootstrap or daily-state answer interception.
2. Symbolic route helpers/persistence and all affected request serialization in settings, comm, network and portal files as one compatibility change. Preserve saved route choice, including legacy numeric values. Keep one selected cloud route and existing local fallback visible; diagnostics distinguish configured from successfully tested, and report actual reply provider.
3. Short-code pairing as one coherent slice (`pairing.py`, mount, minimal phone/device UI and tests), preserving current credential storage and existing paired clients. Do not port WORK/control-store plumbing. Pairing polish follows CALL/PLAN reliability; leave existing pairing usable if the slice becomes disproportionately large.
4. Launcher handoff verification and `--already-in-launcher` installer wiring with launcher tests. Inspect the actual installation path for erase/partition writes before use. A Python/static check is not physical handoff proof. If safe app-only installation cannot be established, stop that operation and record why.

## Two bounded PC actions

Use existing Core models, authentication, PC controller and firmware worker. No browser automation framework, arbitrary computer use, command runner, plugins or new agent framework.

**Interaction:** explicit PC-action mode accepts typed text or CALL audio. Reuse STT for audio; send only the user's action request to the selected existing cloud route. Normal CALL and Context Ask do not acquire the new execution path. The model proposes; Core validates; Cardputer shows the concrete action; ENTER confirms, ESC cancels. This confirmation is product behavior, not a request for development approval.

Strict model output is one of:

```json
{"action":"open_youtube"}
{"action":"create_notepad_note","text":"Buy milk"}
{"action":"unsupported"}
```

Reject extra fields, unknown actions and invalid types. Note text is plain UTF-8, 1–4000 characters, no NUL. Never accept model-supplied URL, path, filename, executable, shell text or authority. Unsupported/invalid output performs nothing.

Core returns a server-generated proposal ID, normalized preview and 120-second expiry. Bind the proposal to the authenticated client/session. Confirm only the stored proposal, never client-resubmitted parameters. Atomically consume it before dispatch; repeated confirms return the existing receipt without launching again. In-memory bounded storage is sufficient: restart invalidates proposals; do not add a database. Unknown completion after interruption must not auto-retry.

- `open_youtube`: open exactly `https://www.youtube.com/` through the normal browser association. No arbitrary URL or search expansion.
- `create_notepad_note`: create a new uniquely named `.txt` in a dedicated MAZ notes directory using exclusive creation, then launch the trusted Windows Notepad executable with that file as a separate argument and no shell. Preserve every existing file. Reject an unsafe/reparse destination. Report note creation and application launch separately if launch fails.
- Return a receipt to the Cardputer. Say "launch requested" until physical PC evidence establishes the window appeared; never equate model intent with execution success. Tests mock launches; physical acceptance uses harmless synthetic text.

Minimal endpoints may be `/pc/propose` and `/pc/confirm`, following existing authenticated API conventions. Reuse the established audio upload/STT boundary with explicit action intent. Do not route these through PLAN, generic shell execution or privileged agent jobs. A cloud outage returns unavailable; it must not silently claim the cloud interpreted a deterministic command.

## Batch C execution order and stop conditions

1. Port route/error compatibility; remove the PLAN opening network block; make diagnostics truthful; test the affected host/firmware contracts. Demote Home extras without deleting them.
2. Build once, then install only through the verified safe app path. Record source commit, binary SHA-256 and observed firmware version. Preserve Launcher, partitions, NVS, SD and captures. Existing physical v1.0.0 evidence does not validate this candidate.
3. Prove CALL and Brain Dump, then PLAN and live STATUS or truthful unavailability. If audio remains unproven after one focused diagnosis/fix cycle, document the exact failure and continue independent submission assets; never replace the human voice demonstration with an unlabelled synthetic one.
4. Implement the two-action contract and focused deny/replay tests. Test physical PC effects and returned Cardputer receipts. Keep this separate from the PLAN execution boundary.
5. Run targeted tests -> relevant suites -> full host suite -> final firmware build after code stabilizes. Three consecutive end-to-end runs on the final candidate; offline recovery and cancelled/replayed action checks. Re-run affected gates after subsequent code edits.
6. Prepare the blog, connection diagram, shot list, sanitized screenshots, BOM and truthful limitations. Obtain real human voice footage/photos. CLI/synthetic speech evidence supports debugging but does not replace human interaction footage.
7. Complete a redacted working-tree AND history secret scan, licence/attribution review and artifact/private-data check before making the repository public. Do not erase/rewrite history automatically. Update FINAL-STATUS with exact checks, unresolved failures and links. No release/submission success claim without evidence.

## Submission and reuse

Use Batch A's sourced RULES.md; the entrant performs the final logged-in submission. Kickstarter, other competitions and MAZ Works client positioning reuse the evidence/template already saved in unified memory and the vault. They are follow-on deliverables, not reasons to expand this firmware. No campaign launch, reward/delivery promise or competition entry is claimed by this verdict.

Batch B did not flash hardware, execute PC actions or verify audio. It resolves the design decisions and identifies precise reliability checks; unresolved physical behavior remains a Batch C gate.
