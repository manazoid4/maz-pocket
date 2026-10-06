# MAZ Pocket — CLAUDE BUDDY Build Prompt (v1)

Controlling prompt for a local build agent. Follows `docs/templates/RESEARCH-CRITIQUE-BUILD-EXECUTE-PROMPT-TEMPLATE.md`: you produce a V2 of this prompt before implementing, then execute V2 end to end.

---

## 0. CONFIGURATION

```text
PROJECT_NAME=MAZ Pocket — CLAUDE BUDDY
REPOSITORY=manazoid4/maz-pocket
BASE_BRANCH=main
CURRENT_RELEASE=0.8.0 CONTROL (v1.0.0 WORK planned in tasks/todo.md)
TARGET_RELEASE=next minor after reconciling with the v1.0.0 WORK plan (agent decides; record why)
TARGET_OUTCOME=Maz watches every Claude Code / Claude desktop session from his Cardputer and approves or denies tool calls with one key, without touching the PC.
PRIMARY_USER=Maz — solo dev, runs several coding agents at once, often away from the desk (security shifts, field work)
PRIMARY_PLATFORM=ESP32-S3 (M5Stack Cardputer ADV, StampS3A, 8 MB flash, NO PSRAM) + Windows MAZ Core host

EXISTING_PLAN_PATH=docs/HARDWARE-BUDDY-BUILD-PROMPT.md (this file)
V2_PROMPT_PATH=docs/HARDWARE-BUDDY-BUILD-PROMPT-V2.md
RESEARCH_OUTPUT_PATH=docs/research/HARDWARE-BUDDY.md
AUDIT_OUTPUT_PATH=docs/audits/HARDWARE-BUDDY-AUDIT.md

NON_NEGOTIABLES:
- Exactly six Home surfaces. CLAUDE BUDDY lives inside AGENTS (stable id "buddy").
- App-only M5Launcher image; 0x180000 app-image ceiling stays enforced in CI.
- BLE link encrypted: LE Secure Connections, passkey bonding, encrypted-only NUS characteristics.
- Desktop-supplied data (hints, transcript lines, ids, filenames) is untrusted display data. Never executed, never used as a storage path.
- The device never auto-approves. Every approval is a deliberate human keypress.
- Existing v0.8 behaviour, stable app IDs and release packaging rules (docs/RELEASE_RULES.md) preserved.

CORE_REQUIRED_OUTCOMES:
- Pair Cardputer with the Claude desktop app (Developer > Open Hardware Buddy) and show the 6-digit passkey.
- Live session view: running / waiting / total, one-line msg, recent entries, tokens today.
- Pending permission prompt raises a toast + sound from ANY screen; Y = approve once, N = deny, inside CLAUDE BUDDY.
- Correct ack for every desktop command (status, owner, name, unpair); unpair erases bonds.
- BLE off by default; enabling persists across reboots; Wi-Fi + MAZ Core keep working with BLE on.

KNOWN_CONSTRAINTS:
- No PSRAM: Wi-Fi + BLE + LVGL/canvas + audio share internal heap. Measure, don't guess.
- Flash ceiling 0x180000 for the app image. Arduino Bluedroid BLE is large; NimBLE-Arduino is the likely fix.
- Network/AI work stays off the UI task; reuse the single bounded Host worker, no new FreeRTOS workers unless proven necessary.
- BLE API only exists when the Claude desktop app (macOS/Windows) is in Developer Mode. Not an officially supported Anthropic feature.

DO_NOT_EXPAND_INTO:
- Folder/character push (char_begin/file/chunk). Deliberately unsupported: don't ack char_begin.
- Animated pet / GIF character packs (later, separate release).
- Phone (Android) BLE relay, voice approvals, turn-event inbox — note as follow-ups only.
- Any new top-level Home app.

PRIMARY_FINAL_ARTIFACT=Standard MAZ Pocket release set per docs/RELEASE_RULES.md, plus a PR-ready branch Maz can push.
```

---

## 1. CONTEXT YOU MUST LOAD FIRST (Stage 0)

Learn how Maz works before changing code. Read, don't skim:

