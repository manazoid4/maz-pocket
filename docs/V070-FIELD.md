# v0.7 FIELD architecture and acceptance

## Product intent

v0.7 makes MAZ Pocket useful during ordinary work and a week away from the desk: glance at one priority, speak/capture quickly, queue safely offline, move short text between devices and inspect the laptop without turning the Cardputer into a monitoring server.

## Runtime shape

- **Home/UI task:** drawing, keyboard, immediate microphone feedback and tiny local state only.
- **One Host worker:** COMM network work, PC actions, Outbox retry, Beam pull/send and one-shot telemetry.
- **Durable Record store:** existing `outbox`, `inbox`, `reminder` contract plus `beam` and `shift` records; no second database.
- **MAZ Core:** AI/STT/TTS, telemetry, Beam persistence/clipboard and project context.

## NOW priority

1. Agent asks for Maz.
2. Outbox waiting.
3. Active shift.
4. Reminder due.
5. Received Beam.
6. Stale/waiting agent evidence.
7. Paired Core offline.
8. FIELD READY / READY.

Only one line is shown so Home remains readable at 240x135.

## Quick keys

1–4 store fixed action identifiers in NVS. Fn+number cycles the safe list. There is no user-entered command/script field.

## Context Ask

Fn+Space obtains `App::contextSnapshot()`, strips control/newline characters and caps it. The focused app describes only its selected/visible item. COMM records normally. For a contextual turn the Host worker transcribes the WAV, labels the context as **untrusted data**, then asks the normal model route. Failure stores the WAV + context in Outbox.

## Outbox

The Field service scans existing durable records at a low cadence. It retries the oldest queued item only when the single Host worker is idle. Failed network attempts use 15s -> 30s -> 60s -> ... -> 5m bounded backoff. Successful delayed AI replies land in Recall/Inbox and remove the WAV.

## Beam

- Text/URL only, capped at 2,000 characters.
- Core persists laptop -> Pocket queue until pull acknowledgement.
- Pocket -> laptop is first a local Outbox record, then Core history/clipboard.
- No automatic opening or execution of received content.

## Laptop telemetry

Core samples only on request and caches for two seconds. CPU/RAM/battery comes from psutil; NVIDIA GPU values come from a short `nvidia-smi` query when present; Ollama loaded-model information comes from `/api/ps`. FIELD mode requests less often.

## AI profile

SMART: 4K ordinary context, 6K/8K only as prompt size grows, 5m keep-alive.
SAVE: 4K ordinary context and `keep_alive=0`.
FAST: adaptive context and 30m keep-alive.

Routing remains independent: LOCAL cannot spill to cloud; AUTO may use cloud only after configured local models fail.

## Real-device acceptance gate

Run the checklist in `RELEASE_NOTES.md`, with special attention to queue ordering, Wi-Fi/Core reconnection, host-worker stack high-water, heap fragmentation, SD fault behaviour, Context Ask key press/release, FIELD screen dimming and M5Launcher rollback.
