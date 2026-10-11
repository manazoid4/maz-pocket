# nod Halt

One key on your nod stops every AI agent that asks nod for permission.
Press it again to let them ask again.

## Use it

- On the device: press **Fn + H**. The top bar flashes **HALTED** in red and you hear a low falling tone. Press **Fn + H** again to resume.
- Halt lasts **30 minutes** by default, then turns itself off. The device chirps "Halt off".
- While halted, every new approval request is denied straight away with the reason `Halted from nod`. The agent is told no. Requests that were still waiting when you pressed the key are denied too.

## What Halt does not do

- It **only affects actions that ask nod for permission**. It does **not** kill a running process, stop a command already started, or block an agent that never asks nod.
- It can **never approve** anything. Halt only says no.
- Halt is kept in the hub's memory. **Restarting the hub clears Halt**; turn it on again if you still need it. Your nod notices on its next check and removes the banner.
- Turning Halt on clears all session-scoped allow-all grants. New allow-all grants are rejected while Halt is on. Requests denied by Halt stay denied after you resume.
- The device checks the hub every 15 s (45 s in field mode). The key press is sent on the next check, so the banner can take a moment. Agents are blocked as soon as the hub has the request.
- If the device cannot confirm the hub state, it shows **HALT?** in red. Treat this as unknown; the device will not show Halt as off until the hub confirms it.

## Hub API

Same token as the other `/buddy` calls (`Authorization: Bearer <token>`).

`POST <hub>/buddy/halt`

| Field | Meaning |
|---|---|
| `on` | `true` to halt, `false` to resume. Required. |
| `minutes` | Optional, 1 to 480, default 30. `on: true` again extends from now. |

`GET <hub>/buddy/halt` returns `{"ok": true, "halted": false, "until": null}`. When halted, `until` is the UTC time Halt switches itself off.

Bad `minutes` is HTTP 422. A missing or wrong token is HTTP 401.

When Halt turns on or off, the hub also posts a `needs` event (`Halt on` / `Halt off`, source `halt`) so devices chirp. See [AGENT-EVENTS.md](AGENT-EVENTS.md).

```sh
curl -s -X POST "$NOD_CORE_URL/buddy/halt" -H "Authorization: Bearer $NOD_TOKEN" \
  -H "Content-Type: application/json" -d '{"on":true,"minutes":60}'
```

## What an agent sees

The Claude Code permission hook (`host/hooks/nod_permission_hook.py`) prints a deny with the message `Halted from nod`. Any other agent that asks `POST /buddy/request` gets `"decision": "deny"` and `"reason": "Halted from nod"` in the reply.
