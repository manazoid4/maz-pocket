# MAZ Pocket

MAZ Pocket is standalone firmware for the **M5Stack Cardputer ADV** plus **MAZ Core**, a local Windows companion that supplies real project/PC context and safe actions.

## v0.5 milestone

Home stays intentionally small:

- **COMM** — voice/text conversation, spoken replies, PC control.
- **CAPTURE** — field capture, recordings and BrainDump.
- **OPS** — Agent Nudge evidence, status and nudges.
- **CONTROL** — Wi-Fi, MAZ Core, PC/COMM, diagnostics, settings and M5Launcher.
- **RECALL** — inbox, notes, snippets and viewer.
- **FLOW** — reminders, focus, sprint and tasks.

**W from Home opens Wi-Fi directly. Ctrl+K opens everything. Ctrl+L returns to M5Launcher.**

## Wi-Fi that is always recoverable

v0.5 makes networking first-class on the device. CONTROL → WI-FI can show status, reconnect, scan/connect primary Wi-Fi, save a backup network, disconnect, forget credentials and start a setup hotspot.

If no saved network works, MAZ Pocket starts:

- SSID: `MAZ-Pocket-Setup`
- password: `mazpocket`
- setup page: `http://192.168.4.1`

On a normal LAN, use `http://mazpocket.local`.

## `mazpocket.local`

The device-hosted v0.5 control plane exposes the same capabilities instead of inventing a separate UI: device/network/Core status, primary+backup Wi-Fi, six-surface launchers, PC controls, diagnostics, Core config, reboot, M5Launcher hand-back and a **~2 FPS 240x135 LCD mirror**.

Cardputer ADV has no built-in camera. v0.5 therefore streams the real LCD; camera video is an external-hardware extension rather than a claimed software feature.

## MAZ Core

The Windows PC is the persistent brain. `MAZ-Core-v0.5.zip` ships with the release; extract it and run `install-core.ps1` once.

MAZ Core provides:

- `lfm2.5-8b-a1b-gpu:latest` through Ollama by default;
- local project discovery and real branch/dirty/recent-commit evidence;
- bounded project/Obsidian search and non-secret file reads;
- allow-listed git status/fetch/fast-forward pull, detected test/build jobs and open-folder action;
- background jobs so builds/tests never freeze the Cardputer;
- Cardputer status and live-LCD proxy;
- Agent Nudge integration;
- optional private GitHub issue bridge: ChatGPT → private GitHub → MAZ Core → local PC, still using the same fixed allow-list and **no arbitrary remote shell**.

This fixes the old generic-AI architecture: the LLM interprets tool evidence; it is not itself the source of truth about your projects, PC, agents or device.

## Hidden Maz Works console

Maz Works contains an **unlinked, `noindex`** `/maz-core` client. No private Core URL or token is committed to the public site. Your own browser supplies them at runtime; the Core URL is localStorage-only and the token is sessionStorage-only.

For remote use from the HTTPS site, `install-core.ps1` can use an already-installed Tailscale client to expose MAZ Core through private Tailscale Serve. The private endpoint is never written into the site repository.

## Install / update

1. Copy `Maz-Pocket-v0.5-M5Launcher.bin` to microSD.
2. Boot M5Launcher.
3. Select the `.bin`, Install, Launch.
4. On PC extract `MAZ-Core-v0.5.zip` and run `install-core.ps1` once.

The firmware `.bin` is an **app-only M5Launcher image**. Do not flash it at address `0x0`.

Generic ArduinoOTA remains disabled because M5Launcher can hold several unrelated applications in internal flash; MAZ must not guess which partition it owns. SD remains the firmware/data library and does not expand the Cardputer ADV's 8 MB executable flash.

## Version naming

Canonical names are **v0.1 / v0.2 / v0.3 / v0.4 / v0.5**. Older preview labels remain only as history/recovery references.

See `QUICKSTART.txt` and `RELEASE_NOTES.md` for the one-shot install path.
