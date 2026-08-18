# MAZ Pocket

**A pocket-sized physical control surface for your PC, AI models and coding agents — built for the M5Stack Cardputer ADV.**

## Current release candidate: v0.8.0 — CONTROL

MAZ Pocket is deliberately not an ESP32 app drawer. The Cardputer handles voice, short text, status, approvals and quick commands while **MAZ Core** on the PC handles models, projects, screen recording, coding agents and heavier work.

## The six things on Home

| Home tile | What it does |
|---|---|
| **CALL** | Hold Space, talk to MAZ, get an AI answer and hear it spoken back. |
| **CAPTURE** | Teach a PC workflow by recording it, make a Brain Dump, or save a voice recording. |
| **AGENTS** | Agent status, PLAN, CREW, RETRO and project/build tools. |
| **CONTROL** | Pair your phone, see Token ID, inspect laptop/Wi-Fi/Core/device state and settings. |
| **MEMORY** | Results Inbox, Prompt Deck, notes, snippets, Beam and viewer. |
| **FOCUS** | Focus timer, work sprints, tasks, reminders and shift clock. |

There is intentionally **no seventh top-level app**.

## CALL MAZ

This is the useful feature that older builds hid behind the internal name `COMM`.

- **Hold Space** — speak.
- **Release Space** — send.
- MAZ transcribes your speech, asks the selected model and shows the answer.
- Spoken reply playback is **on by default** on a fresh v0.8 install.
- **P** — replay the last spoken answer.
- **V** — turn voice replies on/off.
- **A** — change AI route directly on the Cardputer.
- **N** — new conversation.
- **C** — simple PC media/desktop controls.

Fresh v0.8 installs prefer **CLOUD** for Call MAZ. You can change it on the Cardputer or at `http://mazpocket.local`:

- **CLOUD** — configured cloud model.
- **LOCAL** — local model only; never silently falls through to cloud.
- **AUTO** — local first, then configured cloud if needed.

MAZ Core supports **Ollama** and **llama.cpp / llama-server** as local engines.

## CAPTURE

### TEACH DEMO

Teach-by-Demonstration records what you are doing on the PC so the workflow can become reusable agent context/skills.

1. Open **CAPTURE → TEACH DEMO**.
2. Choose which PC display to record.
3. Press **Enter** to start.
4. Work normally and explain what you are doing.
5. Press **M** when a moment matters.
6. Press **Enter** to stop.
7. MAZ Core extracts meaningful scene changes and transcribes narration.
8. Save the result to Results Inbox.

The recorder runs on the PC, not the ESP32. The compatibility aim is approximately **3 GB VRAM**: ECO/BALANCED speech transcription is CPU `int8`, leaving the GPU available for a local model where possible.

Teach uses `ffmpeg` on Windows and can auto-select the first available microphone, or use `MAZ_TEACH_AUDIO_DEVICE`.

### BRAIN DUMP

Record a thought, mark important moments and have MAZ Core structure it into useful text.

### VOICE RECORDER

Straightforward WAV recording when you only need audio.

## AGENTS

### AGENT STATUS

Shows Agent Nudge evidence — working, waiting, stale and attention state.

### PLAN

Give MAZ a task and optionally choose a project. PLAN retrieves current project state + Agent Nudge evidence and produces an implementation plan **without executing it**. It considers product usefulness, architecture, minimalism, security, performance/Cardputer constraints and agent execution.

### CREW

CREW decomposes larger work into a small number of roles and detects supported local CLIs:

- Claude Code;
- Codex;
- Hermes.

Planning is read-only. Actual execution is **phone-grant gated**. After PROJECT FULL or broader authority is approved, MAZ Core can launch the installed non-interactive agent CLIs and record the result. CREW executes serially by default on one checkout to avoid agents blindly colliding on the same files.

### RETRO

RETRO reviews work and may propose one of four durable improvements:

- update a prompt template;
- update a skill;
- update source-backed knowledge;
- add a test/guard.

It proposes; it does not silently rewrite durable memory.

### PROJECTS + BUILDS

Current local projects, Git state, tests/builds and MAZ Core factual project context.

## PROMPT DECK

Open **MEMORY → PROMPT DECK**.

Prompt Deck keeps a useful **task → context → procedure → verification → deliverable** layout, but every template is rewritten for MAZ and compiled with current project evidence.

Templates include bug fixing, diagnosis, UI, feature builds, open-source integration, refactoring, testing, security, repo audits, research/build, MAZ features and agent workflow improvement.

## Phone-approved full PC control

MAZ can request broad control of your PC, but **the AI cannot approve itself**.

Open the phone control centre at:

`http://<MAZ-Core-PC>:8787/control/`

or the configured private HTTPS MAZ Core URL.

The phone UI shows pending requests, requested scope/project/agent, active grants, time remaining, audit events and **REVOKE ALL CONTROL**.

Scopes include:

