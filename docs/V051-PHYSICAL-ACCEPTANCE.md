# MAZ Pocket v0.5.1 — physical acceptance gate

Do not merge/release PR #11 until one real Cardputer ADV completes this gate.
The released v0.5 binary is the rollback control.

## 1. Boot / Launcher ownership

- Install `Maz-Pocket-v0.5.1-M5Launcher.bin` through M5Launcher as an app image.
- Do not repartition or flash at `0x0`.
- Confirm Home shows exactly six primary surfaces: COMM / CAPTURE / OPS / CONTROL / RECALL / FLOW.
- Press Ctrl+L and confirm hand-back to M5Launcher.
- Launch MAZ Pocket again.

## 2. Offline / no-host behavior

- Boot with the PC/MAZ Core unavailable.
- Move around Home/CONTROL/RECALL/FLOW for at least 60 seconds: input and display must never pause waiting for Host I/O.
- Record one COMM turn. The raw WAV must remain on SD/Outbox when delivery cannot complete.
- Record one CAPTURE/BrainDump. Leaving the screen while processing must not delete the WAV.
- Open OPS and refresh. It may report offline; keyboard/navigation must stay responsive.

## 3. Streaming COMM

With MAZ Core v0.5.1 running on the same LAN:

- Hold SPACE and speak naturally for 20–60 seconds.
- During capture the screen should show `WS+SD` when streaming is active.
- Release SPACE. Transcript/reply should arrive without a whole-screen network freeze.
- Confirm `CONTROL > RUNTIME`:
  - WS frames sent increased;
  - WS frame drops = 0;
  - Host result drops = 0;
  - first-token / total latency populated after an AI turn.
- Interrupt/cancel one turn with SPACE and immediately navigate elsewhere.

## 4. Failure / fallback proof

- Begin a turn, then make MAZ Core unavailable before completion.
- The device must keep the complete WAV and either use REST when possible or leave the turn queued in Outbox.
- Restore MAZ Core. Background Outbox delivery must complete without freezing the foreground screen.
- Repeat while using FLOW or RECALL so the non-blocking behavior is visible.

## 5. Local model failover

On the PC, verify both configured local roles:

- Normal policy: `MAZ_LOCAL_MODEL_POLICY=auto`.
- Primary available: provider should be `local:lfm2.5-8b-a1b-gpu:latest`.
- Make only the primary unavailable, leaving `maz-pocket-lite:latest` available.
- A LOCAL Cardputer turn must succeed as `local:maz-pocket-lite:latest` and must not contact cloud.
- Restore the primary afterwards.

## 6. 30-minute ADV soak

From a PC on the same LAN:

```powershell
python scripts/adv_soak.py --url http://mazpocket.local --minutes 30
```

During the soak actively use COMM, CAPTURE, OPS and CONTROL. Acceptance requires:

- no reboot/hang/input lock;
- WS frame drops = 0;
- Host result drops = 0;
- no excessive end-state heap loss reported by the script;
- task stack watermarks remain non-zero;
- SD remains readable and recordings remain valid WAV files.

Record the final metrics in the PR before release.

## 7. Rollback proof

- Ctrl+L back to M5Launcher.
- Install the packaged `ROLLBACK-v0.5-M5Launcher.bin`.
- Confirm the known-good v0.5 Home boots.
- Reinstall the v0.5.1 candidate through M5Launcher and confirm its Home boots again.

Only after all seven sections pass should PR #11 be marked ready/merged and v0.5.1 tagged.
