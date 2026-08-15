# MAZ Pocket v0.6

The **friction release** for Cardputer ADV. v0.6 deliberately improves installation, pairing, updating and local-AI resilience instead of expanding the six-surface menu.

## Highest-leverage changes

- **One start button on Windows:** extract `MAZ-Pocket-v0.6-Install.zip` and double-click `START-HERE.cmd`.
- **Stable MAZ Core install:** Core is installed/updated under `%LOCALAPPDATA%\MAZ Core`, so deleting the release ZIP or Downloads folder does not break startup. Existing `.env` is preserved.
- **Core-only USB pairing:** a v0.6 Cardputer can receive the PC address/port/token without asking for or replacing known-good Wi-Fi credentials.
- **Phone-first future updates:** the hardened local portal can accept an authenticated app `.bin`, stream it to SD, verify safe size + ESP image magic + SHA-256, and stage it for M5Launcher. MAZ Pocket itself never flashes a firmware partition.
- **Local AI failover:** LOCAL tries the configured primary Ollama model and then a configured/installed backup, but never falls through to cloud. AUTO tries both local models before optional cloud.
- **Single version source:** firmware identity, install helpers and release packaging now derive from root `VERSION`, with CI guards against stale hard-coded release names.
- **v0.5.2 web hardening is included:** the old foreground-blocking portal parser is replaced by the bounded `portal_v2` state machine, LCD mirroring is authenticated, browser PC actions use the existing bounded Host worker, and interrupted firmware staging cleans up temporary files.

## Product surfaces remain focused

- **COMM** — voice assistant + safe PC controls.
- **CAPTURE** — voice/field capture + BrainDump.
- **OPS** — Agent Nudge evidence + nudges.
- **CONTROL** — Wi-Fi, MAZ Core, PC, diagnostics, settings and M5Launcher.
- **RECALL** — inbox, notes, snippets and viewer.
- **FLOW** — reminders, focus, sprint and tasks.

No extra top-level apps were added for v0.6.

## Fresh install

1. Download `MAZ-Pocket-v0.6-Install.zip` and extract it.
2. Double-click `START-HERE.cmd`.
3. Core installs/updates and starts automatically.
4. If exactly one removable microSD is detected, setup offers to copy the verified app image.
5. Put the card in the Cardputer, boot M5Launcher, select `Maz-Pocket-v0.6-M5Launcher.bin`, Install, then Launch.

The legacy `INSTALL-MAZ-POCKET.cmd` microSD helper remains available as a fallback.

## Normal phone update after the hardened portal is installed

1. Download `Maz-Pocket-v0.6-M5Launcher.bin` to the phone.
2. Open `http://mazpocket.local` on the same LAN and unlock it.
3. Choose the `.bin` under **STAGE NEXT FIRMWARE**.
4. Verify/stage it to SD.
5. Return to M5Launcher and install the staged app image.

## What was deliberately deferred

A prior experimental branch implemented realtime microphone-frame streaming plus a much broader async/protocol refactor. The useful low-risk local-model failover work was brought into v0.6, but realtime COMM streaming is **not** merged wholesale. Durable WAV/HTTP remains the production voice path until physical Cardputer ADV testing proves streaming is at least as reliable under Wi-Fi/Core loss, audio cycles and memory pressure.

## Safety boundaries

- Firmware remains an **app-only M5Launcher image**. Never flash it at address `0x0`.
- M5Launcher owns firmware installation and rollback.
- CI enforces the known `0x180000` Launcher app ceiling.
- Generic ArduinoOTA/direct self-flashing remains disabled.
- PC/Core actions stay allow-listed; there is no arbitrary remote shell.
- Maz Works stores no private Core token or endpoint.

## Physical validation gate

CI can validate source/tests/image/package integrity, but the real Cardputer ADV still has to prove the field path. Before treating v0.6 as hardware-proven, run:

- 30+ minute soak with no reset/watchdog event;
- 20+ COMM turns, including exiting/re-entering COMM mid-request;
- Wi-Fi and MAZ Core loss/reconnect;
- phone LCD polling and firmware staging;
- SD absent/unreadable/near-full and interrupted upload cases;
- Core-only USB pairing;
- repeated microphone record/playback cycles;
- M5Launcher reinstall and rollback;
- inspect `[health]` and `[host-worker]` measurements for heap/stack/loop headroom.
