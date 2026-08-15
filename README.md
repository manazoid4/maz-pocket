# MAZ Pocket

MAZ Pocket is standalone firmware for the **M5Stack Cardputer ADV**, designed as a small physical front-end for voice capture, PC control, reminders and AI-agent workflows.

## v0.4

The device presents six primary surfaces directly on Home:

- **COMM** — voice/text conversation with MAZ Host, spoken replies and PC controls.
- **CAPTURE** — fast field capture and voice notes.
- **OPS** — Agent Nudge status, inspection and nudges.
- **DESK** — PC/device controls, Wi-Fi, diagnostics and settings.
- **RECALL** — inbox, notes, snippets and text viewer.
- **FLOW** — reminders, focus, sprint and tasks.

Long-tail utilities remain available through **Ctrl+K**. **Ctrl+L** performs the guarded hand-back to M5Launcher.

## Installation

1. Download `Maz-Pocket-v0.4-M5Launcher.bin` from the GitHub Release.
2. Copy it to the Cardputer microSD card.
3. Boot M5Launcher.
4. Select the `.bin`, install it into an app slot and launch it.

The release binary is an **app-only image**, not a complete ESP32 flash image. Do not write it to address `0x0` as a full-device firmware.

## Wi-Fi

MAZ Pocket stores Wi-Fi credentials in NVS. On boot it automatically attempts the saved network; the networking layer supports a second saved network. If the connection drops, it retries with bounded exponential backoff.

First-time setup is available on-device under **DESK → Connections**: scan, choose a network and enter its password.

## `mazpocket.local`

Once the Cardputer is connected, open `http://mazpocket.local` from a phone or computer on the same network. The v0.4 dashboard is designed as a proper remote control rather than a diagnostics page. It provides:

- live Wi-Fi, IP, signal, battery, storage, current-app, MAZ Host and Agent Nudge status;
- one-tap launch of COMM / CAPTURE / OPS / DESK / RECALL / FLOW on the Cardputer;
- allow-listed PC controls for desktop, media, volume, mute and lock;
- MAZ Host address, local/auto/cloud route and spoken-reply configuration;
- backup Wi-Fi configuration;
- Wi-Fi reconnect, host test, speaker/SD tests, reboot and M5Launcher hand-back.

Read-only status is available immediately. State-changing controls require the existing MAZ pairing token.

## Local AI

Heavy AI runs on MAZ Host, not the ESP32. The v0.4 preferred Ollama model is:

`lfm2.5-8b-a1b-gpu:latest`

MAZ Host defaults to local routing, low-temperature generation and an explicit verified MAZ Pocket capability map. This is intended to prevent the old behavior where a tiny generic model invented apps or answered vaguely about what the device could do.

Existing `host/.env` files override defaults. Upgrade them to:

```text
MAZ_OLLAMA_MODEL=lfm2.5-8b-a1b-gpu:latest
MAZ_DEFAULT_ROUTE=local
```

Agent state is only supplied to the model as live Agent Nudge evidence; roadmap ideas are explicitly separated from current features.

## Storage

Cardputer ADV has **8 MB internal flash**. A large microSD card does not increase executable flash space. Use SD as the firmware library and data store; install only the apps you actively need and swap them through M5Launcher when internal flash is tight.

## Updating

Generic ArduinoOTA is intentionally disabled. With M5Launcher, several unrelated applications may share internal flash and a generic "next OTA slot" is not a safe ownership boundary.

For v0.4 the supported update method is the same reliable path as installation: download the new M5Launcher `.bin`, keep it on SD and install/update it from M5Launcher.

The experimental browser flasher code remains only for recovery research/tests and is **not the primary v0.4 user update path**.

## Version naming

Canonical product versions use the short scheme: **v0.1 / v0.2 / v0.3 / v0.4**. Older GitHub preview tags remain only as historical/recovery references.

## Build gates

CI runs the MAZ Host tests, recovery-flasher safety tests, Cardputer ADV firmware build, ESP32 app-image validation and the known M5Launcher slot-size ceiling before producing the release binary.

See `QUICKSTART.txt` and `RELEASE_NOTES.md` for the current release path.
