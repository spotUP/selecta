# I170 report: history, play and skip counts, LIKE, KEEP, per-song rp.config in the DB

## Done
- New worker op SEL_LIB_REQ_USER (src/library/db/user.c): history row, liked, kept, stickers.
- Engine writes one history row per song: natural end (SONG_ENDED paths, gapless/crossfade commit, REPEAT one round) = skipped 0; a user OPEN/NEXT/PREV replacing a playing song = skipped 1. Counter `history_writes` in sel_engine_stats, printed by `@library_stats`.
- New commands LIKE and KEEP act on the song being played.
- Query field `skips` added next to `plays`.
- Per-song rp.config now lives in the DB as sticker `rp.config` of the song key, read at OPEN. `sel_engine_set_song_config` is removed; `render --rp-config-song` stores the value in a throwaway in-memory library.
- Reachability test `user-data` (tests/m3/userdata.sel): drives selecta-cli script, plays one song to its end, replaces another mid-song, LIKE; asserts history_writes=2 (sentinel), plays:1, skips:1, liked:yes.

## Not done
- `smart-downloads` still seeds history with `@mark history:`. Real plays need the modland songs fetched and played to their end inside that network-mock test; not attempted within budget. The `history` branch of `sel_library_mark` therefore stays.
- KEEP has no reachability assertion (no query field for kept).
