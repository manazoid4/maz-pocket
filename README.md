# MAZ Pocket

MAZ Pocket is standalone firmware for the **M5Stack Cardputer ADV** plus **MAZ Core**, the lightweight Windows companion that supplies AI, PC context and safe actions.

## Current release candidate: v0.7.1 — FIELD (installer hotfix)

v0.7 keeps the six-surface product intact:

- **COMM** — voice conversation, Context Ask and safe PC controls.
- **CAPTURE** — field capture, recordings and BrainDump.
- **OPS** — Agent Nudge evidence, status and nudges.
- **CONTROL** — Wi-Fi, laptop status, Beam, MAZ Core, diagnostics and settings.
- **RECALL** — inbox, received Beams, notes, snippets and viewer.
- **FLOW** — shift clock, reminders, focus, sprint and tasks.

No seventh top-level app was added. The Cardputer remains a pocket command surface rather than an ESP32 app drawer.

## Home: NOW + four quick keys

Home now uses the existing compact strip as **NOW**. It shows one high-priority fact such as an agent needing you, queued offline work, an active shift, a due reminder, a received Beam or Core being offline.

Keys **1–4** launch four persistent quick actions. Defaults are:

`1 TALK   2 NOTE   3 LAP   4 BEAM`

Hold **Fn + 1/2/3/4** to cycle that slot through the fixed safe list: Talk, Capture, Laptop, Beam, Shift, Focus 25, Lock PC, Play/Pause, Mute, Recall, Flow and M5Launcher. Quick actions are identifiers, never shell commands.

## Context Ask

From almost any screen, hold **Fn + Space**, speak the question, then release Space. MAZ Pocket captures only a tiny snapshot of the currently focused item and sends it as explicitly untrusted context with the voice question. It does not dump a database or expose secrets.

Examples: “summarise this”, “what should I do next?”, “what did I miss?”

## Beam

Beam moves short text/URLs between laptop and Pocket without pretending to be a file-transfer system or remote terminal.

- Laptop → Pocket: MAZ Core keeps a durable queue until the Cardputer pulls it.
- Pocket → laptop: queued offline on the Cardputer, then saved to Core history; Windows also copies it to the clipboard when available.
- Received text is **data only** and is never executed.
- `host/BEAM-TO-POCKET.cmd` beams the Windows clipboard (or supplied text) to the Pocket.

## Offline Outbox

COMM already protected failed recordings; v0.7 turns that into an automatic service. Voice turns, Context Ask recordings and outgoing Beams survive Core/Wi-Fi loss as durable records. A single existing Host worker retries the oldest queued item with bounded backoff when connectivity returns. No extra worker fleet or queue database is added.

## Laptop snapshot

CONTROL → LAPTOP shows an on-demand cached snapshot of:

- CPU and RAM use;
- NVIDIA GPU utilisation, VRAM and temperature when `nvidia-smi` is available;
- laptop battery/charging when Windows exposes it;
- Ollama loaded model, model VRAM and context length.

There is no background telemetry sampler thread. The laptop does the work and caches it briefly; the Cardputer only renders the result.

## FIELD mode + Shift Clock

**Fn + F** toggles FIELD mode. FIELD mode shortens screen dim timing, slows low-value background refreshes and keeps Outbox/Beam useful. FLOW → SHIFT CLOCK stores completed shift elapsed time locally and makes the active shift the Home NOW item.

## Smarter, lighter local AI

MAZ Core exposes one understandable `MAZ_AI_PROFILE`:

- **smart** (default): ordinary Pocket turns use 4096 context, grow only for larger grounded prompts, and keep the local model warm for 5 minutes;
- **save**: small context and immediate unload after a response;
- **fast**: adaptive context and a 30-minute warm model.

LOCAL still never silently falls through to cloud. AUTO still tries configured local models before optional cloud. MAZ Core records Ollama load/prompt/output timing metadata so performance changes can be measured instead of guessed.

## Install / update

Fresh Windows setup remains `START-HERE.cmd`. M5Launcher remains the only firmware installer/rollback owner; MAZ Pocket never writes firmware partitions itself.

Every releasable build now **must** emit:

- `MAZ-Core-v<version>.zip` — complete laptop/client package;
- `MAZ-Cardputer-v<version>.zip` — complete app-only handheld package;
- `MAZ-Pocket-v<version>-Install.zip` — convenience bundle containing both;
- the raw M5Launcher `.bin` and SHA-256 evidence.

This is enforced in CI and documented in `docs/RELEASE_RULES.md`.

## Safety / rollback boundary

The target is StampS3A / ESP32-S3FN8 with 8 MB flash and no PSRAM. The firmware image is app-only for M5Launcher and must not be flashed at address `0x0`. Generic direct self-OTA remains disabled. PC actions remain allow-listed; Beam cannot execute received text; there is no arbitrary remote shell.

CI can prove compile/tests/package integrity, not physical hardware behaviour. v0.6 remains the rollback release until v0.7 passes the real-device field gate in `docs/V070-FIELD.md`.
