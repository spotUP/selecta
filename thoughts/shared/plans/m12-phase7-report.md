# M1.2 Phase 7 report (rows I82, I83)

Red evidence (commit 5761822), `ctest -R selecta-m12-reach`:
`1/1 Test #51: selecta-m12-reach ...***Failed` with `baseline run failed (0)` (the shell printed no `cmdline_runs=... bindings=` line yet).

Green: counters `3 1 <default+1> 3 1 1 tree` and the golden screen match; ratchet `commands --unimplemented | wc -l` = 27; `cmake --build build/amiga -j4` green; selecta-* and unit tests still pass.

Product wiring: UI stat tags CMDLINE_RUNS, PALETTE_OPENS, BINDINGS (text UI counts submitted command lines, palette openings, table size); `sel_engine_stats` gains undo_depth and redo_depth from the history; `sel_view_style_label`; the shell prints the new exit line (playlist_entries = queue_count).

Deviations
- reach.cmake runs a baseline `q`-only shell to measure the default binding count, then asserts B = default + 1 (the plan gives no absolute number).
- reach.keys adds `Tab` (show the queue view) and `Enter` (expand the first folder) after the palette closes; without them the last screen is the browse view. Plan key list did not include them.
- Playlist entries are relative, so the tree shows `tests/m12/a` (relative folders), not absolute. Titles come from file names (`one.mod - mod`): the queue load reads no tag metadata, so `%title% - %format%` renders filename and extension class.
- The title format is set through `prefs.conf` in a `--config-dir` written by the test script.
- Worktree needed copies of third_party/nuade and libopenmpt from main to configure (not committed).

Open questions: should PLAYLIST_LOAD or VIEW_STYLE bring the queue view forward (removing the Tab)? Should loaded entries read metadata so titles are real?
