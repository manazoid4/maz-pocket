# MAZ Host

The Cardputer handles capture, keys, status and durable queues. MAZ Host runs
speech-to-text and local/cloud models on the laptop, and proxies the loopback
Agent Nudge daemon without exposing its credential to Wi-Fi.

```powershell
cd host
.\setup.ps1
.\run.ps1
```

`setup.ps1` creates the virtual environment and a random device token, then
prints the address and token to enter under **Connections → C** on MAZ Pocket.
Edit `.env` to choose Ollama/cloud models or connect Agent Nudge. Keep port
8787 on a trusted private network; do not expose it to the public internet.

## Reliable device monitoring

MAZ Host includes an original, narrow implementation of the useful embedded
lesson from `terminal_mcp`: a bounded serial session can follow the Cardputer's
USB identity across resets and retain boot logs. It deliberately does not copy
that project's large multi-protocol server or conflicting licence metadata.
Monitoring is opt-in so it cannot occupy the COM port during M5Launcher installs:

- `POST /device/monitor/start`
- `GET /device` and `GET /device/logs`
- `POST /device/monitor/stop` before installing firmware

The repository-level `scripts/accept-device.py` uses the same bounded USB
transport to exercise all eight product surfaces on physical hardware. Its
`MAZOPEN`, `MAZKEY`, `MAZTYPE` and `MAZSCREEN` commands are local-serial only;
they are deliberately not exposed by MAZ Host over the network.
