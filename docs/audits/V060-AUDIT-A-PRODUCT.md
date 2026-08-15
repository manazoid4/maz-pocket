# MAZ Pocket v0.6 — Audit A: product and friction

Scope: current `main` after v0.5.2 web-control hardening, the six primary surfaces, Windows MAZ Core setup, M5Launcher install/update, `mazpocket.local`, and the Maz Works bridge.

## Highest-friction findings

1. **Version identity has drifted.** v0.5.2 behavior is merged, but firmware build flags, README, Quickstart, Core messages and CI artifact names still say v0.5.1. This makes it unnecessarily hard to know what is installed or which file to use.
2. **The best update path is hidden.** `portal_v2.cpp` already supports authenticated phone-to-SD firmware staging, but the primary README/Quickstart still teach the older PC → SD-reader flow first.
3. **MAZ Core is installed wherever the ZIP happened to be extracted.** The startup shortcut points back to that folder, so moving/deleting Downloads can silently break Core startup.
4. **Too many setup entry points.** Firmware copy, Core install, USB pairing and browser control each exist, but there is no single “start here” path that composes them.
5. **Browser pairing is session-only.** `mazpocket.local` forgets the token when the browser session ends, adding repetitive unlock friction on a trusted personal phone/PC.
6. **Pairing is still manual in common cases.** The current USB pairing script asks for the Wi-Fi password even when the Cardputer is already on Wi-Fi and only Core address/token need updating.
7. **Host/Core naming is inconsistent.** User-facing text alternates between MAZ Host, MAZ Core and PC for the same companion boundary.
8. **Old release workflows and source variants remain visible.** They are useful rollback history but make the active path harder to understand.

## What v0.6 should optimize for

- One obvious first-run entry point on Windows.
- After v0.6 is installed, normal firmware updates should be possible from a phone on the same LAN without removing the microSD.
- Pair once on a trusted browser; provide an explicit “forget this browser” action.
- Keep M5Launcher as the firmware installer/rollback owner. Do not add a direct partition writer just to remove one tap.
- Keep six primary surfaces. No new app count for its own sake.
- Make the local AI path more resilient without requiring another cloud dependency.

## Recommended v0.6 product changes

### Ship
- Single source of truth for version `0.6` and CI checks that packaging/docs/firmware agree.
- `START-HERE.cmd` / unified Windows setup that installs Core into a stable per-user location, starts it, optionally pairs a USB-connected Cardputer, optionally copies the firmware to one detected microSD, then opens `mazpocket.local`.
- Promote phone staging to the normal **update** path for devices already running v0.5.2+.
- Persist portal token in browser local storage and add a visible forget action.
- Add a Core-only USB pairing command that does not ask for Wi-Fi credentials when the device already has network access.
- Local-model backup chain: primary → second installed Ollama model; LOCAL never spills to cloud, AUTO may use cloud only after both local models fail.
- Keep all PC actions allow-listed; no arbitrary shell.

### Do not ship in the first v0.6 cut
- Full realtime microphone-frame streaming from stale PR #11. It is high-value but touches audio, networking, Core, queues and fallback behavior at once and needs physical ADV validation.
- More games, novelty apps, or another top-level surface.
- Self-flashing OTA that bypasses M5Launcher.

## Definition of “minimal friction”

**Fresh Windows setup:** extract one release ZIP → double-click `START-HERE.cmd` → Core is placed somewhere durable and started; if USB/SD are present they are configured automatically or with one clear confirmation.

**Normal update after v0.6:** download the next app `.bin` on phone → open `mazpocket.local` → choose file → verify/stage → tap M5Launcher → Install/Launch.

**Daily use:** browser remains paired on the user’s trusted device until they explicitly forget it; Core survives Downloads cleanup and starts at sign-in.