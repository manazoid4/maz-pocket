# Approve Claude Code actions from the Cardputer

1. Claude Code's `PermissionRequest` hook (`host/hooks/nod_permission_hook.py`) POSTs `/buddy/request {tool, summary, project, session_id}` to Core and long-polls `/buddy/state?id=..&wait=..`.
2. The device polls one tiny `GET /buddy/summary` on the shared host worker (5 s awake, 12 s screen-off, 2.5 s while an overlay is up; not logged by Core). It returns `{agent, count, items[{id,tool,summary,project,risk,hash,left}]}`. `summary` is the full request text (Core accepts up to 2000 characters, the hook no longer cuts it to 200); `risk` is READ, WRITE, NET, MCP or RUN (unknown tools count as RUN); `hash` is 6 hex characters of sha256(tool, NUL, full text).
3. A pending item opens a global overlay on any screen (tone, risk word and `#hash`, full tool name, the full request text in pages of 5 lines, project, 60 s countdown, page counter, "+N more" queue). `;`/`.` (or the arrow keys) change page. `N`/`ESC` deny at any time. `Y`/`ENTER` first reads on page by page; on the last page the first press arms approve for 4 s ("Y/ENT again = APPROVE") and the second press approves. Any page change disarms. A key that only wakes the screen never decides. There is no allow-all key.
4. The device POSTs `/buddy/decide {id, decision: allow|deny}`, shows "Approved"/"Denied" for 1 s, then shows the next queued request or the previous screen.
5. Request states: pending -> allow | deny | cancel (an expired request is `cancel`; Core answers HTTP 409 `approval_expired` and the device shows "Expired"). The hook sends `cancel` after 60 s or on error; the terminal prompt then appears as normal. Overlay disappears if it times out, is cancelled, or Core stops answering (3 failed polls).
6. Status light in the status bar: green = agent active in the last 90 s, amber = waiting on you, none = idle (`agent` field of `/buddy/summary`).
7. Core unreachable when deciding: the device retries twice, then shows "Core unreachable - use terminal"; the hook falls back to the terminal at timeout. Phone fallback is `/control`.
8. There is no timed "allow tool for 10 minutes" and no session-wide allow: the `allow_all` decision and `/buddy/allow-all` route do not exist (HTTP 422 / 404).

## Test without Claude Code

    pwsh host/hooks/test-approval.ps1 -Summary "git push --force" -Tool Bash   # Windows
    bash host/hooks/test-approval.sh "git push --force" Bash maz-pocket        # Linux/macOS

Reads `MAZ_TOKEN` from the environment or `host/.env` (never printed), posts a fake request, waits up to 60 s, prints the decision. Flash the firmware first; the overlay should appear within ~5 s on any screen.

## Real use: `~/.claude/settings.json`

    {
      "hooks": {
        "PermissionRequest": [
          { "matcher": "",
            "hooks": [ { "type": "command",
                         "command": "python C:/Users/manaz/maz-pocket/host/hooks/nod_permission_hook.py",
                         "timeout": 75 } ] }
        ]
      }
    }

Set `MAZ_TOKEN` (and `MAZ_CORE_URL` if Core is not on 127.0.0.1:8787) in the environment Claude Code runs in.
