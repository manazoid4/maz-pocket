# MAZ Pocket v0.5.1

The **daily-driver hardening** release for Cardputer ADV. It keeps the v0.5 six-surface product model and focuses on responsiveness, recovery, safer installation and private AI access rather than adding more apps.

## What changed from v0.5

- Active **COMM** no longer performs session creation, voice upload, optional TTS, or its small allow-listed PC command deck on the UI task. One bounded low-priority Host worker serialises those network operations.
- Leaving COMM while the PC is slow is safe: no `App*` crosses task boundaries, and failed/offline voice remains durable in the outbox.
- USB/LAN control no longer waits on `readStringUntil()` and `MAZSTATUS` no longer triggers synchronous Host network requests.
- Runtime evidence now records free heap, minimum free heap, largest contiguous allocation, main-task stack headroom and worst foreground-loop duration. The Host worker also records minimum remaining stack after HTTP/TLS/TTS work.
- Partition/update comments now match reality: **M5Launcher owns firmware installation and rollback**. MAZ Pocket does not self-flash arbitrary partitions.
- A new Windows install bundle gives the normal path: **extract ZIP → insert microSD → double-click `INSTALL-MAZ-POCKET.cmd` → M5Launcher Install**. The helper validates the ESP32 app image, refuses fixed disks and verifies SHA-256 after copying.
- Maz Works now has an unlinked/noindex `/maz-pocket-ai` capability client. Private Core URLs/tokens are supplied at runtime and are never committed into the public site. Trusted AI clients can use authenticated HTTPS, while AI clients with GitHub access can use MAZ Core's existing private `[MAZ CORE]` issue queue.

## Product surfaces

- **COMM** — voice assistant + safe PC controls.
- **CAPTURE** — voice/field capture + BrainDump.
- **OPS** — Agent Nudge evidence + nudges.
- **CONTROL** — Wi-Fi, MAZ Core, PC, diagnostics, settings and M5Launcher.
- **RECALL** — inbox, notes, snippets and viewer.
- **FLOW** — reminders, focus, sprint and tasks.

## Install

1. Download `MAZ-Pocket-v0.5.1-Install.zip` and extract it.
2. Insert the Cardputer microSD card into Windows.
3. Double-click `INSTALL-MAZ-POCKET.cmd`.
4. Safely eject the card, boot M5Launcher, select `Maz-Pocket-v0.5.1-M5Launcher.bin`, Install, then Launch.
5. Extract `MAZ-Core-v0.5.1.zip` on the PC and run `install-core.ps1` once if Core is not already installed/configured.

Manual copying of the `.bin` to microSD remains supported.

## Safety boundaries

- The firmware is an **app-only M5Launcher image**. Never flash it at address `0x0`.
- CI enforces the known `0x180000` Launcher slot ceiling.
- Generic ArduinoOTA remains disabled.
- MAZ Core exposes fixed capabilities; there is no arbitrary remote shell.
- AI capability links are secrets. Rotate the Core token if one is exposed.
- This release intentionally keeps durable WAV/HTTP for voice. It does **not** claim realtime microphone-frame streaming yet.

## Physical validation gate

CI validates build/tests/image/package integrity, but a real Cardputer ADV still has to prove the hardware path. Before treating v0.5.1 as fully field-proven, run:

- 30+ minute soak with no reset/watchdog event;
- 20+ COMM turns, including exiting/re-entering COMM mid-request;
- Wi-Fi and MAZ Core loss/reconnect;
- SD absent/unreadable/near-full behavior;
- repeated microphone record/playback cycles;
- M5Launcher reinstall and rollback;
- inspect `[health]` and `[host-worker]` serial measurements for heap/stack/loop headroom.
