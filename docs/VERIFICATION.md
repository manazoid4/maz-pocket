# MAZ Pocket verification

## Version naming

Canonical releases use **v0.1 / v0.2 / v0.3 / v0.4**. Historical preview tags are retained only as recovery/history references.

## Physically established on Cardputer ADV

Previous builds have been installed through M5Launcher on the real Cardputer ADV. The hardware path has established Launcher-managed app installation, display, ADV keyboard initialization, storage initialization and Wi-Fi/host operation.

The user also physically installed the v0.3-era build during this development cycle. That real-device test exposed two product problems that v0.4 directly addresses: the main UI hid too much functionality, and the browser ZIP flasher was not a reliable primary update experience. The v0.4 release therefore uses the six-surface Home and M5Launcher + SD-card `.bin` as the supported update path.

## v0.4 release gates

A v0.4 binary is acceptable only when CI passes:

- MAZ Host pytest suite, including the v0.4 local-first model default.
- Recovery/browser-flasher syntax and partition-safety tests (kept as regression coverage, not the primary install UX).
- Cardputer ADV PlatformIO build.
- ESP32 application magic check.
- Minimum sensible app-image size.
- Hard `0x180000` maximum image size matching the known Launcher app slot used by the physical installation path.
- SHA-256 generation for the exact M5Launcher release binary.

## v0.4 behavior that must be checked on hardware after install

- Home visibly shows COMM / CAPTURE / OPS / DESK / RECALL / FLOW and each opens the intended surface.
- Saved Wi-Fi reconnects at boot and after a forced reconnect.
- `mazpocket.local` resolves on the LAN (or the shown IP works) and the new responsive dashboard loads on phone/desktop.
- Web status correctly reflects Wi-Fi, battery, storage, current app, host and agents.
- Pairing-token lock prevents state-changing web actions until unlocked.
- Web launch buttons open only allow-listed MAZ surfaces.
- PC quick controls remain allow-listed and execute through MAZ Host.
- Speaker/SD diagnostics, reboot and M5Launcher hand-back work from the dashboard.
- COMM uses MAZ Host with `lfm2.5-8b-a1b-gpu:latest` when the host configuration is upgraded; responses about MAZ Pocket stay within the verified capability map rather than inventing apps.
- Microphone capture, spoken reply and BrainDump remain functional.

CI cannot prove acoustics, RF quality, physical keyboard feel or the actual local Ollama model installed on a user's PC. These remain physical acceptance checks after installing the released `.bin`.