1. **This repo**: README, QUICKSTART, CHANGELOG, RELEASE_NOTES, `docs/RELEASE_RULES.md`, `docs/VERIFICATION.md`, `docs/V100-*`, `tasks/plan.md`, `tasks/todo.md`, `docs/audits/*`, `.github/workflows/*`, `platformio.ini`, `partitions.csv`.
2. **App framework**: `src/core/app.h`, `src/core/shell.*`, `src/apps/registry.cpp`, `src/apps/surfaces.cpp` (AGENT_ITEMS), `src/core/notify.*`, `src/audio/sfx.*`, `src/ui/ui.*`, `src/ui/theme.h`, `src/core/settings.*`, `src/main.cpp`, `src/net/host_worker.*`. Copy existing conventions (namespaces, fonts, header/hint bars, toasts, Preferences/NVS use).
3. **Maz's memory / knowledge system**: find where his durable context lives — MAZ Core skill vault (`host/mazhost/skill_vault.py`, `skill_routes.py`), braindump/teach outputs, `~/.maz-pocket/`, any unified memory database on this machine, CLAUDE.md / AGENTS.md files, past build prompts in `docs/`. Record in the audit: where it lives, its schema, how Maz expects agents to read/write it. **Do not invent a schema.** If you can't find it, say so and ask; don't block the firmware work on it.
4. **Agent Nudge integration** (AGENTS > STATUS): CLAUDE BUDDY must sit next to it, not duplicate it. Decide whether Buddy's session counts should appear in Agent Status.
5. **Draft code already in the working tree (unverified, never compiled):**
   - `src/buddy/ble_link.{h,cpp}` — NUS + LE Secure bonding, adapted from Anthropic's `ble_bridge.cpp`
   - `src/buddy/buddy.{h,cpp}` — protocol: line buffer, heartbeat parse, acks, status, permission send via ArduinoJson
   - `src/apps/claude_buddy.cpp` — the screen (OFF / PAIRING / WAITING / LINKED / APPROVE)
   NOT yet done: `makeClaudeBuddy()` declaration in `apps.h`, registry row, AGENT_ITEMS row, `buddy::boot()` in setup, `buddy::update()` in loop, third_party licence copy, docs. Treat the draft as a proposal to audit, not truth.

## 2. UPSTREAM REFERENCES (Stage 1 research, minimum)

