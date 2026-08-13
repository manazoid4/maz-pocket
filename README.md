# MAZ Pocket

MAZ Pocket turns the M5Stack Cardputer ADV into a dedicated physical interface
to intelligence running on a laptop. The device stays focused on fast capture,
visible state, shortcuts, timers, reminders and agent assurance; MAZ Host does
STT and local/cloud model work.

## The eight fast surfaces

| Key | Surface | Purpose |
|---|---|---|
| `T` | MAZ Talk | Hold SPACE, speak, receive a visible laptop-generated answer. |
| `B` | BrainDump | Records immediately; `H` highlights, `P` pauses, raw audio always survives. |
| `I` | Inbox | Useful answers and processed outputs, not another notes app. |
| `D` | Decision | Captures both what and why. |
| `F` | Focus | A deliberately small timer. |
| `S` | Sprint | Intended outcome, 25-minute timer, then voice debrief entry. |
| `N` | Nudge | Evidence-backed agent state and explicit nudging. |
| `R` | Reminders | Local reminders with done and snooze actions. |

`Ctrl+K` opens every secondary utility. Captures and queued turns use SD when
available and internal LittleFS otherwise. Audio is streamed; the ADV has no
PSRAM and never buffers a whole WAV in memory.

## Run the laptop compute layer

```powershell
cd host
.\setup.ps1
.\run.ps1
```

Then open **Connections**, join Wi-Fi, press `C`, and enter the address/token
printed by setup. Agent Nudge remains loopback-only; MAZ Host is the authenticated
LAN proxy used by the Cardputer.

## Build and install

```powershell
py -m platformio run
.\scripts\package-release.ps1
```

- **Recommended:** keep M5Launcher and install `dist/maz-pocket-app.bin` through Launcher's WUI or FAT32 microSD manager.
- Full replacement: use the browser installer in `flash/` only when MAZ Pocket should own the whole device.

When launched from M5Launcher, MAZ Pocket never formats a shared internal partition. Use a FAT32 SDHC card for durable audio unless compatible LittleFS storage already exists.

The full web image replaces the current firmware and launcher. If automatic USB
connection fails, unplug the ADV, hold `G0`, reconnect, and retry.

## Verification truth

Host tests and the Cardputer ADV target build run in CI. Physical-device results
are recorded separately in [docs/VERIFICATION.md](docs/VERIFICATION.md); a compile
is never presented as a hardware demonstration.

MIT licensed. The MAZ UI and assets are original; third-party references and
licence decisions are listed under `docs/research/`.
