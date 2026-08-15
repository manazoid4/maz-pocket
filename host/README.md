# MAZ Host

The Cardputer handles capture, keys, status and durable queues. MAZ Host runs speech-to-text, the local Ollama model, optional cloud fallback, deterministic PC controls and Agent Nudge access on the laptop.

```powershell
cd host
.\setup.ps1
.\run.ps1
```

`setup.ps1` creates the virtual environment and device token, then prints the address/token to enter under **Connections → C** on MAZ Pocket.

## v0.4 local brain

Preferred model:

```text
lfm2.5-8b-a1b-gpu:latest
```

The v0.4 defaults are local-first and the known old shipped `gemma3:1b` default is migrated automatically. A different model you deliberately configured is respected.

To pin it explicitly in `host/.env`:

```text
MAZ_OLLAMA_MODEL=lfm2.5-8b-a1b-gpu:latest
MAZ_DEFAULT_ROUTE=local
```

Generation uses a low temperature and a verified MAZ Pocket capability map. This is specifically intended to stop generic replies and invented app/features when COMM is asked about the device.

Keep port 8787 on a trusted private network; do not expose it through raw public port forwarding.

## Reliable device monitoring

MAZ Host includes bounded serial monitoring that can follow the Cardputer USB identity across resets and retain boot logs. Monitoring is opt-in so it cannot occupy the COM port during M5Launcher installs:

- `POST /device/monitor/start`
- `GET /device` and `GET /device/logs`
- `POST /device/monitor/stop` before installing firmware

The repository-level `scripts/accept-device.py` uses the same bounded USB transport for physical acceptance. Its `MAZOPEN`, `MAZKEY`, `MAZTYPE` and `MAZSCREEN` commands are local-serial only; they are deliberately not exposed by MAZ Host over the network.