- `anthropics/claude-desktop-buddy` — official reference firmware + `REFERENCE.md` wire protocol (MIT; bufo GIFs are NOT MIT — don't copy them).
- `espressif/esp-desktop-buddy` — ESP-IDF library.
- Community ports/forks: Cardputer variants (`dakshaymehta/cardputer-claude-os`, cwc-makers / `moremas/build-with-claude`), `vthinkxie/claude-desktop-buddy-esp32`, `lennonkc/openbuddy`, Clawdmeter, `argeuthiesen/claudinho`.
- NimBLE-Arduino docs (flash/RAM footprint, security API, bonding store).
- ESP32-S3 Wi-Fi/BLE coexistence guidance (Espressif docs).
Check licences for anything you borrow; record in `docs/research/LICENCES.md`.

Protocol summary (verify against REFERENCE.md, it wins):
- NUS service `6e400001-…`, RX write `6e400002-…`, TX notify `6e400003-…`; advertise a name starting `Claude`.
- UTF-8 JSON, one object per `\n`. Desktop heartbeat on change + every 10 s; >30 s silence = dead link.
- Heartbeat: `total, running, waiting, msg, entries[], tokens, tokens_today, prompt{id,tool,hint}`.
- Reply: `{"cmd":"permission","id":<exact prompt.id>,"decision":"once"|"deny"}`.
- Every desktop `cmd` gets `{"ack":<cmd>,"ok":bool,"n":0}`; `status` ack carries `data{name,sec,bat,sys,stats}`.
- One-shot on connect: `{"time":[epoch,tz]}`, `{"cmd":"owner","name":…}`.

## 3. CRITIQUES TO RUN (Stage 2)

Run each as an independent perspective and write findings to the audit:
- **Flash/heap engineer**: measure current image size and free heap (Wi-Fi up, Call MAZ active, Buddy on). Bluedroid vs NimBLE. Can Buddy fit under 0x180000? If not, NimBLE or cut.
- **Security**: bonding flow, bond storage, `sec:true` only after encryption, injection via `prompt.id`/hint (must be serialised, never string-spliced), oversized line handling, stale prompt after disconnect (must clear, never approve a stale id).
- **UX on a 240×135 screen**: prompt visible from any screen; can Maz read what he's approving (wrap the hint, never truncate silently); a misclick must never approve — consider requiring Y on the Buddy screen only, or a global Fn+Y with a confirmation; quiet-hours behaviour.
- **Product**: is this actually used daily? What would make Maz pick the Cardputer up instead of alt-tabbing? Kill anything that doesn't serve that.
- **Release**: CI, packaging, version reconciliation with the in-flight v1.0.0 WORK plan.

## 4. BUILD (Stage 3 — execute V2)

Minimum shape (V2 may improve it):
1. BLE stack choice made and justified with measured numbers.
2. `src/buddy/` module finished; lazy BLE init; bounded per-loop RX work; 30 s stale timeout.
3. CLAUDE BUDDY app wired into registry + AGENT_ITEMS; global toast on new prompt.
4. Status ack includes battery, uptime, heap, approve/deny counts.
5. Settings: Buddy on/off visible in CONTROL > SETTINGS (off requires reboot is acceptable; say so on screen).
6. `third_party/claude-desktop-buddy/LICENSE` + attribution in adapted files.
7. Docs: `docs/HARDWARE-BUDDY.md` (setup in ≤6 steps, troubleshooting), README feature row, CHANGELOG, RELEASE_NOTES, QUICKSTART.

## 5. VERIFY (Stage 4)

Agent-verifiable (must pass before handoff):
- `pio run` clean, no new warnings in `src/buddy/`; app image < 0x180000 (print the size).
- Host-side protocol test: a Python script (bleak or a serial shim) that replays heartbeat, prompt, status, unpair, oversized line, malformed JSON, `char_begin` — asserts correct acks/no acks.
- Unit-style tests for the line parser if the build allows native tests.
- All existing CI workflows green; `scripts/check-version.py` passes.

Operator checks (list them for Maz, don't fake them):
- Pair with Claude desktop on Windows; passkey shown and accepted.
- Real Claude Code prompt → toast → Y approves, N denies, desktop reflects it.
- Wi-Fi + Call MAZ still work with BLE on; no reboot over a 1-hour session.
- Forget in Hardware Buddy window → re-pair shows fresh passkey.

## 6. HANDOFF

- One branch `feat/claude-buddy`, small reviewable commits, PR description with measured size/heap before/after.
- Do NOT push the `.release/` marker; Maz releases.
- Final report: what shipped, measurements, operator checklist, follow-ups, risks.

## 7. OPEN-SOURCE + CLAUDE FOR OPEN SOURCE (prepare, Maz submits)

Facts (verify current terms at claude.com/contact-sales/claude-for-oss before relying on them):
- Program = 6 months Claude Max 20x for eligible maintainers/contributors; rolling review.
- Headline bar has been primary maintainer of a repo with 5,000+ stars or 1M+ monthly npm downloads; there is an "apply anyway" path, and a July 2026 expansion to contributors landing PRs. MAZ Pocket alone will not hit the headline bar.

Deliverables to maximise the odds:
1. Make `manazoid4/maz-pocket` public-ready: MIT already present; scrub secrets/tokens from history (`git log -p` scan + gitleaks), clear README with photo/GIF placeholder, contributing guide, issue templates.
2. **Upstream contribution**: prepare a clean, standalone Cardputer ADV port PR (or board-support PR) for `anthropics/claude-desktop-buddy` following its CONTRIBUTING.md. A merged PR there is the stronger application evidence. Prepare it as a separate branch/patch; Maz opens it.
3. Draft `docs/OSS-APPLICATION.md`: project summary, links, Maz's role, why it matters, merged-PR links (fill after merge).

## 8. RULES OF ENGAGEMENT

- Repo code is truth over docs, including this prompt.
- Measure before claiming. No "should fit", "should work".
- If blocked (hardware, credentials, memory DB location), state the exact blocker, do everything else, and stop only that thread.
- Concise output. No restating this prompt back.
