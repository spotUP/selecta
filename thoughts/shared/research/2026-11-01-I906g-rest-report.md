# I906g rest report (2026-11-01), stopped on ENOSPC

Branch i906g-rest. NOTHING COMMITTED: the disk filled up and Bash could not start, so no commit was possible. All edits are uncommitted in the worktree.

Rows: 0 of 5 closed. I906g sites: 0 of 11 closed (STICKER_GET/LIST code written, builds, sticker tests not run).

- I931: art test already has SLOW_MS=300 and the watched_waits==0 gate (53d10d90). Run showed pump_ms_max=59 with lib_ui_watched_waits=0 and lib_ui_wait_ms=0 (loaded machine, wall time). Rerun not done. Consider gating on lib_ui_wait_ms as in I946.
- I945: root cause: tests/m3/folders_screen.cmake read the real ~/.config/selecta/prefs.conf (mix.stereo_width = 99). Fix (isolated config dir, poisoned prefs as regression) written; ctest text-ui-folders-tree passes. Revert-check not done.
- I946: lib_ui_wait_ms (worker.c, db.h, main.c, direct_calls_async.cmake) built. dui-direct-calls-async not run.
- I947: engine-majority-format fails: lines 124, 127 (sel_library_collection shelf: sc.n==1, name "Tied", fmt Amiga) and 141 (Home tile "Mixed" not seen). Cause not found. Shelf and home format output are what fail; items list (section 1) passes.
- I906g STICKER_GET/LIST: wq-based edit in src/engine/engine.c, compiles. Untested.
