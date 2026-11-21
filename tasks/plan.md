# MAZ Pocket: recovery, Core GUI, Project14 release and Mazwrks showcase

Date: 2026-09-06. Status: ready for executor review. This is a plan, not a claim of implementation or hardware acceptance.
Primary repository: C:/Users/manaz/maz-pocket.
Execution model: GPT-5.6 Sol or another lower-cost coding model. Use the accepted architecture below; escalate only a concrete unresolved design/security decision. No repeated model-switch stops are required by this new handoff.

## Outcome and priorities

Deliver a reliable Cardputer ADV connected to one understandable MAZ Core Windows client, a separately versioned Project14 edition, and an evidence-backed Mazwrks showcase. Preserve existing working features and device/user data. Keep CALL, CAPTURE, AGENTS and CONTROL as the competition Home. PLAN never executes. Only confirmed fixed-homepage YouTube and fresh Notepad-note actions belong in this release.

Order: recover and identify baseline; make installation safe; restore pairing and truthful health; prove a real dictated PC note and returned Cardputer receipt immediately; complete competition acceptance and submission assets; deliver the minimal Core GUI without holding the competition build hostage; publish reviewed release and Mazwrks showcase. Work on one implementation slice at a time. Independent read-only reviews may run in parallel. Task numbers identify work packages, not a requirement to delay task 8 until GUI polish is complete.

## Full prior-session audit — controlling corrections

The original session is 01a07315-2925-77e0-9b18-e75266c5bc28 (5–6 September). See docs/element14/SESSION-REQUIREMENTS-AUDIT.md for timestamped requirement coverage and historical evidence. Read the existing BATCH-A-HANDOFF.md, BATCH-B-ASTRA-VERDICT.md and PHYSICAL-EVIDENCE-2026-09-05.md once when implementing the relevant slice. Do not restart completed batches or treat old evidence as a current pass.

- Target entrant submission on 12 September, with 13 September 2026 at 23:59 UK as the recorded hard deadline. The actual published entry URL/timestamp remains part of completion, not merely a drafted article.
- First hardware milestone is a real dictated follow-up note: physical Cardputer microphone, cloud interpretation, concrete preview, ENTER confirmation, fresh Notepad file on this PC, and a receipt back on Cardputer. Test typed input separately. Ordinary CALL and PLAN must not acquire execution powers.
- Tkinter is a new implementation recommendation for the later requested GUI, not a historical user-selected toolkit. Keep it isolated in the host client; no firmware GUI framework migration. Competition evidence can be completed before tasks 6–7. If GUI changes affect Core behavior, rerun affected acceptance on the released combination.
- Keep Teach Demo, spoken replies, CREW and RETRO optional unless already reliable and physically evidenced. No new radios, sensors, PCB, inference/agent framework, WORK or telemetry expansion. Preserve existing features and stable app IDs under deeper menus.
- Use stronger-model review for unresolved architectural/security decisions and one skeptical release review, with routine execution on the cheaper model. Do not repeat completed Batch A/B or block routine work on model-label checks.

## Verified planning snapshot

- Branch agents/element14-competition, HEAD 21484a8. Two local commits ahead of remote: ce993f2 and 21484a8. Remote PR #36 head 2dbfab35b6575edbf7a66ad824c78011a147a2c0 has passing CI; that does not validate local changes.
- Dirty host/firmware changes replace PC action client IDs with live session IDs. Documentation and media drafts remain uncommitted. Existing staged renames preserve tasks/plan.md and tasks/todo.md as tasks/v1-work-plan.md and tasks/v1-work-todo.md. Preserve those.
- Untracked logs and dist_download/ require classification; do not blanket-add them. Never publish local configuration, tokens, captured client content or downloaded debug artifacts.
- Read-only Windows enumeration identifies COM5, USB VID_303A PID_1001. No serial connection, reset, flash or current firmware identification was performed in this planning session.
- Prior session reported 88 host tests and a successful firmware build. These are historical; fresh checks are required after final code changes.
- Existing v0.8.0 release targets 853489e; current competition VERSION also says 0.8.0. Choose a new unused version/tag and keep existing releases intact. Prior device v1.0.0 is also historical; do not disguise a downgrade through renaming.
- PR #35 is an open, failing-CI selective donor, never an automatic wholesale merge.
- Graph results include files absent from this branch. Re-index or verify file presence before using graph results.
- Mazwrks site: C:/Users/manaz/mazos-site, GitHub manazoid4/mazos-site. Read its own instructions and current project status before edits.
- Existing RULES.md records 13 September 2026, 23:59 UK deadline and entrant-only final submission. Recheck official rules before submission.

