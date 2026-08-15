# MAZ Pocket

MAZ Pocket is standalone firmware for the **M5Stack Cardputer ADV**, designed as a small physical front-end for voice capture, PC control, reminders and AI-agent workflows.

## v0.4

The device now presents six primary surfaces directly on Home:

- **COMM** — voice conversation with MAZ Host, local/auto/cloud routing, spoken replies and PC controls.
- **CAPTURE** — fast field capture and voice notes.
- **OPS** — Agent Nudge status, inspection and nudges.
- **DESK** — PC/device controls, Wi-Fi, diagnostics and settings.
- **RECALL** — inbox, notes, snippets and text viewer.
- **FLOW** — reminders, focus, sprint and tasks.

Long-tail utilities remain available through **Ctrl+K**. **Ctrl+L** performs the guarded hand-back to M5Launcher.

## Installation

The supported user path is intentionally simple:

1. Download `Maz-Pocket-v0.4-M5Launcher.bin` from the GitHub Release.
2. Copy it to the Cardputer microSD card.
3. Boot M5Launcher.
4. Select the `.bin`, install it into an app slot and launch it.

The release binary is an **app-only image**, not a complete ESP32 flash image. Do not write it to address `0x0` as a full-device firmware.

## Wi-Fi

MAZ Pocket stores Wi-Fi credentials in NVS. On boot it automatically attempts the saved network and the networking layer supports a second saved network. If the connection drops, it retries with bounded exponential backoff rather than continuously hammering the radio.

First-time setup is available on-device under **DESK → Connections**: scan, choose a network and enter its password. Once connected, `http://mazpocket.local` exposes the small local status/control surface.

## Storage

Cardputer ADV has **8 MB internal flash**. A large microSD card does not increase executable flash space. Use SD as the firmware library and data store; install only the apps you actively need and swap them through M5Launcher when internal flash is tight.

## Updating

Generic ArduinoOTA is intentionally disabled. With M5Launcher, several unrelated applications may share the internal flash and a generic "next OTA slot" is not a safe ownership boundary.

For v0.4 the supported update method is therefore the same reliable path as installation: download the new M5Launcher `.bin`, keep it on SD, and install/update it from M5Launcher.

The experimental browser flasher code remains in the repository for recovery research/tests but is **not the primary v0.4 user update path**.

## MAZ Host

Heavy work remains on the laptop/cloud side. MAZ Host provides speech-to-text, local/cloud model routing, TTS, deterministic PC controls and Agent Nudge integration. The Cardputer stays a fast interface rather than trying to run a large model locally.

## Version naming

Canonical product versions use the short scheme: **v0.1, v0.2, v0.3, v0.4**. Older GitHub preview tags remain only as historical/recovery references.

## Build gates

CI runs the MAZ Host tests, browser-recovery safety tests, Cardputer ADV firmware build, ESP32 app-image validation and the known M5Launcher slot-size ceiling before producing the release binary.

See `QUICKSTART.txt` and `RELEASE_NOTES.md` for the current release path.
