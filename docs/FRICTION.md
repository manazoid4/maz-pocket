# nod friction audit (2026-10-07)

Measured on the installed Core (Groq + Fish keys, read-only probes) and from code. Device-side seconds are from code paths, not a stopwatch: UNPROVEN until the owner tries it.

## Flows

| Flow | Keys / steps | Seconds | Failure points |
|---|---|---|---|
| (a) Call, from Home | hold SPACE (250 ms) = 1 gesture | STT 1.1 warm (7.8 first turn, cold Whisper), LLM 0.2-1.0 Groq (6.7-14 on local fallback), Fish TTS 3.7-5.0 (all 3 latency modes the same), WAV download. First sound about 5.5-7 s, text used to show only at the end | hold-key only worked from Home or inside Call; elsewhere T then hold SPACE |
| (a) Call, from any other screen | before: ESC-hold Home, then SPACE (3 steps). now: hold Ctrl+SPACE | same | none known |
| (b) Dictate into PC | PR #43 (Right Ctrl) | not measured here | needs deps + autostart, PR #45 task |
| (c) Approve Claude prompt | PR #41 | not measured here | |
| (d) Cold start | device boot screen is 250 ms; Wi-Fi auto-retries (net.cpp); Core autostarts via PR #45 task | Core ready then first turn was +6-8 s (Whisper load), now loaded at startup | token/pairing is one-time |
| (e) Update | Wi-Fi OTA is A2's work | | cardputer-adv build failed on a fresh checkout (M5GFX 0.2.26 gone from registry), fixed in platformio.ini |
| (f) Errors | call failure shows a banner "MAZ unavailable / voice call kept in outbox" and keeps the WAV, not silent | | banner has no reason; TTS failure just stays silent |

## Real per-turn numbers

- STT whisper base.en warm: 1068, 1118 ms. Cold first call: 7821 ms.
- LLM, Groq gpt-oss-120b: 0.2-0.9 s. Groq 429 reason: llama-3.3-70b is 404 on this account (wasted hop each turn) and gpt-oss-120b has 8000 tokens/min, so bursts hit 429 and fell to local qwen (4-14 s). 9router stage has no key (fails in about 2 s).
- Fish TTS: 5046, 3710, 4582 ms; 3874, 4435, 3899 ms. It IS the bottleneck, about 4 s for a 9 s reply, no API knob helps.

## Ranked (daily pain x cheap fix)

1. LLM falls to local on Groq 429: 7-14 s replies. Fixed (PR llm).
2. Reply text waits for Fish TTS and WAV download. Fixed: text shows when the PC answers (PR bundle).
3. First turn after Core start +6-8 s (Whisper cold). Fixed: warm at startup (PR bundle).
4. Call only from Home or Call screen. Fixed: Ctrl+SPACE anywhere (PR bundle).
5. Fresh-checkout firmware build broken. Fixed (PR bundle).
6. Fish about 4 s remains. Not fixed: needs chunked or streamed TTS on device (bigger change).
7. Error banner lacks reason, TTS failure silent. Left.
