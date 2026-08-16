# MAZ Pocket v0.7.1 — FIELD (installer hotfix)

v0.7.1 is a release-engineering hotfix on top of v0.7 FIELD. Every Pocket and MAZ Core capability below is unchanged; only the Windows install path and its CI gate were repaired.

## What was broken in v0.7

`START-HERE.cmd` launches `powershell.exe`, which on a normal Windows machine is Windows PowerShell 5.1. That edition decodes a script with no byte-order mark using the ANSI code page, so the two UTF-8 em dashes in `setup-all.ps1` were read as cp1252 characters ending in `0x94` — a smart closing quote that PowerShell accepts as a string delimiter. The quoting silently unbalanced and setup died with misleading errors:

```
setup-all.ps1:85 char:5   Unexpected token '}' in expression or statement.
setup-all.ps1:89 char:7   Unexpected token 'elseif' in expression or statement.
```

The braces were never unbalanced. CI missed it because it parsed repository sources under `pwsh` (PowerShell 7), which reads UTF-8 correctly and never exercised the shipped ZIP or the 5.1 fallback.

## What v0.7.1 changes

- Executed installer scripts (`START-HERE.cmd`, `setup-all.ps1`, `INSTALL-MAZ-POCKET.cmd`, `install-to-sd.ps1`) are plain ASCII, enforced by CI and again by `prepare-release.ps1` at packaging time.
- Those scripts ship with a UTF-8 BOM so Windows PowerShell 5.1 cannot guess the encoding.
- `START-HERE.cmd` prefers `pwsh.exe` when installed and still falls back to Windows PowerShell 5.1.
- CI builds the release package, extracts the generated `MAZ-Pocket-v0.7.1-Install.zip`, and parses plus dry-runs the scripts inside it with real `powershell.exe` on both the 5.1 fallback and the preferred shell. A syntax or encoding regression now fails the build.

Upgrading is Core-side and installer-side only. v0.7 firmware behaviour is unchanged; reinstall the v0.7.1 `.bin` through M5Launcher so device and Core report the same version.

---

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