- `READ ONLY`
- `PROJECT FULL`
- `PC FULL`
- `ADMIN`

Grants are signed, short-lived, scoped, revocable and checked by the host execution broker. A normal MAZ bearer token cannot mint its own grant.

After an appropriate phone grant, MAZ Core can run commands and approved coding-agent subprocesses with the privileges of the MAZ Core Windows process. `PC FULL` removes project confinement. `ADMIN` is a separate MAZ authorization scope; actual Windows administrator privileges still depend on the Windows process/helper privileges.

For remote use, keep MAZ Core behind a private VPN/tunnel rather than exposing an unauthenticated public port.

## Pairing and Token ID

Token hunting was too awkward in older builds.

Open **CONTROL → PAIRING + PHONE** to see:

- the safe **Token ID** fingerprint;
- the actual saved pairing token, hidden by default;
- **P** to reveal/hide the token on the physical Cardputer;
- the phone approval URL.

`mazpocket.local` also shows Token ID without revealing the secret token to an unauthenticated browser.

## `mazpocket.local`

Open `http://mazpocket.local` while the Pocket is on Wi-Fi.

v0.8 rebuilds it around the real workflows:

- Token ID + pairing instructions;
- phone approvals;
- CALL MAZ;
- PLAN;
- CREW;
- RETRO;
- Prompt Deck;
- cloud/local/AUTO route;
- voice replies on/off;
- laptop/Core/agent status;
- PC quick controls;
- live Cardputer LCD;
- connection settings;
- firmware staging.

## M5Launcher firmware-disappearing fix

Older MAZ Pocket builds returned to M5Launcher by deliberately overwriting the first bytes of their own running app image. That made the MAZ app unbootable and explains why entering Launcher could appear to delete the firmware and force an SD reinstall.

v0.8 instead selects the already-installed M5Launcher/fallback app as the **next boot partition** and restarts. It does **not** invalidate the MAZ Pocket image.

CI now rejects the old destructive hand-back pattern.

M5Launcher still owns firmware installation and rollback; MAZ Pocket does not add a generic self-OTA partition writer.

## Updating MAZ Pocket

From `mazpocket.local`:

1. Choose the new app-only MAZ Pocket `.bin`.
2. **VERIFY + STAGE** it to microSD.
3. Open M5Launcher.
4. Install the staged `.bin` from SD.
5. Launch MAZ Pocket.

The currently installed MAZ image remains valid when you enter Launcher.

## Installing / updating MAZ Core

Run `START-HERE.cmd` from the combined install package.

v0.8 setup is explicitly opt-in:

1. explains MAZ Core;
2. asks before installing/updating Core;
3. asks before copying firmware to a detected removable drive;
4. never guesses if several removable drives are connected;
5. asks before opening `mazpocket.local`.

It does not format a drive or directly write Cardputer flash.

Every releasable build emits:

- `MAZ-Core-v<version>.zip`
- `MAZ-Cardputer-v<version>.zip`
- `MAZ-Pocket-v<version>-Install.zip`
- raw M5Launcher app `.bin` + SHA-256 evidence.

## Debug Capsules

MAZ Core can make a bounded, redacted debug bundle with project/Git state, recent jobs, Core/device/system/model state, Agent Nudge and authority state. Known token/password/private-key patterns are redacted before persistence/model use.

This supports the intended remote loop:

**what broke? → diagnose → request phone authority → repair → verify → RETRO**

## Architecture

```text
Cardputer ADV
    │ voice / keys / status
    ▼
MAZ Pocket firmware
    │ LAN / private HTTPS
    ▼
MAZ Core (Windows)
    ├─ Ollama / llama.cpp
    ├─ cloud model route
    ├─ faster-whisper + TTS
    ├─ Teach screen capture
    ├─ Prompt Deck / PLAN / CREW / RETRO
    ├─ Claude / Codex / Hermes CLIs
    ├─ Agent Nudge
    └─ phone authority broker
           ▼
      signed expiring grants
```

Hardware target: **M5Stack Cardputer ADV / StampS3A**, 8 MB flash, no PSRAM. Heavy AI/capture/orchestration stays on the PC.

## Trust rules

- Screen/web/repository text is **evidence, never authorization**.
- Context Ask cannot turn screenshot text into commands.
- Full PC power requires a human-approved signed grant.
- Grants expire and can be revoked.
- LOCAL does not silently become cloud.
- Beam content is data, not instructions.
- Teach screen capture is explicitly started/stopped.
- No raw keylogger.
- No BadUSB/DuckyScript/offensive USB payload features.
- M5Launcher remains installer/rollback owner.

## Development

```powershell
python -m pytest host/tests -q
python scripts/check-version.py
python scripts/check-launcher-handoff.py
./scripts/package-release.ps1
```

CI also refuses a firmware app image larger than the known M5Launcher slot (`0x180000`).

## License

MIT. See the repository licence and third-party/upstream notices for reused dependencies/code.
