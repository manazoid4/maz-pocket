# MAZ Pocket

MAZ Pocket turns the M5Stack Cardputer ADV into a dedicated physical interface
to intelligence running on a laptop. The device stays focused on fast capture,
visible state, shortcuts, timers, reminders and agent assurance; MAZ Host does
STT and local/cloud model work.

The glanceable Home surface is rendered with pinned LVGL 9.5.0 over the
existing M5Unified framebuffer. This adds reusable embedded widgets and styles
without replacing the proven ADV keyboard, storage, audio or Launcher paths.
The official sources and retained MIT notice are recorded in
[docs/research/LVGL.md](docs/research/LVGL.md).

## Try it today

Start MAZ Host with `host/run.ps1`, then use the eight Home tiles or their
single-letter shortcuts: **T** Talk, **B** BrainDump, **I** Inbox, **D**
Decision, **F** Focus, **S** Sprint, **N** Nudge and **R** Reminders. Hold
SPACE from Home to speak. `/` opens every additional utility.

For a repeatable physical acceptance run, connect the Cardputer over USB and
run `host/.venv/Scripts/python.exe scripts/accept-device.py`. It opens and
renders every first-class screen, drives the actual app key handlers, records
through the device mic, exercises the live host, and returns the device Home.
The control commands exist only on the local USB serial interface.

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
Start-Process powershell -ArgumentList '-File', '.\run.ps1' -WindowStyle Hidden
.\pair.ps1
```

`pair.ps1` finds the Cardputer and laptop address automatically and asks for the
Wi-Fi password in a private prompt. Agent Nudge remains loopback-only; MAZ Host
reads its owner-only local credential and acts as the authenticated LAN proxy.

## Build and install

```powershell
.\scripts\install.ps1
```

- This is the only supported install path. It builds MAZ Pocket, hands control
  back to M5Launcher when needed, prepares isolated MAZ storage, uses
  M5Launcher's pinned official serial flasher, and verifies the real boot banner.
- `Ctrl+L` or **Tools → Back to M5Launcher** returns to Launcher. Re-running the
  same command replaces the old MAZ slot instead of filling flash with copies.

MAZ Pocket never formats shared Launcher storage. The installer creates a named
2 MB LittleFS partition for settings, queues and metadata. Use a FAT32 SDHC card
for longer audio capture; offline text/timers/reminders still use internal storage.

## Verification truth

Host tests and the Cardputer ADV target build run in CI. Physical-device results
are recorded separately in [docs/VERIFICATION.md](docs/VERIFICATION.md); a compile
is never presented as a hardware demonstration.

MIT licensed. The MAZ UI and assets are original; third-party references and
licence decisions are listed under `docs/research/`.
