# nod audio tuning — Cardputer ADV and future Kickstarter hardware

**Status:** Host DSP implemented and testable; acoustic quality is *not* yet proven on a physical unit. Hardware listening and release QA are mandatory before claiming improved sound.

## Current signal chain

Fish Sarah or custom Fish voice (or Kokoro/Windows fallback) -> nod Core PCM WAV -> native 16 kHz mono PCM16 -> `shape_speech_wav` (profile) -> /speak -> device stores WAV -> M5Unified ES8311 codec -> NS4150B amp -> Cardputer ADV 1 W speaker.

The Cardputer ADV also supports a 3.5 mm audio jack; it bypasses/mutes the speaker in hardware when a plug is inserted. It does **not** have a jack-detection software interface in nod.

### Changes

- Default `cardputer_adv` applies a gentle **165 Hz high-pass**, **5.7 kHz low-pass**, **-1 dB fixed trim**, peak ceiling **86% full scale**, and **3 ms click protection** at segment boundaries. This is an *engineering baseline*, not measured EQ. Sound may be better, worse, or quieter depending on the enclosure and room.
- `flat` is byte-for-byte DSP bypass (still delivers native WAV). A/B compare immediately.
- `headphones` has a broader spectrum and separate peak/headroom; use **lower device volume** with the jack.
- `prototype_small` and `prototype_large` are provisional starting profiles for *future hardware*, not a finished Kickstarter driver or factory calibration.
- Core voice caches and Fish preview caches are profile-specific so switching cannot serve stale audio.
- Firmware removes the hardcoded minimum voice volume **64/255**; device playback uses the stored user level, including zero. No ES8311 register hacks or new pins.
- Both phone-control and bearer-protected API expose `GET /voices` (returns `audio_profile`) and `POST /voices/profile` with JSON `{"profile":"flat"}`. Phone **Control → Voice → Output Sound** offers one-tap comparisons. The saved choice persists in the existing Core `settings.json`; bearer tokens are never returned.

## Quick A/B test without entering PC

After updating Core to include this code, open `http://<your-Core-LAN-IP>:8787/control/` from your phone on the same trusted Wi-Fi, authenticate via your existing Control page, choose **Voice → Output Sound**. Start with Cardputer ADV, play a short Fish/Sarah reply on nod at a comfortable volume. Switch to Flat, repeat the exact same sentence, then compare. Say whether the difference is *clearer, thinner, quieter, harsher or distorted*. For a meaningful comparison use the same voice, words and volume, plus a second trial via the Cardputer's headphone jack at **low volume**.

If the **Windows fallback** is actually talking, this tuning cannot create the naturalness of Fish: check provider via `X-TTS-Provider` or Core logs and verify Fish API configuration on the host, without revealing secrets.

## Local configuration and rollout

`MAZ_AUDIO_OUTPUT_PROFILE=cardputer_adv` (default) or `flat`, `headphones`, `prototype_small`, `prototype_large` in local Core config. A choice made on the phone overrides this and is stored in Core settings. No firmware flash is required for host EQ alone, but **removing the playback volume floor requires an explicit Launcher OTA**, after build, manual checks and owner approval.

Use a clean deploy worktree, never the dirty Windows repository. Follow normal CI; test the new device firmware on real hardware before merging/deploying. Do not merge legacy approval PRs or alter the token model.

## Kickstarter hardware interface (required before production)

The future PCB may have a different I²S codec, amplifier, speaker, enclosure and thermal behaviour. The present firmware still drives `M5.Speaker` and the ADV ES8311; it **is not yet a generic Kickstarter firmware HAL**.

Before shipping: split firmware capture/playback into a per-board audio backend (`begin/capture/play/volume/end`), provide a signed/provisioned *hardware ID* selecting the appropriate host DSP profile (do not trust an unverified HTTP header to choose it), set sample rates and amplifier headroom per board, then instrument latency, dropouts, noise, volume and clipping. Run standardized voice clips and near-field recordings at multiple volumes and with/without the final 3D-printed enclosure. Tune response/EQ using those measurements, not guessed frequencies; test continuous playback, I²S transitions after microphone use, low battery, charging noise, and headphone safety. Validate BOM, power and acoustic variations across production units.

**Rollback:** set Output Sound = Flat; for firmware rollback use M5Launcher A/B recovery with explicit owner approval. No partition flashing through ArduinoOTA.
