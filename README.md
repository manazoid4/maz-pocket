# MAZ Pocket

MAZ Pocket is standalone firmware for the **M5Stack Cardputer ADV** plus **MAZ Core**, a local Windows companion that supplies real project/PC context and safe actions.

## Current release: v0.5.1

Home stays intentionally small:

- **COMM** — voice conversation, spoken replies and safe PC controls.
- **CAPTURE** — field capture, recordings and BrainDump.
- **OPS** — Agent Nudge evidence, status and nudges.
- **CONTROL** — Wi-Fi, MAZ Core, PC/COMM, diagnostics, settings and M5Launcher.
- **RECALL** — inbox, notes, snippets and viewer.
- **FLOW** — reminders, focus, sprint and tasks.

**W from Home opens Wi-Fi directly. Ctrl+K opens everything. Ctrl+L returns to M5Launcher.**

v0.5.1 is deliberately a hardening release rather than an app-count release. COMM's expensive Host work is serialised on one bounded background worker, device/USB status paths avoid foreground network waits, and runtime heap/stack/loop measurements make Cardputer ADV tuning evidence-based.

## Wi-Fi that is always recoverable

CONTROL → WI-FI can show status, reconnect, scan/connect primary Wi-Fi, save a backup network, disconnect, forget credentials and start a setup hotspot.

If no saved network works, MAZ Pocket starts:

- SSID: `MAZ-Pocket-Setup`
- password: `mazpocket`
- setup page: `http://192.168.4.1`

On a normal LAN, use `http://mazpocket.local`.

## `mazpocket.local`

The device-hosted control plane exposes device/network/Core status, primary+backup Wi-Fi, six-surface launchers, PC controls, diagnostics, Core config, reboot, M5Launcher hand-back and a **~2 FPS 240x135 LCD mirror**.

Cardputer ADV has no built-in camera. Camera video therefore remains an external-hardware extension rather than a claimed firmware feature.

## MAZ Core

The Windows PC is the persistent brain. `MAZ-Core-v0.5.1.zip` ships with the release; extract it and run `install-core.ps1` once.

MAZ Core provides:

- local Ollama model routing plus optional cloud routing;
- real project discovery and branch/dirty/recent-commit evidence;
- bounded project/Obsidian search and non-secret file reads;
- allow-listed git status/fetch/fast-forward pull, detected test/build jobs and open-folder action;
- background project jobs so builds/tests do not freeze the handheld;
- Cardputer status and live-LCD proxy;
- Agent Nudge integration;
- optional private GitHub `[MAZ CORE]` issue bridge for AI clients with GitHub access, still using the same fixed allow-list and **no arbitrary remote shell**.

## Private Maz Works AI bridge

Maz Works contains an **unlinked, `noindex`** `/maz-pocket-ai` client alongside `/maz-core`. No private Core URL or token is committed to the public site. Your browser supplies them at runtime.

The AI bridge can create a private capability link/access pack for a trusted AI client that supports authenticated HTTPS. When the optional GitHub bridge is enabled, it can also provide the private-queue instructions for an AI with GitHub access.

A capability link is a secret: rotate the Core token if it is exposed.

## Install / update

Fast path:

1. Download `MAZ-Pocket-v0.5.1-Install.zip` and extract it.
2. Insert the Cardputer microSD card into Windows.
3. Double-click `INSTALL-MAZ-POCKET.cmd`.
4. Safely eject the card, boot M5Launcher, select `Maz-Pocket-v0.5.1-M5Launcher.bin`, Install, Launch.
5. Install/update MAZ Core from `MAZ-Core-v0.5.1.zip` when you are back at the PC.

The helper only copies the app image to removable media, validates the ESP32 image and verifies SHA-256. It does not format the card or flash partitions.

The firmware `.bin` is an **app-only M5Launcher image**. Do not flash it at address `0x0`. Generic ArduinoOTA remains disabled because M5Launcher owns firmware installation and rollback.

## Cardputer ADV stability gate

The target is StampS3A / ESP32-S3FN8 with 8 MB flash and no PSRAM. CI enforces the known Launcher app ceiling, but the final field gate remains physical hardware testing: soak, repeated COMM, Wi-Fi/Core loss, SD faults, audio cycles and M5Launcher rollback while observing `[health]` and `[host-worker]` measurements.

See `QUICKSTART.txt`, `RELEASE_NOTES.md` and `docs/V05-HARDENING.md`.