## Architecture decisions

1. Reuse the existing Python Core, authenticated API, worker and Windows launcher. Add a thin Tkinter window and a small lifecycle controller in the logged-in user session; defer tray dependencies and standalone executable packaging. Tkinter availability must be checked in the installed Python before implementation. Do not introduce Electron, another backend, or a privileged Windows service. Interactive browser/Notepad actions must run in the user's desktop session.
2. The GUI is the daily entry point: Connection, Pair device, AI route, Start/Restart and sanitized diagnostics. Display the actual running Core version/address/port and device firmware compatibility. Do not conflate configured, reachable and tested.
3. USB bootstrap is the primary local Cardputer pairing path. Short-code phone pairing is a separate coherent slice; a phone token exchange is not Cardputer bootstrap. Keep strong underlying credentials, preserve existing paired clients, and never require typing/copying a long token.
4. Installer changes must fail before any destructive operation when identity, space or recovery is uncertain. No raw erase, sibling-app removal, shrinking data or automatic history rewrite.
5. Freeze a separate competition release without importing WORK/v1 expansion. Reuse proven evidence for Mazwrks, future competitions and Kickstarter readiness. A Kickstarter campaign itself is not ready solely because the competition build works.

## Execution tasks

### 1. Inventory and preserve the current work (small)

Files: tasks/plan.md, tasks/todo.md, docs/element14/FINAL-STATUS.md.
Record git status, staged/unstaged diff, HEAD, remote PR heads, installed Core process/port/version and read-only USB identity. Review the two unpushed commits and session-binding edits together. Classify each untracked item without printing secrets. Do not reset, stash blindly or commit other people's work.
Acceptance: every pending source/doc change has an explicit keep/review decision; remote and local evidence are distinct; previous plans remain preserved.
Verify: git diff --check; scoped diffs; current PR/CI query. Dependency: none.
Also audit controlled repository presentation: README/docs, PR title/body and release notes. Remove irrelevant promotional/session metadata such as generated-with banners and private session links; preserve required licence and author attribution. Do not rewrite Git history just to remove old promotional metadata. Record the checked surfaces and edits; secret scanning is a separate gate.

### 2. Make Launcher inspection and preparation fail safely (medium)

Files: scripts/launcher-device.py, scripts/install.ps1, focused installer tests.
Add a read-only inspect operation using existing partitions() protocol only when Launcher is confirmed. It must not reset/delete/create/flash. Current prepare() deletes all mazpo-prefixed app partitions before checking free space; replace that ordering with a complete preflight. Validate exact intended MAZ slot, layout, capacity, retained storage and a recoverable app image before replacement. Ambiguous identity or insufficient space must stop before writes. Record allowed write range and prove other partitions unchanged.
Acceptance: ambiguous slots, insufficient space and missing Launcher perform zero mutating commands; valid known layout preserves Launcher/data/sibling apps; supplied binary hash is recorded and used consistently.
Verify: tests asserting command order and absence of mutation on failures; read-only partition report on actual Launcher. Dependency: 1.

### 3. Restore one Core instance and truthful health (medium)

Files: host/mazhost/app.py, host/mazhost/device.py, host/tests/test_app.py.
Fix the reviewed diagnostics contract mismatch: DeviceMonitor.status() uses connection_state, while /diagnostics reads connected. Derive health from affirmative recent evidence, not a nonempty dictionary. Identify running installed Core versus checkout; use the intended version without overwriting configuration. Resolve duplicate startup/process ownership deliberately.
Acceptance: connected/unavailable/stale/unknown states are distinct; the GUI/API identifies the active version and actual listener; restart preserves settings.
Verify: focused diagnostics tests and a live redacted health response before/after one restart. Dependency: 1.

### 4. Build and install a coherent recovery candidate (small)

Files: VERSION and existing version metadata as required by scripts/check-version.py; docs/element14/FINAL-ACCEPTANCE.md.
Select an unused release version consistent with firmware/Core compatibility. Run host tests, version/handoff checks and firmware build. Before flash re-enumerate USB and confirm Launcher through the audited path. If current firmware cannot hand back, use manual Launcher entry already offered by the user. Do not infer Launcher readiness from USB presence.
Acceptance: exact commit/version/binary SHA-256 recorded; observed boot and ADV keyboard startup match candidate; retained settings/data and Launcher remain intact.
Verify: commands below plus actual device observations; stop only this operation if safe installation is unavailable. Dependencies: 2, 3.

### 5. Make USB Cardputer pairing deterministic (medium)

