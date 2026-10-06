# nod permission hook

Claude Code `PermissionRequest` hook. Sends tool + one-line summary to Core
`/buddy/request`, long-polls `/buddy/state`. Device allow/deny -> decision JSON.
60 s timeout or Core down -> silent exit 0 (+ `ask` on stderr) = normal terminal prompt.

Env: `MAZ_TOKEN` (Core token), `MAZ_CORE_URL` (default `http://127.0.0.1:8787`), `MAZ_BUDDY_TIMEOUT` (default 60).

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

Device endpoints: `GET /buddy/pending`, `POST /buddy/decide {id, decision: allow|deny|allow_all}`,
`POST /buddy/allow-all {session_id, on}`. `allow_all` auto-allows later requests of that Claude session.
