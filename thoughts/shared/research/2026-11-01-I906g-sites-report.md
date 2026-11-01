# I906g sites report (2026-11-01), Mac-track agent, branch i906g-sites

Sites done: 1 of 10 (playlist_play). Sites that are not real UI-thread waits: 3 (sources_row, source_name_row, drill_sample_row). Sites open: 6.

## Mechanism added
`sel_wq_op.res` (engine.c): an owned result region, allocated on the pump at queue time, filled by the writer thread, read by the done callback, freed by `wq_op_free` (collect and engine destroy). Replaces the 'x'-filled scratch string hack the sticker ops use.

## Done
- playlist_play (PLAYLIST_PLAY, VIEW_ACTIVATE on a playlist row): `wq_playlist_play_run` reads the keys on the writer thread, `wq_playlist_play_done` inserts and `queue_start`s. Test: site 7 in tests/dui/direct_calls_async.cmake (knob SELECTA_LIB_KEYS_SLOW_MS=300 on SEL_USER_PLAYLIST_KEYS, gate lib_ui_wait_ms < 50, sentinel: overlay queue >= 1 after PLAYLIST_PLAY 1). Passes with the fix. Revert proof: NOT clean. With the old synchronous function restored the test did not report a FATAL_ERROR, it ran into the 300 s ctest timeout (twice). The previous agent saw the same timeout once on unchanged code (suspected contention), so the timeout is not evidence. A direct run of the site-7 command line against a reverted build is still to do.

## Not real waits (structural evidence)
- sources_row: `cmd_SOURCES` wraps `sel_library_nowait(1)` around `sel_source_each(sources_row)`, so its library call is not a watched wait (retry through `sources_retry` in `lib_reads_collect`).
- source_name_row: iterates the in-memory source registry only, no library call.
- drill_sample_row: reached only through `drill_formats_load` <- `drill_build` <- `coll_build_run`, which runs on the `rd_coll` posted-read thread.

## Open (designs in 2026-11-01-ui-thread-library-waits.md)
- run_search callers (QUEUE_ADD_QUERY, PLAY_QUERY, VIEW_ACTIVATE then view_source_apply): plan is one wq op with a `sel_lib_job` in `res`, buffers swapped into res_text/res_meta in done (as the rd_search collect does), then the action.
- radio_fill: one wq op per pick with an in-flight guard, done inserts and re-calls radio_fill until SEL_RADIO_AHEAD.
- order_ensure: per-row `sel_library_flag_skipped`; needs a posted read or cached flags.
- songs_unstage: the free() is harmless; the wait is `sel_library_playlist_keys` in songs_open (SONGS_PLAYLIST) and `sel_engine_library_playlist_keys`; needs the verb handlers split.
- LIBRARY_FETCH and remote read_song fetch (sel_library_fetch): job-thread task plus reply event; read_song must resolve before begin_song.
- Like-via-heart dui test, STICKER dui test and revert proofs for STICKER, I931, I946: not done.

## Found
- tests/dui/playlists.cmake fails on the golden hash (got d311cba464f4ea6d, want ef91e727ce2cb2d3). The tested frame is the untouched page, so likely the pending golden rebless (I673), not this change. Unverified on main.
- A selecta process (pid 20174, 49 min old, cwd /Users/spot/Code/MusicPlayer) is not mine.

## Scratch
build/ in the worktree (build/sdl). Logs in the session scratchpad (small).