Files: host/pair.ps1, src/net/control.cpp, relevant network configuration code, focused pairing tests.
Reuse MAZCOREPAIR. Replace first-interface/first-device assumptions with the identified device and actual Core LAN endpoint/port; wait for serial readiness instead of relying on 300ms startup. Handle stale tokens and firmware mismatch visibly without changing Wi-Fi. Suppress raw token output, including any reused setup.ps1 output.
Acceptance: first-time and stale-credential pairing work without long-token entry; invalid/ambiguous device selection writes nothing; restart/reconnect retains pairing and existing user data.
Verify: focused malformed/wrong-device/timeout tests, physical pair and protected health request, restart/reconnect. Dependencies: 3, 4.
Release the existing serial monitor before pairing/flashing and reacquire it afterward; close serial handles in finally. Check actual tab/newline framing before diagnosing rejected CLI commands as token or firmware failure. An attached USB interface does not prove an authenticated LAN connection. Keep a direct-IP recovery path if mDNS fails.

### 6. Deliver the minimal Core connection GUI (medium, split if over five files)

Implement this as two small sequential slices: (6a) host/mazhost/desktop_runtime.py plus host/tests/test_desktop_runtime.py; (6b) host/mazhost/desktop.py plus host/OPEN-MAZ-CORE.cmd. The controller starts the exact Core interpreter hidden, waits for authenticated health with a deadline, reuses a healthy owned instance, and never kills an unknown process occupying a port. The Tkinter window keeps network/serial work off the UI thread. Connect one clear status screen to the real active Core: device state, pairing action, version mismatch, selected versus effective AI route, and actionable error. Show progress and retry rather than false success. Keep credentials out of HTML, logs and screenshots.
Acceptance: user can open Core, identify a disconnected or mismatched device, pair via USB and see verified connection; Core-down and route-down states stay truthful; window resizing and keyboard use work. Show configured and effective model via existing /models. Route selection must persist through a validated backend change; if none exists, add a separately tested settings slice rather than a cosmetic dropdown.
Verify: lifecycle tests cover duplicate start, unknown occupied port, startup failure, owned restart and timeout; real Windows GUI tests cover launch, no Core, no Pocket, unplug/replug, pair and provider failure. Browser-check the existing phone portal only if changed. Dependencies: 3, 5.

### 7. Finish launcher/lifecycle and optional phone short-code pairing (two separate small slices)

First update host/install-core.ps1 and host/README.md to launch the user-session companion through existing Startup packaging: one entry point, one owned Core process, Open/Restart/Quit, preserved config. Remove raw token printing and unconditional setting overrides. Defer a tray library. Then port only the coherent phone short-code implementation/tests from the recovery branch if required for the GUI. Do not mistake authenticated /pair/start for initial Cardputer provisioning.
Acceptance: repeated launch creates no duplicate Core listener; normal restart keeps configuration and desktop PC effects work; short codes, if included, expire and are single-use without invalidating current clients.
Verify: normal-user launch/login/restart checks; expired/wrong/replayed-code tests; verify confirmed actions still run in user's desktop session. Dependencies: 5, 6.

### 8. Freeze and prove the final competition candidate (medium evidence task)

Files: docs/element14/FINAL-ACCEPTANCE.md, FINAL-STATUS.md and scoped fixes only.
Review PC proposal session ownership, expiry, atomic consumption, cancel and replay behavior. A session's existence alone must not be mistaken for per-client ownership. Run focused tests, full host suite, final firmware build once code stabilizes; commit source and rebuild from that exact commit; record hash and reflash through safe path.
Run three consecutive physical trials of real human CALL, Brain Dump, responsive online/offline PLAN, live or truthfully unavailable STATUS, confirmed YouTube and fresh Notepad note with device receipt, cancellation/replay, reconnect/reboot/Launcher return. Record provider and result for the same turn. Preserve captures and existing files.
Acceptance: final matrix has actual evidence for every claimed behavior; no synthetic speech is presented as human voice; no CLI success is presented as observed PC window or heard audio.
Verify: FINAL-ACCEPTANCE.md plus physical observation/media. scripts/accept-device.py is supplementary: it targets older screens and synthesized PC speech. Dependencies: 4, 5 and required reliability fixes; tasks 6–7 are not prerequisites for early competition proof. Rerun affected gates after GUI/lifecycle changes if bundled in the final release.

