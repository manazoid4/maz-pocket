# MAZ Pocket

MAZ Pocket is standalone firmware for the **M5Stack Cardputer ADV** plus **MAZ Core**, a local Windows companion that supplies real project/PC context and safe actions.

## Current release: v0.6

Home stays intentionally small:

- **COMM** — voice conversation, spoken replies and safe PC controls.
- **CAPTURE** — field capture, recordings and BrainDump.
- **OPS** — Agent Nudge evidence, status and nudges.
- **CONTROL** — Wi-Fi, MAZ Core, PC/COMM, diagnostics, settings and M5Launcher.
- **RECALL** — inbox, notes, snippets and viewer.
- **FLOW** — reminders, focus, sprint and tasks.

**W from Home opens Wi-Fi directly. Ctrl+K opens everything. Ctrl+L returns to M5Launcher.**

v0.6 is the **friction release**: install, pairing, updates and local AI resilience improve without adding another top-level app. COMM keeps the proven durable WAV/HTTP transport and one bounded Host worker; realtime microphone-frame streaming remains a later hardware-tested experiment.

## Fastest setup on Windows

Fresh setup is one entry point:

1. Download `MAZ-Pocket-v0.6-Install.zip` and extract it.
2. Double-click **`START-HERE.cmd`**.
3. MAZ Core is installed/updated under `%LOCALAPPDATA%\MAZ Core`, preserving an existing `.env`.
4. If a v0.6 Cardputer is connected over USB, Core address/token pairing is attempted without changing its Wi-Fi credentials.
5. If exactly one removable microSD is present, setup offers to verify/copy the firmware there. It never formats a disk and never flashes the Cardputer directly.
6. The local portal opens at `http://mazpocket.local` when available.

M5Launcher remains the only firmware install/rollback owner.

## Normal updates from your phone

Once the Cardputer is running the hardened portal from v0.5.2+:

1. Download the new `Maz-Pocket-v0.6-M5Launcher.bin` on the phone.
2. On the same LAN, open `http://mazpocket.local`.
3. Unlock with the MAZ pairing token.
4. Choose the `.bin` under **STAGE NEXT FIRMWARE** and tap **VERIFY + STAGE TO SD**.
5. Tap **M5LAUNCHER**, install the verified staged image, then Launch.

The device streams the upload to a temporary SD file, checks safe size bounds, ESP image magic and SHA-256, then promotes only a verified image. MAZ Pocket never writes firmware partitions itself.

## Wi-Fi that is always recoverable

CONTROL → WI-FI can show status, reconnect, scan/connect primary Wi-Fi, save a backup network, disconnect, forget credentials and start a setup hotspot.

If no saved network works, MAZ Pocket starts:

- SSID: `MAZ-Pocket-Setup`
- password: `mazpocket`
- setup page: `http://192.168.4.1`

On a normal LAN, use `http://mazpocket.local`.

## `mazpocket.local`

The device-hosted control plane exposes device/network/Core status, primary+backup Wi-Fi, six-surface launchers, allow-listed PC controls, diagnostics, Core config, reboot, M5Launcher hand-back, a ~2 FPS 240x135 LCD mirror and verified phone-to-SD firmware staging.

Cardputer ADV has no built-in camera. Camera video therefore remains an external-hardware extension rather than a claimed firmware feature.

## MAZ Core

The Windows PC is the persistent brain. v0.6 installs Core into a stable per-user location rather than relying on a Downloads extraction folder.

MAZ Core provides:

- local Ollama routing with **primary → backup local model** failover;
- LOCAL mode that never silently spills into cloud;
- AUTO mode that can use configured cloud only after both local models fail;
- real project discovery and branch/dirty/recent-commit evidence;
- bounded project/Obsidian search and non-secret file reads;
- allow-listed git status/fetch/fast-forward pull, detected test/build jobs and open-folder action;
- background project jobs so builds/tests do not freeze the handheld;
- Cardputer status and live-LCD proxy;
- Agent Nudge integration;
- optional private GitHub `[MAZ CORE]` issue bridge for AI clients with GitHub access, still using the same fixed allow-list and **no arbitrary remote shell**.

## Private Maz Works AI bridge

Maz Works contains an **unlinked, `noindex`** `/maz-pocket-ai` client alongside `/maz-core`. No private Core URL or token is committed to the public site. Your browser supplies them at runtime.

The AI bridge can create a private capability link/access pack for a trusted AI client that supports authenticated HTTPS. When the optional GitHub bridge is enabled, it can also provide private-queue instructions for an AI with GitHub access.

A capability link is a secret: rotate the Core token if it is exposed.

## Safety / rollback boundary

The firmware `.bin` is an **app-only M5Launcher image**. Do not flash it at address `0x0`. Generic ArduinoOTA remains disabled because M5Launcher owns firmware installation and rollback.

The target is StampS3A / ESP32-S3FN8 with 8 MB flash and no PSRAM. CI enforces the known Launcher app ceiling, but the final field gate remains physical hardware testing: soak, repeated COMM, Wi-Fi/Core loss, SD faults, phone staging, USB Core-only pairing, audio cycles and M5Launcher rollback while observing runtime health measurements.

See `QUICKSTART.txt`, `RELEASE_NOTES.md`, `docs/V060-PLAN.md` and `docs/V05-HARDENING.md`.
