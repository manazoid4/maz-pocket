# MAZ Pocket

MAZ Pocket turns the M5Stack Cardputer ADV into a pocket field terminal for the
computer, agents and personal tools you already use.

The current firmware keeps Home deliberately focused on three proven surfaces:

- **COMM** — push-to-talk to MAZ Host, persistent conversation, spoken replies
  and a bounded Windows command deck.
- **LOG** — raw-first field/captain's log with highlights and host processing.
- **OPS** — Agent Nudge fleet state, evidence and explicit nudge actions.

The approved v0.4 direction expands the product to six primary interaction
surfaces — **COMM, CAPTURE, OPS, DESK, RECALL and FLOW** — without turning Home
into an app drawer. See [`docs/V04-ROADMAP.md`](docs/V04-ROADMAP.md). Existing
utilities remain available through `Ctrl+K` until they are absorbed into one of
those surfaces.

## mazpocket.local

Once the Cardputer is paired and connected to Wi-Fi, open:

```text
http://mazpocket.local
```

This page is served **by the Cardputer itself** using mDNS; it is not a public
Internet control panel. The first useful control-plane cut includes:

- live firmware, battery, storage, heap and IMU state;
- Wi-Fi/IP/RSSI, MAZ Host link and agent summary;
- authenticated Wi-Fi + MAZ Host configuration;
- LOCAL / AUTO / CLOUD route selection and spoken-reply toggle;
- speaker self-test;
- four-second microphone test with live level meter;
- MAZ Host reachability probe;
- authenticated browser firmware OTA from a `.bin` file;
- reboot and safe M5Launcher hand-back.

Write operations reuse the existing MAZ pairing token. The token is never
rendered back by firmware; the browser keeps what you type in session storage.
First pairing still happens over USB so an unconfigured device never exposes a
network setup secret.

If `.local` resolution is unavailable on a particular Windows/network setup,
open the Cardputer's numeric IP shown in **Connections** or by the Windows
updater, for example `http://192.168.1.42`.

## COMM / Call PC

MAZ Pocket tries the local MAZ Host address first for minimum latency, then an
optional verified HTTPS remote address when away from home. The Cardputer owns
mic/speaker/UI; the PC performs STT, model routing, Agent Nudge access, bounded
machine actions and optional TTS. API/model keys stay off the ESP32.

Press **C** inside COMM and LEFT/RIGHT through the bounded command deck:

`DESKTOP` · `PLAY` · `MUTE` · `VOL -` · `VOL +` · `LOCK`

Direct phrases such as `mute my pc`, `show desktop`, `next track` or `lock my
computer` are parsed deterministically and skip the LLM. There is no arbitrary
remote shell.

## Hidden extras and utilities

`Ctrl+K` exposes Notes, Tasks, Focus, Sprint, Reminders, Inbox, Recorder,
Calculator, Stopwatch, Beam/QR, Text Viewer, Generator, Snippets, Connections,
Tools and Settings. Snake and the BMI270-driven Hyperdrive demo remain hidden
extras rather than product priorities.

## Updates

The Windows package contains the exact Cardputer firmware produced by the same
CI run. It supports USB install through M5Launcher, authenticated Wi-Fi
ArduinoOTA, pairing and optional remote Call PC provisioning.

For development:

```powershell
.\scripts\install.ps1
cd host
.\setup.ps1
.\run.ps1
```

After this web-control-plane build is installed, normal LAN firmware iterations
can also use **mazpocket.local → Browser OTA** and select the newly built
`maz-pocket-app.bin`.

## Architecture

```text
Cardputer ADV
  COMM / CAPTURE / OPS
  future: DESK / RECALL / FLOW
       |
       +-- mazpocket.local (device admin + browser OTA)
       |
       +-- LAN first / verified HTTPS fallback
       v
MAZ Host (Windows)
  STT / TTS / model routing
  bounded PC controls
  Agent Nudge
  future generic tool protocol
```

The ADV has no PSRAM, so heavy generative reasoning remains on the PC/cloud.
Local firmware is reserved for deterministic actions, cache/offline behaviour,
UI, audio, networking and narrowly useful edge logic.

## Anti-bloat and security gates

The application has a **2,100,000-byte firmware ceiling** in CI even though its
OTA slot is larger. Spare flash is headroom, not a feature quota.

- no arbitrary remote shell;
- no silent agent/tool auto-approval;
- authenticated network mutation and OTA;
- raw captures survive before AI processing;
- cloud/model secrets stay PC-side where possible;
- compile success is never presented as physical-hardware proof.

GitHub Actions runs host tests, syntax-checks the updater, builds the Cardputer
ADV target, enforces the firmware budget and packages the Windows updater plus
firmware. Physical acceptance remains tracked separately in
`docs/VERIFICATION.md`.

MIT licensed. Third-party references and licence decisions are recorded under
`docs/`.