Required focused checks carried forward from Astra:
- PC actions: typed and dictated requests; fixed https://www.youtube.com/ only; plain note text 1–4000 characters, no NUL; no model-supplied path, executable or shell; reject extra fields and unsupported/ambiguous requests without effects. Show normalized preview and allow cancellation/rephrase.
- Proposal security: authenticated endpoints; real session ownership; 120-second expiry; atomic single consumption; repeated confirm returns the existing receipt; cross-session, unknown, expired and post-restart proposals cannot execute. Interrupted/unknown completion must not auto-retry. Dedicated uniquely named notes use exclusive creation; unsafe/reparse destinations rejected; file creation and application launch reported separately.
- PLAN: no synchronous discovery prerequisite on open; ESC stays responsive offline. Malicious model JSON cannot override server-owned ok/kind/provider/authority or invoke an executor. STATUS must name current source/time or say unavailable.
- Routing: preserve symbolic and legacy numeric saved routes. Prove the existing selected cloud route and independent local fallback with actual replies; report actual provider, not only configured route. Exercise provider unavailable, timeout, invalid credential/model and malformed response without unhandled 500s or false success. No new backend. Do not silently label local/deterministic actions as cloud-interpreted.
- CONTROL: expose existing/cheap truthful Wi-Fi state, RSSI, Core reachability, measured request latency, selected/effective route, local/cloud readiness, agent availability and pairing state. Unknown or stale values stay labelled; no new telemetry expansion.
- Physical failure/recovery: actual Wi-Fi interruption, Core restart and reconnection; no data loss. Preserve the pre-existing raw Brain Dump capture created by the prior surface test. Opening Brain Dump itself can start recording, so it is not a read-only probe.
- Audio diagnosis: active CALL maps through CommApp/comm.cpp and host_worker, not the legacy CallApp. Check beginTake, worker state, voice::start and real key transition before changing audio. Acknowledged injected keys do not prove recording. Record observed recording, saved audio, transcript, provider and displayed result for the same turn.
- Historic boot output reported internal storage and an SD FAT mount error. Do not format SD or claim SD works; document actual storage mode and preserve contents. Serial open caused USB reset in prior attempts, so keep discovery read-only and serial testing deliberate.

### 9. Push reviewed project work and clear public release gates (medium)

Files: release metadata, docs/element14/FINAL-STATUS.md, release notes/checklist.
Reviewed source commits may be pushed to the existing agents/element14-competition branch before hardware completion, with PR #36 accurately marked pending physical acceptance. Use explicit file staging. Never push project main directly. Review full working tree, all reachable Git history, new packages AND existing releases/assets for secrets/private data and licenses (public visibility exposes historical assets too); keep scanner reports redacted. A secret finding requires containment, not automatic public exposure or history rewrite.
After source review and passing checks, merge through the PR workflow (never a direct main push). If merge changes source, rebuild and rerun affected acceptance against that final source. Acceptance: PR contains intended changes only, CI passes at its exact head, public gate has recorded scope/results; no reused v0.8.0 artifact/tag; release links identify final binary/hash and limitations.
Verify: remote head and CI, downloaded release hash, installation instructions against package. User's request authorizes reviewed pushes; do not repeatedly seek push permission. Public visibility/release follows satisfied gates. Dependencies: 1 for branch pushes; 8 and clean public gate for published tested release.

### 10. Finish Project14 evidence and entrant handoff (medium)

Files: docs/element14/SUBMISSION-DRAFT.md, VIDEO-SHOT-LIST.md, CONNECTION-DIAGRAM.svg, component/BOM document, FINAL-STATUS.md.
Produce a step-by-step build story, component part numbers, reproducible install steps, origin/design choices, measured limitations, real photos and short video showing human, Cardputer, Wi-Fi, PC/Core, actual AI/agent and returned result. Use harmless synthetic note content but genuine human interaction. Recheck official rules and eligibility requirements.
Acceptance: one complete functional-project blog package with video and photographs; each claim maps to final evidence; entrant submission URL and timestamp recorded only after actual submission.
Verify: open rendered assets/links, inspect media for secrets, check official page/terms. The saved terms prohibit agent submission: the entrant must perform that final action. Continue independent showcase work while waiting. Dependency: 8; final public links from 9.

### 11. Build and publish the Mazwrks showcase (medium)

