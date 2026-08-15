# MAZ Pocket v0.7 — FIELD

v0.7 turns the v0.6 friction foundation into a useful everyday/field command surface without expanding the six-surface Home menu.

## Pocket changes

- **NOW on Home** chooses one useful priority: agent question, Outbox, shift, reminder, Beam, stale/waiting agents or Core-offline state.
- **Four persistent quick keys** on 1–4; Fn+number cycles each slot through a fixed allow-list.
- **Context Ask** on Fn+Space captures a bounded snapshot of the focused screen/item and pairs it with a spoken question.
- **Automatic Offline Outbox** retries failed voice turns, Context Ask audio and outgoing Beams with bounded backoff.
- **Beam** sends/receives short text and URLs. Received data is never executable.
- **Laptop** view shows one cached CPU/RAM/GPU/VRAM/battery/Ollama snapshot instead of streaming graphs.
- **FIELD mode** (Fn+F) reduces background refresh and shortens display dim timing while keeping Outbox/Beam active.
- **Shift Clock** lives in FLOW and saves completed elapsed time.
- Home no longer hard-codes the stale `MAZ 0.5` label.
- Small `Fn+M` Home Easter egg adds retro flavour without another dependency or game engine.

## MAZ Core / laptop changes

- Durable Beam queue/history with Windows clipboard handoff for Pocket -> laptop.
- `BEAM-TO-POCKET.cmd`/`beam.ps1` sends clipboard text or a supplied string to the handheld queue.
- On-demand laptop telemetry uses psutil, `nvidia-smi` when available and Ollama `/api/ps`; no sampler thread.
- `MAZ_AI_PROFILE=smart|save|fast` separates performance/resource policy from LOCAL/AUTO/CLOUD routing.
- SMART defaults ordinary local requests to 4096 context and a 5-minute keep-alive, increasing context only for larger grounded prompts.
- SAVE unloads immediately; FAST keeps the local model warm for 30 minutes.
- Ollama response metadata records context, load time, prompt/output tokens and total time in `/models` status.
- MAZ Core/FastAPI public status now derives version identity from the release `VERSION` file instead of stale literals.

## Packaging rule added

Every releasable PR/build must produce all three ZIPs:

1. `MAZ-Core-v0.7.zip` — laptop/client.
2. `MAZ-Cardputer-v0.7.zip` — Cardputer/M5Launcher package.
3. `MAZ-Pocket-v0.7-Install.zip` — convenience bundle containing both.

CI fails if the split packages are missing. `docs/RELEASE_RULES.md` makes this a permanent project rule.

## Deliberately not added

No new top-level app, weather/news/browser/email client, arbitrary remote shell, realtime monitoring daemon, second framebuffer, realtime microphone streaming refactor or direct self-OTA.

## Physical validation gate

CI passing is not hardware proof. Before calling v0.7 field-proven, test on the actual Cardputer ADV:

- 30+ minute normal soak and 30+ minute FIELD soak with no reset/watchdog;
- 20+ ordinary COMM turns plus 10+ Context Ask turns from different screens;
- leave/re-enter COMM mid-request without losing the recording/result;
- Wi-Fi/Core loss during voice, then automatic Outbox retry after reconnect;
- multiple queued voice turns maintain oldest-first order;
- outgoing Beam while offline then reconnect; laptop -> Pocket Beam; QR/save paths;
- Laptop status refresh with Core up/down and NVIDIA tools present/absent;
- Fn+1..4 persistence across reboot; Fn+F FIELD persistence;
- shift start/stop and saved record;
- SD absent/unreadable/near-full while queueing audio;
- phone LCD polling and firmware staging remain responsive;
- M5Launcher hand-back, reinstall and v0.6 rollback;
- inspect `[health]` and `[host-worker]` heap/stack/loop evidence.

v0.6 remains the rollback release until this gate is satisfied.
