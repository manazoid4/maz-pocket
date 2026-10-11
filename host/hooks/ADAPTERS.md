# nod adapters

Every adapter ends in the same call: `nod_notify.py <kind> "<text>" --source NAME`
(contract: `docs/AGENT-EVENTS.md`). Notify only, nothing here can approve anything.
Set `NOD_TOKEN` (and `NOD_CORE_URL` if the hub is not on this machine) in the
environment of the agent. Replace `/path/to/nod` with where you cloned the repo.

Status key: **Docs-claimed** = config keys as written in PR #83 from the vendor docs of
2026-10-11; NOT re-checked during integration. **Untested-on-device** = no real nod has
received a ping from it yet. Every adapter here is Untested-on-device.

## Claude Code - Docs-claimed (existing hook in this repo), Untested-on-device

`~/.claude/settings.json`:

```json
{
  "hooks": {
    "Stop": [
      { "hooks": [ { "type": "command", "command": "python /path/to/nod/host/hooks/nod_stop_hook.py", "timeout": 5 } ] }
    ]
  }
}
```

## Codex CLI - Docs-claimed, Untested-on-device

Docs: https://learn.chatgpt.com/docs/config-file/config-advanced (`notify`, event
`agent-turn-complete`, "receives a single JSON argument"). In `~/.codex/config.toml`:

```toml
notify = ["python3", "/path/to/nod/host/hooks/nod_notify.py", "done", "--source", "codex"]
```

`nod_notify.py` reads the JSON argument and uses `last-assistant-message` as the text.
(Assumes Codex appends its JSON after the listed arguments; if your build
differs, wrap it in a one-line script.)

## Gemini CLI - Docs-claimed, Untested-on-device

Docs: https://github.com/google-gemini/gemini-cli/blob/main/docs/hooks/index.md and
`.../hooks/reference.md` (hooks in `settings.json`, event `AfterAgent`, matcher `"*"`,
stdin JSON has `prompt_response`; `Notification` events carry `message`). In
`~/.gemini/settings.json`:

```json
{
  "hooks": {
    "AfterAgent": [
      { "matcher": "*", "hooks": [ { "type": "command", "command": "python3 /path/to/nod/host/hooks/nod_notify.py done --source gemini" } ] }
    ]
  }
}
```

The docs say stdout must be only JSON; the command prints nothing.

## Cursor - Docs-claimed, Untested-on-device

Docs: https://cursor.com/docs/hooks (`~/.cursor/hooks.json`, `version: 1`, event `stop`).
The `stop` input has no message text, so pass fixed text:

```json
{
  "version": 1,
  "hooks": {
    "stop": [ { "command": "python3 /path/to/nod/host/hooks/nod_notify.py done \"Cursor finished\" --source cursor" } ]
  }
}
```

## n8n / Make / Zapier - Untested-on-device

Use the node that sends an HTTP request (n8n: HTTP Request, Make: HTTP "Make a request",
Zapier: Webhooks "POST"). Untested: node field names differ per product; set the same values as the curl call:

- Method `POST`, URL `<hub>/buddy/event`
- Header `Authorization: Bearer <token>`, `Content-Type: application/json`
- Body `{"kind":"done","text":"Flow finished","source":"n8n"}`

## Plain shell - Untested-on-device

```sh
long-command; python /path/to/nod/host/hooks/nod_notify.py done "build finished" --source shell
```

Any other agent: **Untested: use the generic CLI from the agent's own notify/hook mechanism.**