Repository: C:/Users/manaz/mazos-site. Read local instructions and bounded wiki/projects/maz-works/STATUS.md first; identify existing case-study route/component before editing. Branch agents/maz-pocket-showcase. Reuse app/projects.ts and app/work/[slug]/page.tsx, public/maz-pocket-*.webp, app/sitemap.ts and tests/static-export.test.mjs. Target /work/maz-pocket. Use Lab/prototype positioning until stronger product evidence exists; preserve JobFilter/Scrap Finance flagship hierarchy.
Show what Pocket solves, how the physical connection works, actual device/PC photos/video, demonstrated features, honest limitations, repository/release link and a relevant contact/demo CTA. Do not invent clients, testimonials, speed measurements, sales or production readiness.
Deliver one recognizable sanitized client scenario and pilot proposition: dictate a fictional customer follow-up, review the bounded action, create a new PC note, and show the useful output. Explain intended user, problem, supported workflow, setup/support boundaries and demo/pilot next step. Do not imply completed CRM integration, paying customers or measured ROI. Site publication follows the competition handoff/submission when possible; independent drafting must not delay the hardware/blog deadline.
Acceptance: complete case-study page discoverable from existing work listing, responsive accessible media, working links and truthful claims; deployed production URL verified.
Verify: npm run verify (includes typecheck, production build, tests and smoke); browser desktop/mobile checks; PR/CI and production page after authorized deployment. Dependency: 8; published release links from 9.

### 12. Persist reusable launch kit and next backlog (small)

Files: docs/element14/FUNDING-AND-REUSE.md; relevant unified-memory plan/status/ledger; Obsidian project/session notes and Local Knowledge mirror.
Store sanitized assets and a reusable competition-entry template. Record Kickstarter readiness gaps: offer, costs/BOM, supply, fulfillment/support, campaign audience, risks and evidence. No reward, price, delivery or campaign launch promises without a concrete later decision.
Canonical reusable template: C:/Users/manaz/unified-memory-database/plans/competition-and-crowdfunding-launch-template.md. Extend it with entry fields for official sources, eligibility, deadline/timezone, judging, format/media, source/IP requirements, submission owner, evidence threshold and confirmed submission URL/time. Create an owned follow-on register for other competitions and Kickstarter, with next action and evidence owner per item. Research specific new competitions when selected; do not turn this release into an unbounded worldwide search or silently submit additional entries.
Create a concrete funding-readiness brief covering the pilot offer, prototype BOM/cost evidence, supply/fulfilment/support assumptions, audience/prelaunch work, risks and next validation experiment. Campaign launch remains a later decision. Assign factual collection/asset tasks to the execution model and legal, pricing, delivery and entrant decisions to the human.
Mandatory closeout after every session: memory.py end with evidence and next action; explicitly stage only relevant unified-memory files; commit and push its agents/ branch to GitHub; verify the remote branch contains the new commit and record its URL. A local memory.py end or Obsidian push alone is not completion. Also mirror and push the knowledge vault to fork main. Preserve unrelated dirty files.
Acceptance: another model can resume from one bounded handoff; all remote commits/PRs and outstanding human steps recorded; both vault stores and unified memory updated without unrelated changes.
Verify: saved links and git push results. Dependencies: 9–11, but write incremental handoffs whenever blocked.

## Commands and execution rules

Use PowerShell in C:/Users/manaz/maz-pocket. Verify tool paths before use:
- Read-only hardware: Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name,PNPDeviceID
- Tests: .\host\.venv\Scripts\python.exe -m pytest host/tests -q
- Versions: .\host\.venv\Scripts\python.exe scripts/check-version.py
- Launcher contracts: .\host\.venv\Scripts\python.exe scripts/check-launcher-handoff.py
- Package/build: .\scripts\package-release.ps1 (first verify the selected Python has PlatformIO)
- Hash: Get-FileHash .\dist\maz-pocket-app.bin -Algorithm SHA256
- Only after task 2 gates: .\scripts\install.ps1 -Port <reidentified-port> -Binary .\dist\maz-pocket-app.bin
- Only after task 5 endpoint/device checks: .\host\pair.ps1 -Quiet

Keep logs private and redacted. No .env dumps or raw token output. A detached helper must launch hidden. Do not open the serial port concurrently from multiple tools.
After two focused failed fix cycles, record exact evidence and move to independent deliverables; do not declare blocked work complete.
Use short chat updates. Keep persisted docs and code in normal precise English.
Do not rerun firmware builds after documentation-only edits. Any subsequent source edit invalidates affected final-build acceptance.
The executor should update tasks/todo.md and FINAL-STATUS.md after each checkpoint.

## Review synthesis and stop boundaries

Hardware review found partition mutation before capacity validation. Product/GUI review found stale graph entries, missing short-code pairing on this branch, and diagnostics contract mismatch. Release review found an existing v0.8.0 tag and outdated status evidence. These are first-class tasks, not optional cleanup.
No implementation, device mutation, application launch, project release or competition submission occurred during planning.
Stop only a dependent unsafe operation; continue authorized independent work. Human handling/filming and entrant-only submission remain explicit evidence dependencies, not fabricated automation successes.
