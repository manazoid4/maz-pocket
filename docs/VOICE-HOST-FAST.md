# nod Core — faster voice (host-only draft)

No firmware change or OTA. The handheld receives audio segments over the existing /speak part endpoint, displays the **full** answer, and speaks at most two short sentences / 220 characters. The existing SpeakPlanner synthesizes the opening sentence as its own early part and synthesizes the remaining spoken part in parallel.

## Local natural voice: optional Kokoro-ONNX

Fish stays the unchanged default, and Windows pyttsx3 remains a last resort. To opt into offline Kokoro on the Core PC:

1. On the Core's Python environment: `pip install kokoro-onnx soundfile`. This is intentionally **not** a mandatory Core dependency.
2. Obtain and verify the official model/voice files from [the upstream Kokoro-ONNX release](https://github.com/thewh1teagle/kokoro-onnx/releases/tag/model-files-v1.1): `kokoro-v1.0.onnx` and `voices-v1.0.bin`. Do not download these automatically during boot.
3. In **your local Core .env** (not the repo), set `MAZ_TTS_PROVIDER=kokoro`, `MAZ_KOKORO_MODEL_PATH=<absolute path to .onnx>`, `MAZ_KOKORO_VOICES_PATH=<absolute path to .bin>`. Optionally `MAZ_KOKORO_VOICE=am_michael` and `MAZ_KOKORO_SPEED=1.05`.
4. Restart Core; use the normal Call and /core/timings endpoints to compare LLM, TTS-first and first-audio figures. The model stays on the Windows host; never print or transmit MAZ_TOKEN.

All readable PCM16 WAVs are converted before serving to the Cardputer's native 16 kHz mono, signed 16-bit format. Unreadable/malformed responses remain a surfaced playback error, not a fabricated audio response.

**Release gates:** Compare voice naturalness with owner, measure first sound and per-stage timings, verify incremental parts play cleanly on Cardputer, assess GPU/CPU use and fallback when Core is busy. Neither latency nor hardware quality has yet been proven. No flash is needed.
