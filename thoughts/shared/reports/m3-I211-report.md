# I211: one song key everywhere

## Decision
Canonical key = source-prefixed `<source id>:<path>` (design doc section 9, song key). A local song is `local:<path>`.
`sel_song_key_of(ref, out, n)` (src/library/song_key.h, defined in source.c) derives it: a `scheme://` URL stays,
a string starting with a registered source id plus `:` stays, anything else gets `local:`. Idempotent.

Why this one: `songs.key`, the stats views, the home view, query.c and radio all join on the prefixed key. History,
likes, ratings, stickers etc. written with the raw path never joined, so stats and home silently ignored them.

## Changes
- engine.c: song_key() wraps sel_song_key_of; history, loudness, stickers, vote, ANALYSE, SHARE use it. begin_song strips `local:` so a library key opens.
- db/user.c: sel_library_user canonicalises the key of every per-song op (history, liked, kept, ratings, cues, notes, flags, stickers, loudness, tag edit).
- radio.c seed canonicalised; session.c and analyse.c use the shared function.
- schema.c: migration v5 rewrites raw keys to `local:<key>` in history, pending_push, liked, kept, ratings, cues, notes, flags, stickers, playlist_entries. Idempotent; on a PK collision the prefixed row wins. Frozen source id list: local, modland, hvsc, hippo.

## Tests
- `song-key` (new, m3): drives analyse + script through selecta-cli, asserts songs.key, history.key and sticker key identical (`local:selecta-test.mod`) and that history joins a song with a loudness gain. Part 2 migrates a v4 DB with raw rows (collision, AmigaDOS path, URL, modland key, idempotency, stats_plays sees the plays).
- Fail-before (engine.c and user.c at HEAD): `history key: got 'selecta-test.mod', want 'local:selecta-test.mod'`. user-data, home-view and metadata-panel also failed at HEAD with the fix reverted.
- New fixture `catalogue_keys.tsv` (local: keys) for user-data, home-view, metadata-panel. Updated expectations: library_db_open (schema 5), home_view, meta_screen, userdata.sh/.expected, userflags.expected, sounds-wrong.golden.
- Passing: song-key, user-data, home-view, metadata-panel, library-db-open, stats, stickers, share, radio, rate/vote, queue-history, library-search, play-query, cues, loudness-analysis. `commands --unimplemented` = 8.

## Found
- loudness-fetch fails (`loudness_put=failed`) at HEAD too; not caused by I211.
- Fixture catalogue.tsv uses raw local keys, unlike real scans.
- m3 tests pollute build/host/tests/songs (userdata_notes.db, st_dir, st.db, sounds-wrong.txt); metadata-panel golden breaks in a parallel full run.
- devilbox.c sel_devilbox_item and cache.c sel_cache_is_remote hard-code modland/hvsc, not hippo.
- Sticker answer events echo the client key, not the canonical one.
- song-key takes about 31 s on first run (a CLI step waits; not investigated).
- Not done: build/amiga not built.
