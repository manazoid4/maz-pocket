# MAZ Pocket v0.3

MAZ Pocket turns the M5Stack Cardputer ADV into a small physical remote for the
intelligence and agents running on your PC. v0.3 deliberately reduces the Home
screen to three jobs instead of presenting every utility at once:

- **CALL** — hold SPACE, speak to MAZ Host, keep the conversation alive across
  visits, read the reply and optionally hear it through the Cardputer speaker.
- **CAPTURE** — record a thought immediately, preserve the raw audio first, then
  structure it through MAZ Host when the PC is available.
- **AGENTS** — see Agent Nudge state, inspect evidence, and explicitly nudge an
  agent from the handheld.

Home is a three-icon retro launcher. LEFT/RIGHT selects a tile and ENTER opens
it. Existing Notes, Tasks, Focus, Sprint, Reminders, Inbox, Recorder and other
utilities are still installed; use `Ctrl+K` or their keyboard shortcuts when
you need them. They no longer compete for the first screen.

## Call PC

MAZ Pocket tries the local MAZ Host address first for minimum latency. An
optional HTTPS remote address can be configured as a fallback, so the same
Call screen can work away from home when the PC is exposed through a secure
remote tunnel such as Tailscale Funnel.

The Cardputer records and uploads audio; the PC performs STT, local/cloud model
routing, Agent Nudge access and TTS. API keys never live on the ESP32. Spoken
replies use Windows speech through MAZ Host by default and are returned as WAV
audio for playback on the Cardputer.

## Lowest-friction Windows updates

CI builds a single **`MazPocketUpdater-v0.3.exe`** containing the exact firmware
image from the same build. The updater provides four actions:

- **USB UPDATE** — preserves M5Launcher, prepares the MAZ app/storage slot,
  installs v0.3 and verifies the real `READY` boot banner.
- **WI-FI UPDATE** — sends the bundled image directly through authenticated
  ArduinoOTA once the device has been paired.
- **PAIR** — one USB setup for Wi-Fi, MAZ Host address and the shared token.
- **REMOTE CALL** — configures a Tailscale Funnel on the PC and stores the HTTPS
  fallback on the Cardputer; LAN stays preferred.

The existing PowerShell paths remain available for development:

```powershell
.\scripts\install.ps1
cd host
.\setup.ps1
.\run.ps1
```

## Why no on-device generative model

`slvDev/esp32-ai` is a useful reference for tiny-model deployment and
quantisation, but its showcased 28.9M model targets an ESP32-S3 configuration
with PSRAM and substantially more model storage. Cardputer ADV has 8 MB flash
and no PSRAM. v0.3 therefore uses the useful architectural lesson rather than
forcing that model onto unsuitable hardware: deterministic/local device work
stays on the ESP32, while STT, generative reasoning and agent tools run on the
PC. A future on-device classifier/router should be designed specifically for
the ADV memory budget rather than pretending it is a chat LLM.

## Architecture

```text
Cardputer ADV
  CALL / CAPTURE / AGENTS
        |
        | LAN first, HTTPS fallback
        v
MAZ Host (Windows)
  STT -> local/cloud models
  TTS -> WAV reply
  Agent Nudge -> agent state/actions
```

Audio is written to storage and streamed rather than buffering whole recordings
in RAM. SD is preferred when present; internal storage remains the fallback.
M5Launcher hand-back and authenticated Wi-Fi OTA remain first-class paths.

## Verification

GitHub Actions runs the MAZ Host tests, syntax-checks the updater, builds the
Cardputer ADV target and packages the Windows updater. Physical-device results
remain separate in `docs/VERIFICATION.md`: a successful compile is not claimed
as proof that mic, speaker, remote access or flashing worked on real hardware.

MIT licensed. Third-party references and licence decisions are recorded under
`docs/research/`.
