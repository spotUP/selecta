# I211b: remote-source decisions from the source table, canonical key in sticker answers

Plan: thoughts/shared/plans/2026-10-25-m3-library.md (I211 canonical song keys).

## Changes
- source.c/.h: new `sel_source_is_remote(id)` (registered and not local/radio).
- cache.c `sel_cache_is_remote` and devilbox.c `sel_devilbox_item` use it; no modland/hvsc literals remain, hippo works.
- engine.c STICKER_GET and STICKER_LIST answers emit the canonical key, not the client string.

## Tests (fail before, pass after)
- new `key-remote` (tests/m3/key_remote_test.c): before, FAIL cache remote hippo, devilbox item hippo, hippo split, and the "modland:" edge.
- `song-key` extended (song_key.sel/.cmake): STICKER_GET/LIST with the plain path must answer SRC_KEY=local:selecta-test.mod; before, it failed (answer carried the plain path).
- Green after: song-key, key-remote, stickers, loudness*, lyrics*, cache-budget, rate*, vote-devilbox. Host and amiga builds clean.
