# MAZ Pocket verification

## Version naming

Canonical releases use **v0.1 / v0.2 / v0.3 / v0.4 / v0.5**. Historical preview tags remain only as recovery/history references.

## What physical testing already established

Previous builds have been installed through M5Launcher on the real Cardputer ADV. That establishes the app-only `.bin` + Launcher path, display/keyboard/storage initialization and basic Wi-Fi/host communication.

Real use of v0.3/v0.4 also exposed the problems that define v0.5:

- useful controls were present but not discoverable enough on the Cardputer;
- Wi-Fi could exist underneath the product without feeling like a first-class recoverable feature;
- generic model answers were being asked to substitute for real PC/project evidence;
- browser flashing was a worse primary path than M5Launcher + SD;
- long PC work must never freeze the handheld.

v0.5 changes the architecture rather than hiding more features behind the same shell.

## v0.5 automated release gates

A v0.5 release is acceptable only when the final release commit passes:

- complete MAZ Core/Host pytest suite;
- Core tests for project-root containment, secret-file blocking, no generic shell, action allow-list and non-blocking job contract;
- Core performance regression test proving health/generic chat do not fan out deep Git summaries across every repo and project-list summaries use a short TTL cache;
- recovery/browser-flasher syntax + partition ownership regression tests;
- Cardputer ADV PlatformIO build;
- ESP32 application magic + minimum-size validation;
- hard **`0x180000` Launcher app-slot ceiling**;
- packaging of exact `Maz-Pocket-v0.5-M5Launcher.bin` and `MAZ-Core-v0.5.zip`;
- SHA-256 checksums for release assets;
- hidden Maz Works `/maz-core` client build/deploy check before its PR is merged.

## v0.5 behavior implemented in code

### Cardputer

- Home: COMM / CAPTURE / OPS / CONTROL / RECALL / FLOW.
- **W** from Home opens the v0.5 Wi-Fi manager directly.
- CONTROL exposes Control Center, Wi-Fi, MAZ Core, PC/COMM, diagnostics and Settings.
- Wi-Fi manager can inspect/reconnect/scan/connect/save backup/disconnect/forget/start setup hotspot.
- No usable saved network starts `MAZ-Pocket-Setup`; repeated connection failures also expose the setup AP while retrying.
- `mazpocket.local` can provision Wi-Fi, launch allow-listed device surfaces, control allow-listed PC actions, configure Core, run diagnostics, reboot/return to Launcher and stream the current 240x135 LCD buffer.
- Firmware installation remains owned by M5Launcher; generic ArduinoOTA is not enabled.

### MAZ Core

- Default local model: `lfm2.5-8b-a1b-gpu:latest`.
- Real local project discovery and bounded factual evidence replace generic model claims about PC/project state.
- Non-secret project reads/search + optional Obsidian search.
- Fixed action allow-list: git status/fetch/fast-forward pull, detected tests/build, open folder.
- Build/test actions can run as background jobs so Cardputer/Web clients poll rather than blocking.
- Optional private GitHub issue bridge uses the same allow-list and rejects arbitrary shell commands.
- Cardputer status/LCD can be proxied through Core.

### Maz Works

- `/maz-core` is an unlinked/noindex browser client.
- No Core endpoint or token is committed into the site.
- The user's browser supplies the endpoint/token at runtime.

## Physical acceptance still required after the released v0.5 `.bin` is installed

Automation cannot prove RF/acoustics/LCD byte order/physical controls or the user's actual Windows/Tailscale/Ollama environment. On hardware verify:

1. Home visibly shows COMM / CAPTURE / OPS / CONTROL / RECALL / FLOW.
2. W → Wi-Fi can scan, connect and save primary + backup networks.
3. With bad/no credentials, `MAZ-Pocket-Setup` appears and `192.168.4.1` can repair Wi-Fi.
4. `mazpocket.local` loads and its live LCD mirror matches the physical display.
5. CONTROL → MAZ CORE shows real local projects and a background `git status`/test/build can be started without freezing Cardputer input.
6. COMM uses the configured LFM2.5 model and project questions cite actual Core evidence instead of inventing state.
7. Microphone, speaker, BrainDump, Agent Nudge, reminders/focus and PC controls still behave correctly.
8. Ctrl+L / CONTROL → M5Launcher returns safely to Launcher.

A successful CI/release is **release verification**, not a claim that these final v0.5 physical checks have already happened.
