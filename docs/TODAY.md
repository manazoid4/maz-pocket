# nod Today

One glance: what did my agents do today?

Open it on the nod: **Home > AGENTS > TODAY**. Press `R` to refresh, `ESC` to go back.

## What you see

- **DONE**: how many times an agent said it finished.
- **NEEDS YOU**: how many times an agent asked for you.
- **APPROVED / DENIED**: your answers to approval requests.
- The agents that finished the most, with their counts.
- The last thing an agent told you.

"Nothing yet today" means no agent has reported since midnight.
"Core unreachable" means the nod could not reach your hub. Press `R` to retry.

## Where the numbers come from

Your hub (nod Core) counts the events that agents send to
[`/buddy/event`](AGENT-EVENTS.md) and the approvals you answer.

| Counter | Counted when |
|---|---|
| `done`, `needs`, `errors`, `usage` | an event of kind `done`, `needs`, `error`, `usage` arrives |
| `approved` | an approval request ends in allow (including "allow all" for a session) |
| `denied` | you deny an approval request |
| `agents` | `done` events grouped by `source`. The first 8 names to report a `done` each get a row (busiest first). Any further new names, and events with no `source`, are grouped as `other`. |

A cancelled request (the terminal prompt took over) counts as neither approved nor denied.

## Day and reset

- "Today" is the hub's local calendar day. At local midnight the numbers start again from zero.
- The numbers are kept in the hub's memory only. **Restarting the hub clears the day.**
- The nod asks the hub when you open the screen, then again every 15 s while it stays open (45 s in field mode), the same pace as agent alerts.

## For developers

`GET <hub>/buddy/today` with `Authorization: Bearer <your nod token>`:

```json
{"ok": true, "date": "2026-10-11", "done": 5, "needs": 2, "errors": 0, "usage": 1,
 "approved": 7, "denied": 1,
 "agents": [{"source": "claude", "done": 3}, {"source": "codex", "done": 2}],
 "last": {"kind": "needs", "text": "pick a branch", "source": "codex", "ts": 1760000000.0}}
```

`last` is `null` when nothing has happened today. A missing or wrong token is HTTP 401.

## What it is not

Counts only. No streaks, no points, no cost figures.
