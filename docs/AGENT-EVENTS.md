# nod agent events

One small contract. Any AI agent, script or automation can ping your nod when
it finishes, hits a limit, or needs you. The nod beeps and shows a toast.

**Notify only.** An event can never approve, deny, run or trigger anything.
It only makes the device show a message.

## Send an event

`POST <hub>/buddy/event` with a JSON body. The hub is your nod Core
(default `http://127.0.0.1:8787`).

| Header | Value |
|---|---|
| `Authorization` | `Bearer <your nod token>` |
| `Content-Type` | `application/json` |

| Field | Required | Meaning |
|---|---|---|
| `kind` | yes | `done`, `usage`, `needs` or `error` |
| `text` | no | What happened. Cut to 160 characters. |
| `source` | no | Free-text agent name, e.g. `codex`. Up to 40 characters. Shown on the device as `codex: <text>`. |

Anything else is rejected (HTTP 422). A missing or wrong token is HTTP 401.

## What the device does

| kind | Toast title | Sound |
|---|---|---|
| `done` | Agent done | rising two-note chirp |
| `usage` | Usage | warning tone |
| `needs` | Needs you | warning tone |
| `error` | Agent error | error tone |

What your agents did today, in numbers: [TODAY.md](TODAY.md).

## Limits

- The hub keeps the last 20 events. The device checks every 15 s (45 s in field mode), so a ping can take that long to appear.
- Events sent while the device is off are not replayed when it starts.
- Keep calls short: use a timeout of 3 s and ignore failures.

## Examples

curl:

```sh
curl -s -m 3 -X POST "$NOD_CORE_URL/buddy/event" \
  -H "Authorization: Bearer $NOD_TOKEN" -H "Content-Type: application/json" \
  -d '{"kind":"done","text":"build finished","source":"my-script"}'
```

Python:

```python
import json, os, urllib.request
req = urllib.request.Request(os.environ["NOD_CORE_URL"] + "/buddy/event",
    data=json.dumps({"kind": "done", "text": "build finished", "source": "my-script"}).encode(),
    headers={"Authorization": "Bearer " + os.environ["NOD_TOKEN"], "Content-Type": "application/json"})
urllib.request.urlopen(req, timeout=3)
```

PowerShell:

```powershell
$h = @{ Authorization = "Bearer $env:NOD_TOKEN" }
$b = @{ kind = "done"; text = "build finished"; source = "my-script" } | ConvertTo-Json
Invoke-RestMethod "$env:NOD_CORE_URL/buddy/event" -Method Post -Headers $h -ContentType "application/json" -Body $b -TimeoutSec 3
```

## Ready-made command

`host/hooks/nod_notify.py` does the above for you, never fails loudly and always
exits 0, so it is safe inside any agent hook:

```sh
python nod_notify.py done "build finished" --source my-script
```

Env: `NOD_TOKEN` and `NOD_CORE_URL`. Setup for each agent: `host/hooks/ADAPTERS.md`.
