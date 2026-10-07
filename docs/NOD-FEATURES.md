# nod feature status (honest)

Rule: PROVEN = owner live test or device evidence. Merged or CI-green is NOT proven.
Status words: PROVEN / UNPROVEN (built, tests pass, nobody has seen it work) / BROKEN-UNKNOWN (was failing, fix merged, not re-tested).
Firmware/Core version: see `VERSION`. Core and device paths below are relative to the repo root.

| Feature | How to use (keys / action) | Where in code | Status |
|---|---|---|---|
| Call (voice to Core, smart reply) | Hold SPACE in Call, speak, release | `src/apps/comm.cpp`, `POST /turn/raw` in `host/mazhost/app.py` | PROVEN (owner live calls; 10/10 no-refusal test) |
| Call from any screen | Hold Ctrl+SPACE | `src/core/shell.cpp` | UNPROVEN |
| Spoken reply (hear the voice) | Reply plays after Call; P replay, V toggle voice | `src/audio/voice.cpp`, `host/mazhost/tts.py` | BROKEN-UNKNOWN (owner heard nothing on v1.0.1; fix #49 merged, not re-tested) |
| Speaker test | Tools > Speaker test, or serial `MAZSPK` | `src/apps/system_apps.cpp`, `src/net/control.cpp` | UNPROVEN |
| Voice picker | Settings > Voice (ENTER saves); phone control page | `src/apps/system_apps.cpp` (VoiceApp), `host/mazhost/voices.py`, `host/VOICES.md` | UNPROVEN |
| Voice endpoints | `GET /voices`, `POST /voices/select`, `GET /voices/search`, `POST /voices/preview` (Bearer token) | `host/mazhost/voices.py` | UNPROVEN |
| About screen shows version | Home > About | `src/apps/home.cpp`, `/system/status` | PROVEN |
| Wi-Fi OTA to device (manual stage) | Stage bin in Core, Home shows "U: update", press U | `src/net/host_worker.cpp`, `GET /fw/manifest`, `GET /fw/latest.bin` | PROVEN (1.0.0 -> 1.0.1) |
| Update from any screen | Ctrl+U, then Y to confirm | `src/core/shell.cpp` | UNPROVEN |
| Core self-update | Automatic poll (default 5 min); `GET /core/update`, `POST /core/update/check` | `host/mazhost/selfupdate.py`, `host/AUTOUPDATE.md` | UNPROVEN (repo is private: needs `MAZ_GITHUB_TOKEN` or a public repo) |
| CI + release | PR runs `nod-ci.yml`; merge to deploy/local runs `nod-release.yml` | `.github/workflows/` | PROVEN for release `nod-v1.0.0-b1` (published by CI) |
| Core survives reboot | Scheduled task "nod Core" | `host/install-core-task.ps1` | PROVEN |
| Tailscale remote | `host/setup-remote.ps1` (admin), Funnel URL in `MAZ_REMOTE_URL`; device falls back LAN -> remote | `host/mazhost/remote.py`, `host/REMOTE.md` | UNPROVEN (Funnel not on yet) |
| Approvals, Core side | Claude Code `PermissionRequest` hook asks Core; decide with `POST /buddy/decide {id, decision}`; 60 s timeout falls back to terminal prompt | `host/hooks/nod_permission_hook.py`, `host/mazhost/buddy.py` (`/buddy/request`, `/buddy/state`, `/buddy/pending`, `/buddy/allow-all`) | UNPROVEN; no device screen yet |
| Dictation, Core side | `POST /dictate` (wav in, cleaned text out; `?target=pc-paste` pastes on PC) | `host/mazhost/app.py`, `host/mazhost/flow.py` | UNPROVEN |
| Dictation, PC agent | Hold Right Ctrl, speak, release, text pastes at cursor | `host/nodflow.py`, `host/install-nodflow.ps1` | UNPROVEN |
| Dictation, device | Hold Ctrl+SPACE in Command palette (Ctrl+K), speak | `src/audio/dictate.cpp`, `src/core/shell.cpp` | UNPROVEN |
| Focus sprint, Core side | `POST /focus/start {minutes 1-180, task}`, `GET /focus/state`, `POST /focus/end`, `GET /focus/events` | `host/mazhost/focus.py` | UNPROVEN; no device screen for it |
| On-screen call errors | Failures shown on Call screen | `src/apps/comm.cpp`, `host/mazhost/errors.py` | UNPROVEN |

Dropped from the old table (no such code or no proof of the claim): "Reply text before voice", "Spoken fallback", "Groq brain chain" as a separate feature (the smart reply is covered by Call), zip packages.

## Update flow

1. Open a PR into `deploy/local`.
2. CI (`nod-ci.yml`): Core tests, firmware build, 1.5 MB size gate, manifest. Must be green.
3. Merge to `deploy/local`. `nod-release.yml` publishes release `nod-v{VERSION}-b{N}` with two assets: `nod-fw.bin` and `nod-manifest.json`. No zip packages.
4. Core self-update (`selfupdate.py`) finds the newest `nod-v*` release, verifies size and sha256, stages the firmware, and git-pulls Core code with rollback if `/health` does not come back. Needs the repo public or `MAZ_GITHUB_TOKEN`; the repo is currently private, so this step is the likely blocker until one of those is done. One-time owner step: `host\BOOTSTRAP-AUTOUPDATE.cmd`.
5. On the device: the Home screen shows "U: update". Press Ctrl+U (any screen), then Y to confirm. Device reboots into the new firmware.

Only step 5 pressed by hand plus the Wi-Fi OTA itself are owner-proven; steps 3-4 end to end are not.

## Setup notes

- Approvals hook: merge the `PermissionRequest` block from `host/hooks/README.md` into `~/.claude/settings.json`. Env: `MAZ_TOKEN`, `MAZ_CORE_URL` (default `http://127.0.0.1:8787`), `MAZ_BUDDY_TIMEOUT` (default 60).
- Dictation and the smart reply need `MAZ_GROQ_KEY` in Core's `.env` (the field is `groq_key` in `host/mazhost/config.py`; earlier docs named `MAZ_GROQ_API_KEY`, which may be a different name: check the installed `.env`).
- Tailscale: follow `host/REMOTE.md`.
