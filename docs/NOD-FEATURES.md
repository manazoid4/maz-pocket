# MAZ Pocket v1.0.0 Feature Reference

## Features

| Feature | How to Use | Core Endpoint/File | Status |
|---------|-----------|-------------------|--------|
| **Call (voice)** | Hold SPACE in Call, or Ctrl+SPACE anywhere; speak & release | POST `/turn/raw`, voice playback via `tts.py` | PROVEN |
| **Reply text before voice** | Type before pressing SPACE in Call reply | Text buffering in Call screen | PROVEN |
| **Voice playback** | P key to replay; V to toggle; Tools > Speaker test; serial MAZSPK | `SpeechOut` class, TTS via Fish API | PROVEN |
| **Groq brain chain** | Dictate or chat; Groq Whisper + cleanup via Groq API | `/dictate`, `flow.py`, `stt.transcribe_prompted()` | UNPROVEN |
| **Spoken fallback** | If Groq unavailable, fall back to local Whisper | `flow.py` exception handling | UNPROVEN |
| **About version** | Home > About > firmware & Core version display | `/system/status`, device firmware version string | PROVEN |
| **Wi-Fi OTA update** | Home U or Ctrl+U anywhere; badge "^U upd" | Device-side: staged firmware pull via `mazpocket.local/update` | PROVEN |
| **Core self-update** | Auto-check GitHub Releases every 5 min; manual via UI | `selfupdate.py`, GitHub API token, `host/AUTOUPDATE.md` | PROVEN |
| **CI + releases** | GitHub Actions workflow; auto-builds & releases v1.0.0-b* | `.github/workflows/`, packaging scripts | PROVEN |
| **Tailscale remote** | MAZ_REMOTE_URL or auto-detect via Tailscale Funnel | `remote.py`, `RemoteGuardMiddleware`, Funnel polling | PROVEN |
| **On-screen call errors** | Network/model failures shown on Call screen | Error display in Call UI, `errors.py` | PROVEN |
| **Dictation (/dictate)** | POST audio WAV; returns cleaned text + intent (note/remind/paste) | POST `/dictate`, `flow.py`, Groq Whisper turbo + cleanup | UNPROVEN |
| **Approvals (buddy)** | Claude Code requests → Core `/buddy/*` → device approval | POST `/buddy/request`, `/buddy/state`, `buddy.py`, hook in `~/.claude/settings.json` | UNPROVEN |
| **Focus (sprint)** | Start timed focus; mutes nudges; fires "Done?" event on timer | POST `/focus/start`, `/focus/state`, `focus.py` | UNPROVEN |

## Update flow

1. **Phone/Claude**: Create PR or commit to repo
2. **CI**: Builds firmware, runs tests, creates release tagged `nod-v1.0.0-b<N>`
3. **GitHub Releases**: Publishes `MAZ-Core-v1.0.0.zip`, `MAZ-Cardputer-v1.0.0.zip`, `MAZ-Pocket-v1.0.0-Install.zip`
4. **Core auto-update**: Detects release via GitHub API; downloads & stages to `~/.maz-pocket/updates/`
5. **Device fetch**: User presses Ctrl+U (Home U) on Cardputer; pulls staged firmware
6. **Reboot**: Cardputer boots updated firmware; Core restarts cleanly

See `host/AUTOUPDATE.md` for Core update mechanics; `host/REMOTE.md` for Tailscale setup.

## New feature setup (owner device)

### Dictation (#43)
- **PC**: No additional setup; uses existing `MAZ_GROQ_API_KEY` or `MAZ_GROQ_KEY`
- **Device**: No screen UI yet; backend ready at `/dictate` endpoint
- **Use case**: Rapid capture via `/dictate` endpoint from mobile app or scripts

### Approvals (#41)
- **PC**: Add to `~/.claude/settings.json`:
  ```json
  {
    "hooks": {
      "PermissionRequest": [
        {
          "matcher": "",
          "hooks": [
            { "type": "command", "command": "python /path/to/maz-pocket/host/hooks/nod_permission_hook.py", "timeout": 75 }
          ]
        }
      ]
    }
  }
  ```
- **Env vars**: `MAZ_TOKEN`, `MAZ_CORE_URL` (default `http://127.0.0.1:8787`), `MAZ_BUDDY_TIMEOUT` (default 60)
- **Device**: No screen UI yet; pending requests visible at `GET /buddy/pending`
- **Use case**: Claude Code hook submits permission requests; device/phone approves before command executes

### Focus (#42)
- **PC**: No configuration needed; POST `/focus/start` with `minutes` (1–180) and optional `task`
- **Env**: Optional `MAZ_NOW_FILE` (defaults to `~/Desktop/Maz Works Knowledge Vault/NOW.md`) and `MAZ_FOCUS_LOG` for event logging
- **Device**: No screen UI yet; state at `GET /focus/state`, events at `GET /focus/events`
- **Use case**: Timed focus sessions; nudge mute; "Done?" prompt at timer end
