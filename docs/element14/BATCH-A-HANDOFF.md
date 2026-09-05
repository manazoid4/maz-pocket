# Batch A handoff — 5 September 2026

## Repository baseline

- Competition branch: `agents/element14-competition`, based on current `origin/main` at `f59d3a445c998f177dc6c2bbf7e59279c2da0116`.
- Competition branch `VERSION`: `0.8.0`. The attached device was running PR #35 firmware `1.0.0`; source and device were deliberately kept distinct.
- Repository visibility: private. Latest release: `v0.8.0` (18 August 2026).
- PR #35 remains separate at `0d1872facc7a8b9d09734b28df75daf9b892e6e0`; merge base `4c7e27563488a7e3e8e318444e51c5fb47a30618`; 16 commits and 5,012 additions across 67 files.
- Untracked release downloads and historical host logs were preserved unchanged.

## Rules

- Deadline: 13 September 2026, 23:59 UK time.
- Submit one completed project blog with build steps, theme explanation, video and photographs.
- Originality, innovation and technical merit have equal judging weight.
- T&Cs section 2.6 prohibits submission by an agent. The entrant must perform the final logged-in submission.
- Source code is encouraged where applicable, not stated as mandatory or required to be public. See `RULES.md`.

## PR #35 selective review

### Drop/defer

- WORK telemetry, custom trackers and client/job pipelines: explicit competition scope creep.
- `today_score`: combines unlike measures and contradicts the documented no-composite-score rule.
- Six-tile Home and premature `1.0.0` release presentation.
- Wholesale merge or history-preserving cherry-picks: three PR commits expose Claude session URLs in commit messages.

### Astra candidates

- Typed provider failures and bounded route handling.
- Symbolic route persistence and only the route configuration proven necessary for the demo.
- Short-code pairing where it directly improves setup reliability.
- M5Launcher/app-partition safety fixes, preserving all installed firmware and data.

Review the duplicated route dispatch in `host/mazhost/llm.py` before porting. Do not carry pipeline code with otherwise useful fixes.

## CI and low-risk fixes

- Latest main CI reached the firmware package step, then failed because PlatformIO no longer serves registry version `m5stack/M5GFX@0.2.26`.
- Upstream still provides tag `0.2.26`. `platformio.ini` now pins that upstream tag directly. A clean dependency resolution and `pio run -e cardputer-adv` succeeded: M5GFX resolved to upstream commit `729297d`, flash usage was 1,449,565 bytes, and the build completed successfully.
- `scripts/accept-device.py` now buffers partial USB serial reads until newline; the fix passed syntax and a live multi-surface run.

## Physical evidence

- Physical MAZ Pocket `1.0.0` identified on `COM5`; Wi-Fi and Core reached online/LAN; primary surfaces opened.
- CALL run 1 failed: no Inbox result and no proven recording/transcription/model/display/audio chain.
- PLAN surface opening coincided with a 7.8-second loop maximum and stall flag.
- Live Agent Status polling returned HTTP 503 from `/nudge`; the surface render is proven, live agent evidence is not.
- No flash, reset, Wi-Fi mutation, M5Launcher return or data deletion was performed.
- See `PHYSICAL-EVIDENCE-2026-09-05.md` for the exact claim boundary.

## Public-release gate

- Current competition branch text scan found no Claude session URL or generated-by marker.
- PR #35 commit history contains Claude co-author/session metadata; port or squash selected work instead of merging its history.
- A pattern scan found only an intentional fake credential in a redaction test. `gitleaks` is not installed, so the public gate is **not clear**.

## Astra decision request

Choose the smallest competition baseline and exact PR #35 portions to port. Resolve the CALL acceptance failure and PLAN stall before approving implementation. Specify the bounded schema and execution boundary for only `open_youtube` and `create_notepad_note`; PLAN and ordinary context queries must remain non-executing.
