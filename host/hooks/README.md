# nod permission hook

Claude Code `PermissionRequest` hook. Sends tool + one-line summary to Core
`/buddy/request`, long-polls `/buddy/state`. Device allow/deny -> decision JSON.
60 s timeout or Core down -> silent exit 0 (+ `ask` on stderr) = normal terminal prompt.

Env: `NOD_TOKEN` (Core token), `NOD_CORE_URL` (default `http://127.0.0.1:8787`), `NOD_BUDDY_TIMEOUT` (default 60). Old `MAZ_*` names still work. Other agents: see `ADAPTERS.md` and `docs/AGENT-EVENTS.md`.

Install: add to `~/.claude/settings.json` (merge into existing `hooks`):

```json
{
  "hooks": {
    "PermissionRequest": [
      {
        "matcher": "",
        "hooks": [
          { "type": "command", "command": "python C:/Users/manaz/maz-pocket/host/hooks/nod_permission_hook.py", "timeout": 75 }
        ]
      }
    ]
  }
}
```

### Tokens: agents must not approve themselves

Set `MAZ_REQUEST_TOKEN` on Core (a different random value from `MAZ_TOKEN`) and give hooks
`NOD_REQUEST_TOKEN` (the hooks prefer it over `NOD_TOKEN`). That token can only call
`POST /buddy/request`, `GET /buddy/state`, `POST /buddy/cancel` and `POST /buddy/event`;
everything else, including `/buddy/decide`, is HTTP 403. Keep `MAZ_TOKEN` for the device and UI.
If only `MAZ_TOKEN` is set the hooks share it and an agent could approve its own request;
Core logs a warning at start (never the token value).

Device endpoints: `GET /buddy/pending`, `POST /buddy/decide {id, decision: allow|deny|cancel}`.
Every approval is for one request only: there is no `allow_all` decision or `/buddy/allow-all` route
and each request expires after 60 s (see `docs/APPROVALS.md`).

## nod stop hook (done ping)

`nod_stop_hook.py` is a Claude Code `Stop` hook. It POSTs `kind=done` (text = first line of the
last assistant message, else "Claude finished") to Core `/buddy/event`; the device chirps and
shows a toast. 3 s timeout, always exits 0, silent on failure. Same env as above. Notify only:
it never approves anything.

```json
{
  "hooks": {
    "Stop": [
      {
        "hooks": [
          { "type": "command", "command": "python /path/to/nod/host/hooks/nod_stop_hook.py", "timeout": 5 }
        ]
      }
    ]
  }
}
```

Other pings (usage limit, needs you, error): `POST /buddy/event {"kind": "done|usage|needs|error", "text": "...", "source": "..."}`;
device polls `GET /buddy/events?since=<id>` (ring buffer of last 20).
