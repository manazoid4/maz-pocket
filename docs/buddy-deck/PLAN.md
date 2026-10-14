# MAZ Buddy Deck — Plan (2026-10-06)

Goal: Cardputer ADV becomes a remote Claude Buddy + gamified action deck, usable anywhere.
Then a Kickstarter path to a custom CNC-metal device.

## Decisions (owner, 2026-10-06)
- Remote first. Emulate Claude Buddy over MAZ Core (Wi-Fi / Tailscale), not BLE. BLE port later for the open-source PR.
- Deck keys v1: Agent control, Call MAZ voice, PC macros. Sales keys later.
- UI: minimal, big glyphs for 240x135, futuristic, gamified (XP, streak, rank).
- Product: maximise margin, cheap to make, raise capital. Research in RESEARCH.md.
- Device: same Cardputer ADV.

## Architecture
```
Claude Code (any session) --PermissionRequest hook--> Core /buddy/request (waits for decision)
Cardputer --polls--> Core /buddy/state      (pending prompt, sessions, XP)
Cardputer --Y/N-->   Core /buddy/decide     -> hook returns allow/deny to Claude Code
Timeout (60s) or no device -> hook falls through to the normal terminal prompt. Never auto-approve.
```
Remote: Cardputer reaches Core the same way beam/pull does today (LAN or Tailscale via phone hotspot).

## Slices (each ships working, proof required)
1. Core: `/buddy/request|state|decide` + XP counters (in memory, JSON file persist). Proof: curl round-trip.
2. Hook: `~/.claude/hooks/maz_buddy.py` PermissionRequest hook. Proof: real Claude Code prompt resolved from curl.
3. Firmware: BUDDY DECK home — 4 tiles (BUDDY, CALL, PC, AGENTS), toast + beep on pending prompt from any screen, Y/N, XP bar. Proof: build size < 1.5MB, flashed, photo via camera.
4. Roadmap: `KICKSTARTER-ROADMAP.md` from RESEARCH.md — BOM, CNC enclosure, certs, tiers, timeline, moat.
5. Open source: Cardputer port PR to anthropics/claude-desktop-buddy (BLE) — later, after 1-3 are stable.

## Out of scope now
Desk handset phone (next week), character packs, sales keys, BLE.

## Fixed today (Core)
- Core logs to `%LOCALAPPDATA%\MAZ Core\core.log`.
- Local route: llama.cpp (dead, 401) -> Ollama. Default route `auto`.
- Cloud key re-issued; `MazLatest` combo now free-first (Antigravity Gemini Flash -> gemini-cli -> OpenRouter free -> codex).
- `_mazlatest` sent no auth header — fixed in installed Core. Repo `host/` lacks this route (installed Core is ahead of repo; reconcile).
