# MAZ Pocket v0.3

MAZ Pocket turns the M5Stack Cardputer ADV into a pocket field terminal for the
computer and agents you already use. v0.3 deliberately gives Home only three
jobs:

- **COMM** — a push-to-talk line to MAZ Host. Hold SPACE, speak, keep the
  conversation across visits, read or hear the reply. Press C for a tiny
  command deck that can show the desktop, control media/volume or lock Windows.
- **LOG** — a field/captain's log. Recording starts immediately, raw audio is
  preserved first, H marks important moments, and MAZ Host structures the
  thought when the PC is available.
- **OPS** — Agent Nudge as a pocket operations console: factual fleet state,
  evidence, waiting/stale/working attention and explicit nudge actions.

Home is a three-icon retro launcher, not an app table. LEFT/RIGHT selects a tile
and ENTER opens it. Notes, Tasks, Focus, Sprint, Reminders, Inbox, Recorder and
other proven utilities remain installed behind `Ctrl+K`/shortcuts; they simply
no longer compete for the first screen.

The visual language is dark instrument-panel amber/cyan, but the retro-future
idea is behavioural rather than decorative: a communicator to your computer, a
portable field log, a machine command deck, and an ambient mission-status line.
See `docs/V03-DESIGN.md` for the explicit anti-bloat product contract.

## COMM / Call PC

MAZ Pocket tries the local MAZ Host address first for minimum latency, then an
optional verified HTTPS remote address when away from home. The Windows updater
can provision a Tailscale Funnel for that fallback; LAN remains preferred.

The Cardputer records and uploads audio. The PC performs STT, local/cloud model
routing, Agent Nudge access, safe machine actions and optional TTS. API/model
keys never live on the ESP32. Spoken replies use Windows speech through MAZ Host
and return as WAV audio for Cardputer playback.

### Command deck

Press **C** inside COMM and LEFT/RIGHT through six deliberately bounded actions:

`DESKTOP` · `PLAY` · `MUTE` · `VOL -` · `VOL +` · `LOCK`

ENTER transmits. These actions are allow-listed in MAZ Host rather than exposing
a remote shell. Direct phrases such as `mute my pc`, `show desktop`, `next
track`, or `lock my computer` are also parsed deterministically and execute with
zero LLM round trip. Questions that merely mention those words do not execute.

## Lowest-friction Windows updates

CI builds one **`MazPocketUpdater-v0.3.exe`** containing the exact firmware from
the same build. It provides four actions:

- **USB UPDATE** — preserves M5Launcher, prepares the MAZ app/storage slot,
  installs v0.3 and checks the real `READY` boot banner.
- **WI-FI UPDATE** — sends the bundled firmware directly through authenticated
  ArduinoOTA after pairing; no PlatformIO installation is required by the EXE.
- **PAIR** — USB-provisions Wi-Fi + LAN MAZ Host. It reuses the actual
  `host/.env` token (auto-detected or selected once) instead of inventing a
  secret that could disagree with the running host.
- **REMOTE CALL** — configures the HTTPS Tailscale Funnel fallback on the PC and
  saves it on the Cardputer.

For development, the existing PowerShell paths remain:

```powershell
.\scripts\install.ps1
cd host
.\setup.ps1
.\run.ps1
```

## Why no on-device chat model

`slvDev/esp32-ai` is a useful tiny-model reference, but its showcased 28.9M
model targets an ESP32-S3 setup with PSRAM and more flash than Cardputer ADV.
The ADV has 8 MB flash and no PSRAM. v0.3 keeps the good edge-first principle:
cheap deterministic/device work stays local; STT, generative reasoning, TTS and
agent integrations run on the PC. Future local ML must earn its memory with a
specific routing/classification task rather than existing for an "AI" label.

## Architecture

```text
Cardputer ADV
  COMM ---- voice + bounded PC commands
  LOG  ---- raw-first thought capture
  OPS  ---- agent assurance/actions
      |
      | LAN first / verified HTTPS fallback
      v
MAZ Host (Windows)
  STT -> deterministic commands -> local/cloud model when needed
  TTS -> WAV reply
  PC Controller -> small Win32 allow-list
  Agent Nudge -> factual agent state/actions
```

Audio is written to storage and streamed rather than buffering full recordings
in RAM. SD is preferred when present; internal storage remains the fallback.
M5Launcher hand-back and authenticated Wi-Fi OTA remain first-class paths.

## Anti-bloat gates

The v0.3 application gets a **2,100,000-byte firmware ceiling** in CI even though
the OTA slot is larger. Spare flash is headroom, not permission to add games,
icon packs, animations, camera code, wake-word listening, duplicate utilities or
a general-purpose remote shell.

## Verification

GitHub Actions runs MAZ Host tests, syntax-checks the updater, builds the
Cardputer ADV target, enforces the firmware budget and packages the Windows EXE.
Physical-device proof remains separate in `docs/VERIFICATION.md`: a successful
compile is not claimed as proof that microphone, speaker, remote access, USB
installation or Wi-Fi OTA worked on a real Cardputer.

MIT licensed. Third-party references and licence decisions are recorded under
`docs/`.
