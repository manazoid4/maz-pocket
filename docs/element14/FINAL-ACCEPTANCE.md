# Final candidate acceptance

Status: **BLOCKED — Cardputer ADV not currently enumerated over USB.**

## Candidate identity

- Branch: `agents/element14-competition`
- Current implementation commit: `21484a8` plus the pending documentation commit
- Firmware version: `0.8.0` (competition candidate; no release tag yet)
- Local build SHA-256 before final documentation: `26543D4DE9530F75C12F27C0EB4A9D305C56C49BA6FBC4F30A00FC10FC55D03E`
- Build result: PASS, 60,196 bytes RAM and 1,454,673 bytes flash
- Host tests: PASS, 85 tests before the two installer tests were added

The hash must be refreshed after the final code commit/build. It is not physical-install evidence.

## Required physical gate

Connect the identified Cardputer ADV by data-capable USB. Run the guarded installer on the exact final firmware. Record the observed boot banner, commit and binary hash without exposing credentials.

Run each flow three consecutive times:

| Flow | Run 1 | Run 2 | Run 3 | Evidence required |
|---|---|---|---|---|
| CALL real human voice → transcription → actual provider → device answer | — | — | — | transcript, provider, displayed result; speaker only if heard |
| Brain Dump → Core → structured result | — | — | — | preserved capture/result IDs without private content |
| PLAN with Core online and offline | — | — | — | responsive opening, result or truthful unavailable state, loop timing |
| Agent STATUS | — | — | — | live source/timestamp or truthful unavailable state |
| YouTube proposal → Enter → PC launch → device receipt | — | — | — | exact fixed homepage; no arbitrary URL |
| Notepad proposal → Enter → new file → PC launch → device receipt | — | — | — | synthetic text; unique new file; no overwrite |
| Cancel and replay | — | — | — | Esc performs nothing; repeated confirmation launches once |
| Reboot / reconnect / return to M5Launcher | — | — | — | data and settings preserved; actual launcher evidence |

Stop and record the exact failure if any flow changes stored Wi-Fi, SD, captures, existing files, partitions outside the MAZ app slot, or M5Launcher state unexpectedly.

