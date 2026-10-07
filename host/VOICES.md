# Reply voices (Fish Audio, free model only)

Researched 2026-10-07 (docs.fish.audio, fish.audio blog, live API).

- List/search: `GET https://api.fish.audio/model` with `title`, `language=en`,
  `sort_by` (score | task_count | created_at), `page_size` (1-100), `page_number`,
  `tag`, `licensed`, `self`. Auth `Authorization: Bearer <key>`. (The endpoint also
  answered an unauthenticated call when checked; Core still sends the key.)
  Item fields: `_id` (= `reference_id`), `title`, `description`, `tags`, `languages`,
  `visibility`, `type` (tts), `samples`, `task_count`. Results window is ~1000 deep.
- Free model: `s2.1-pro-free` is $0 under a Fair Use Policy, no SLA/latency
  guarantee. It works with Fish's public library voices via `reference_id`;
  cloned/custom voices are not supported. No numeric rate limit is published
  (they reserve the right to throttle abusive patterns), so Core caches
  previews on disk and search results for 10 minutes.
- Core never uses a paid model: `MAZ_TTS_MODEL` stays `s2.1-pro-free`.

## Curated list (each id confirmed present, public, type=tts, en in the live API)

Adrian `bf322df2096a46f18c579d0baa36f41d` (default), Sarah `933563129e564b19a115bedd57b7406a`,
Ethan `536d3a5e000945adb7038665781a4aca`, Selene `b347db033a6549378b48d00acb0d06cd`,
Slax `c5f56a6cc2ec4fa8920cb4c5889a3fb7`, ELITE `d8a1340984ee4b63ad1ffae27a6a4339`,
Verity `711cf3ed00ab441a8f54a45058047b7a`, Energetic Male `802e3bc2b27e49c2995d23ef70e6ac89`.
Voices that imitate real people were deliberately left out. Audio quality with
the free model was not auditioned here (no key in the build sandbox).

## Core endpoints (Bearer token; phone page uses the same under `/control/api/...`)

- `GET /voices` curated list + current
- `POST /voices/select {reference_id}` persisted in `host/data/settings.json` (or `MAZ_DATA_DIR`)
- `GET /voices/search?q=` Fish search proxy (needs `MAZ_FISH_API_KEY`), cached 10 min
- `POST /voices/preview {reference_id}` WAV of "Hi, I'm nod.", cached in `data/voice-previews/`

An id Fish rejects (400/404/422) is logged once and the selection reverts to Adrian.
