# Fish Audio voices: Sarah + your favourite

This is a **host-only** change; no firmware OTA required. Fish credentials remain on the PC, never on nod or in source control.

## On your phone (when reachable to nod Core)

1. Connect your phone to the same Wi-Fi as the Core PC, or use your already configured secure remote access.
2. Open the Core **/control** page, log in using your normal phone-control procedure, then select **VOICE**.
3. Under **Your Two Voices**, Sarah is always available. Paste a public Fish Audio voice model URL (for example, `https://fish.audio/m/<32-hex-id>/`) or its 32-character model ID, and press **SAVE & USE**.
4. Use **PLAY** to preview each voice. Switch between Sarah and your saved custom favourite with **USE**. The custom favourite survives switching voices and restarting Core.
5. The wider Fish Library search continues to work. Choosing another **custom** voice replaces the single custom favourite; selecting Sarah or a curated voice does not.

Selections affect subsequent Fish TTS replies. The `MAZ_TTS_PROVIDER` must be `fish`, TTS enabled and a Fish API key configured on the Core. Private/unavailable IDs can fail to synthesise: no API key should be pasted into the voice box.

## More natural voice and quicker first sound

- Fish S2.1 Pro's chosen voice should be used rather than the last-resort robotic Windows TTS; verify the actual `X-TTS-Provider` and timings rather than assuming the provider.
- Keep the spoken answer short; Core already synthesises early reply parts concurrently and keeps full text on-screen.
- Fish supports an explicit `reference_id` per voice. Fish audio streaming modes exist, but the existing synchronous `/speak` path cannot truthfully claim streaming to the Cardputer.
- Re-test on the actual Cardputer after Core updates. No hardware result is implied by green CI.
