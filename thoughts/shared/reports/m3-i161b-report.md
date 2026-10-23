---
date: 2026-10-23
topic: I161b modland_sync.c defects
status: done
---

# I161b report

1. Indented changelog: `apply_changelog` trims leading blanks of the key before comparing.
2. Move keeps its data: new request `SEL_LIB_REQ_MOVE` (`sel_move`, `sel_library_move` in `src/library/db/scan.c`) re-keys the songs row, the files row and every key-bearing table (history, liked, kept, ratings, cues, notes, flags, stickers, genre_cache, pending_push, playlist_entries) in one transaction. md5, size, duration, year, added survive; name/artist/format come from the new path. A `from` the catalogue never had is added at the destination as before. A move no longer counts into `cl_removed`.
3. Listing parser: `split_listing_line` already accepted `<size>\t<path>`; no code change was needed. The test now pins it (Part 3 of `tests/m3/source_modland.sh`, listing rewritten to size form, golden unchanged).

Tests: `tests/m3/source_modland.sh` plus fixture `tests/m3/modland_changelogs/changelog_2026-10-21.txt` (indented added x2, indented moved/to of `Ace Tracker/505/blocks.am`). Counts now use `^ *key:`.

Before/after (script run directly):
- Whole modland_sync.c reverted: FAIL new_this_week != 6 (4 instead of 6; indented lines ignored).
- Only apply_move reverted to remove+add: FAIL moved row lost its md5.
- Fixed: `source_modland_ok listing_rows=100 new_this_week=6 db_songs=107`; ctest `modland|source` minus vamos|rig: 9 of 9 passed.
- Defect 3 has no failing-before case, the parser was already correct.

Leftover: the `ps | grep vitest` wait loop self-matches other sessions' wrapper commands, so it was replaced by `pgrep -x ctest`.
