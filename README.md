# MAZ Pocket

Standalone firmware for the **M5Stack Cardputer ADV**. A pocket capture device
that installs and launches through M5Launcher alongside Bruce, Nemo and the
rest — and the hardware front end for the MAZ assistant stack.

**v0.1 is local only.** Nothing talks to a network service yet. The voice,
audio, storage and networking foundations are built so that v0.2 can replace
`record -> local playback` with `record -> Wi-Fi -> OpenFlowKit -> MAZos -> TTS`
without rewriting the Call screen.

```
┌──────────────────────┐
│ ◉ 14:32      WiFi 87%│
│ ┌──────────────────┐ │
│ │ NEXT             │ │
│ │ Finish FlowLens  │ │
│ └──────────────────┘ │
│ ┌─────┐┌─────┐┌─────┐│
│ │Call ││Capt ││Focus││
│ └─────┘└─────┘└─────┘│
│ ┌─────┐┌─────┐┌─────┐│
│ │Notes││Tasks││Recor││
│ └─────┘└─────┘└─────┘│
│ hold SPACE   ^K cmd  │
└──────────────────────┘
```

## What it does

| App | What it is for |
|---|---|
| **Call** | Hold SPACE, talk, release. Plays the take back; keep or bin it. The screen v0.2 wires to the assistant. |
| **Capture** | Fastest thought to stored. Hold SPACE for voice, or just start typing. |
| **Notes** | Local notes: write, read, edit, delete. |
| **Focus** | 10/25/45/60 min sessions with a label. Keeps running when you leave the screen. |
| **Tasks** | Today / Later. Home shows the next open one. |
| **Recorder** | Long-form recording with playback and file management. |
| **Command palette** | `Ctrl+K` anywhere. Type `rec`, `note`, `focus`. |
| **Utilities** | Calculator, Stopwatch, QR, Text Viewer, Generator, Snippets — see [the research](docs/research/COMMUNITY_FEATURES.md) for why these six and not others. |
| **Tools** | Mic/speaker/keyboard tests, Wi-Fi scan, battery, memory, storage, device info, reboot. Diagnostics only. |
| **Connections** | Wi-Fi, and the MAZ Host placeholder. Shows `NOT CONFIGURED` rather than pretending. |

## Keys

| Key | Does |
|---|---|
| hold **SPACE** | Voice capture, from anywhere on Home |
| **Ctrl+K** | Command palette |
| **ESC** / hold **ESC** | Back / straight Home |
| **C X F / N T R** | Call, Capture, Focus / Notes, Tasks, Recorder |
| **D** twice | Delete (no modal, no accidents) |

## Build

```bash
pio run                      # build
pio run -t upload            # flash over USB-C
pio device monitor           # serial log at 115200
```

Output: `.pio/build/cardputer-adv/firmware.bin`

## Install through M5Launcher

1. Copy `firmware.bin` to the microSD card (FAT32).
2. On the Cardputer, hold **ENTER** at boot to enter M5Launcher.
3. Choose **SD**, pick `firmware.bin`, install.
4. Bruce, Nemo and anything else stay exactly where they were.

MAZ Pocket is an app-only binary and does not claim ownership of the launcher.
Tools then Reboot returns you to it.

## Hardware notes that matter

The ADV is **not** a Cardputer with a bigger battery. The keyboard moved behind
a **TCA8418 I2C expander** — which is why firmware built for the original
Cardputer boots on an ADV with a dead keyboard — and audio moved to an **ES8311
codec**. MAZ Pocket ships its own TCA8418 driver and pins M5Unified 0.2.19, the
first release with ADV support. Full detail in
[docs/research/HARDWARE_ADV.md](docs/research/HARDWARE_ADV.md).

There is **no PSRAM**. Audio is streamed to storage, never buffered whole.

## Storage

```
/maz/notes/       /maz/recordings/   /maz/captures/
/maz/tasks/       /maz/snippets/     /maz/logs/    /maz/cache/
```

SD when present, internal LittleFS otherwise, and it still boots and runs with
neither — settings live in NVS, which survives being flashed by any launcher.

## Docs

- [Community research](docs/research/COMMUNITY_FEATURES.md) — what shipped and what was rejected
- [Hardware notes](docs/research/HARDWARE_ADV.md) — ADV pin map and traps
- [Licences](docs/research/LICENCES.md) — what was read, what was used
- [Verification](docs/VERIFICATION.md) — what is tested and what still needs the device

## Licence

MIT. No Bruce or Nemo code or branding is used; all artwork is drawn in code.
