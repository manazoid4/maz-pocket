# MAZ Pocket v0.6 — Audit B: firmware and Core reliability

Scope: current firmware/Core architecture plus the unmerged/conflicting streaming branch in PR #11.

## Current strong foundations

- v0.5.2 `portal_v2.cpp` replaces the old blocking HTTP request parser with a bounded state machine and adds authenticated LCD mirroring and phone-to-SD firmware staging.
- COMM session creation/upload/TTS and allow-listed PC controls already share one bounded Host worker rather than blocking the foreground UI task.
- USB serial parsing is non-blocking and status is based on cached measurements rather than synchronous Core calls.
- M5Launcher remains the install/rollback boundary; staged firmware is validated and never written directly to flash by MAZ Pocket.
- Device health exposes heap, largest block, stack headroom and loop-stall evidence.

## Reliability / maintenance findings

1. **Versioning is duplicated and already inconsistent.** `platformio.ini`, PowerShell installers, CI package names, Core output, docs and release workflows independently hard-code version strings.
2. **The old `portal.cpp` remains in-tree while `portal_v2.cpp` is selected through `build_src_filter`.** This is safe but easy for a future edit to hit the wrong file. The active path needs explicit comments/tests.
3. **PR #11 is not safe to merge wholesale.** It is based on an older tree, conflicts with current `main`, and combines audio streaming, async host layers, generated protocol schema, UI replacement, model routing and extensive refactors in one branch.
4. **PR #11 contains useful isolated work.** The lowest-risk pieces are local Ollama primary/backup failover and its tests. The highest-risk piece is realtime COMM streaming.
5. **Core install location is operationally fragile.** Startup points at the directory where the archive was extracted rather than a stable application location.
6. **Current USB pairing unnecessarily rewrites Wi-Fi credentials.** A dedicated Core pairing command can update only host address/port/token and leave known-good Wi-Fi untouched.
7. **The CI package job is version-specific.** Advancing versions requires editing many filenames rather than reading one canonical version.

## Selected technical changes for v0.6

### Safe/high leverage
- Add a canonical `VERSION` file and PlatformIO pre-build script to inject `MAZ_POCKET_VERSION` from it.
- Make CI/release packaging read `VERSION`; add a consistency guard.
- Add `MAZCOREPAIR` trusted-serial command to update Core address/port/token only.
- Make `pair.ps1` prefer the Core-only pairing path and avoid asking for Wi-Fi password unless full Wi-Fi provisioning is explicitly needed.
- Add primary/backup local model routing and tests without introducing new network protocol requirements.
- Add a unified setup script that installs Core to `%LOCALAPPDATA%\MAZ Core`, preserves `.env` across upgrades, starts Core, handles optional USB/SD helpers and opens the local portal.
- Keep v0.5.2 non-blocking portal architecture and single Host worker.

### Deferred until hardware proof
- WebSocket realtime microphone streaming and reply deltas from PR #11.
- Replacing the existing Host worker with the broader PR #11 async layer.
- Large UI renderer swaps or source-tree cleanup that could hide regressions before physical acceptance.

## Required v0.6 CI checks

- Host tests including local-model failover.
- PowerShell parser checks for all shipped setup/install scripts.
- Version consistency between `VERSION`, generated firmware define and release artifact names.
- Cardputer ADV PlatformIO build under the known `0x180000` M5Launcher app ceiling.
- Existing recovery flasher safety tests.
- Static guard that active portal is `portal_v2.cpp` and direct self-OTA remains absent.

## Physical acceptance still required

CI can prove build/test/package integrity, not the physical Cardputer. Before calling v0.6 hardware-proven: repeated COMM turns, slow/failed Core, Wi-Fi reconnect, phone screen polling, phone firmware staging, SD faults, USB Core-only pairing, M5Launcher handoff/rollback and a 30-minute soak while observing health metrics.