# I906g rest report (2026-11-01), second agent

Branch i906g-rest. Commits: c4c582a0 (WIP by first agent: I945 fix, I946 counter and gate, I906g STICKER_GET/LIST edit), db50b467 (I931), 4eabd6bd (I947).

Rows done: 3 of 5 (I945, I931, I947). I946 verified passing but has no revert proof. I906g not closed. Sites with a dui reachability test: 0 of 11. STICKER_GET/LIST is written, `ctest -R stickers` passes (0.55 s), no revert proof, no dui test. The other 10 sites are not started.

- I945 (c4c582a0): cause was the real ~/.config/selecta/prefs.conf (mix.stereo_width) read by tests/m3/folders_screen.cmake. Fix: isolated --config-dir plus a poisoned XDG prefs.conf. text-ui-folders-tree passes 1.08 s. Revert proof: old file makes it FAIL (0.58 s).
- I931 (db50b467): tests/dui/song_panel_art_async.cmake gates on lib_ui_wait_ms < 50 instead of pump_ms_max. Passes 9.61 s, watched_waits==0. No revert proof.
- I946 (c4c582a0): lib_ui_wait_ms counter. dui-direct-calls-async passes 75.49 s (first run hit the 300 s ctest timeout, cause not investigated, suspected contention). Gates: part1 pump_ms_max=7, turn_ms_max=0, text_reads=0, group_reads=1; every lib_ui_wait_ms gate is 0 or 1. No revert proof; the old wall-time flake cannot be reproduced on demand.
- I947 (4eabd6bd): two causes. (1) Fixture inserted albums rows that the V10 songs_album_ai trigger already creates, so the shelf had 2 tiles (lines 124, 127). (2) Since I678j the Home shelves are item cards (home_item_line in src/library/db/user.c) with no 0x1d format tag, so album, composer and playlist cards lost the platform cover. home_item_line now appends the majority format via sel_db_majority_format_cached. Test finds the item card (item_kind ALBUM+1, "Mixed") and asserts the Amiga family. Revert proofs: old user.c fails, restored album insert fails, fixed passes (0.51 s).

Gaps: I906g remaining sites (run_search callers, radio_fill, order_ensure, playlist_play, songs_unstage, drill_sample_row, sources_row, source_name_row, LIBRARY_FETCH, remote read_song), the Like-via-heart dui test, I946 and I931 revert proofs. Budget ran out.

Scratch: build/ in the worktree (gitignored, build/sdl tree kept for the next agent, size not measured, small logs b.log t1-t3.log m.log). Nothing else left. Disk at last check 3.4 GB free.
