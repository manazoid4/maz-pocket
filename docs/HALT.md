# nod Pause Asks / Halt (Fn+H)

**Pause Asks** temporarily denies new agent permission requests that are routed through nod Core. It also denies unresolved queued requests. It **does not stop processes that are already running, revoke permissions granted elsewhere, or stop agent tools that bypass nod**. Never market it as a kill switch.

- Press Fn+H while paired with Core: POST immediately to /buddy/halt, not after the background 15/45-second poll. The status bar shows HALT when confirmed.
- Press Fn+H again: end the pause; future sensitive requests still require individual explicit approval. No session-wide or blanket approvals exist.
- If Core is unreachable or the state cannot be determined, Fn+H only attempts the safe action (pause). The device reports a failed send and never pretends the state changed.
- Pause defaults to 30 minutes, maximum 480 minutes; Halt lives only in Core memory: restarting Core clears it. It only denies pending and new approval requests and cannot stop processes that are already running.
- An approval already consumed by the hook cannot be undone. Halt cannot make it safe after the fact.

This is a prototype until tested on the actual Cardputer.