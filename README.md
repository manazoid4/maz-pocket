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
Internet control panel. v0.03 deliberately keeps the on-device web code small:

- firmware, battery, active storage and free-space state;
- SD present/unreadable state;
- Wi-Fi/IP/RSSI, MAZ Host link and agent summary;
- authenticated MAZ Host address/port configuration;
- LOCAL / AUTO / CLOUD route selection and spoken-reply toggle;
- speaker self-test;
- four-second microphone test with live level meter;
- MAZ Host reachability probe;
- a small non-destructive SD read/write verification when SD is active;
- reboot and guarded M5Launcher hand-back;
- a link to the dedicated online firmware flasher.

Sensitive controls reuse the existing MAZ pairing token. Basic device status is
read-only without it; the token is never rendered back by firmware and the
browser keeps what you type only in session storage.

Firmware writing does **not** live inside `mazpocket.local`. That separation is
intentional: a Launcher-managed device can contain many unrelated OTA app
partitions, and generic "next OTA slot" behaviour is not an ownership boundary.

If `.local` resolution is unavailable on a particular phone/network, open the
numeric IP shown by MAZ Pocket instead, for example `http://192.168.1.42`.

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

## Updates — browser first

The normal v0.02 → v0.03 path is the MAZ browser flasher:

```text
https://mazos-site.vercel.app/maz-pocket/flasher/
```

It is modelled on the proven Bruce/Tasmota/Meshtastic browser-install pattern
and uses Espressif `esptool-js` rather than a custom Windows flashing stack.
Chrome/Edge desktop uses Web Serial; Chrome on Android can fall back to Google's
WebUSB Serial polyfill.

The MAZ flasher adds Launcher-specific safety before writing:

1. read the live ESP32 partition table;
2. require exactly one OTA app partition whose label starts with `MAZ-Pocket`;
3. verify the new ESP32 app image fits that live partition;
4. read and download a full raw backup of that partition;
5. write only that partition — never full-flash erase;
6. read the new image back and require an exact SHA-256 match;
7. attempt rollback on verification failure, with a known-good accepted v0.02
   image available if an earlier broken updater already invalidated the old app
   header.

The flasher never writes the partition table, NVS/settings, M5Launcher, SD data,
Bruce, or another installed firmware.

Cardputer ADV recovery follows the familiar browser-flasher pattern: if normal
automatic reset cannot enter download mode, unplug the Cardputer, hold **G0**
(upper-right), reconnect USB while holding G0, release it, then try again.

For local development:

```powershell
.\scripts\install.ps1
cd host
.\setup.ps1
.\run.ps1
```

## Architecture

```text
                    MAZ POCKET
                        |
       +----------------+----------------+
       |                |                |
      COMM           CAPTURE            OPS
       |                |                |
       +---------- DESK / RECALL / FLOW-+
                        |
        +---------------+----------------+
        |                                |
mazpocket.local                    MAZ Web Flasher
status / control / diag            USB Web Serial
        |                         live-partition update
        v                                |
     MAZ Host                            v
STT / TTS / agents                MAZ-Pocket slot only
PC controls / tools
```

The ADV has no PSRAM, so heavy generative reasoning remains on the PC/cloud.
Local firmware is reserved for deterministic actions, cache/offline behaviour,
UI, audio, networking and narrowly useful edge logic.

## SD posture

`Mraanderson/CardputerSDtool` is an MIT-licensed Cardputer ADV reference for SD
information, filesystem checks, benchmarks, integrity checking and formatting.
MAZ adopts the useful diagnostic/recovery mindset but keeps v0.03 conservative:
status plus a tiny temporary-file read/write check only. Experimental formatting
or destructive card tests do not run from normal MAZ flows.

## Anti-bloat and security gates

The physically accepted v0.02 build was installed by M5Launcher into a
`0x180000` (1,572,864-byte) aligned app partition. **v0.03 CI must fit that exact
ceiling.** The updater is not allowed to solve firmware growth by repartitioning
the user's Cardputer or deleting sibling apps.

- no arbitrary remote shell;
- no silent agent/tool auto-approval;
- authenticated network mutation;
- raw captures survive before AI processing;
- cloud/model secrets stay PC-side where possible;
- no generic ArduinoOTA round-robin updater under M5Launcher;
- no full-flash erase in the MAZ browser updater;
- compile success is never presented as physical-hardware proof.

GitHub Actions runs host tests, syntax-checks the browser flasher, builds the
Cardputer ADV target, enforces the physical v0.02 slot ceiling, validates the
known-good v0.02 recovery artifact and packages the phone-first flasher bundle.
Physical acceptance remains tracked separately in `docs/VERIFICATION.md`.

MIT licensed. Third-party references and licence decisions are recorded under
`docs/` and `web-flasher/README.md`.
