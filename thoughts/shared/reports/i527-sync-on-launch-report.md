# I527 sync on launch banner

Engine: meta key `sync.<id>` holds the last successful sync time (written by LIBRARY_SYNC on success). SOURCES rows carry SRC_SYNCED and SRC_DUE; due is computed by `sel_source_sync_due` (include/selecta/source.h) from the sync time, the pref `library.sync_notice_at` and `library.sync_stale_days` (default 30, 0 = off).

Desktop UI: banner under the Library heading, one row per due source with a checkbox (default ticked), plus Sync and Not now. Sync sends LIBRARY_SYNC per ticked source and then dismisses, so unticked sources wait until the next stale point too. Not now persists the dismissal time.

Findings:
- The launch SOURCES request had to move into `desk_wait_turn`: sent earlier, the engine did not yet answer UI events.
- `push_timed_events` (backend_sdl.c) truncated the TIMED script at 255 characters, which dropped the final `q@` quit token and hung long scripted runs. Buffer is now 2048. `push_test_events` still has `buf[256]`.
- The banner shifts the Library layout once the SOURCES reply arrives, and the fixtures hold never-synced remote sources. Eight existing dui tests broke. Root-cause handling: `dui_run.cmake` seeds `library.sync_stale_days=0` in the test config unless a test sets DUI_SYNC_BANNER (sync_banner, library_sources, tall_pixels keep it on); song_menu_live writes it in its own prefs.conf. The SOURCES reply now repaints only on the Library page and only when the row changed (dui-dirty measured a full redraw on a clock tick otherwise).
- Goldens regenerated (banner on): library-sources-rgba32-1280x800@1, library-rgba32-640x256-ydiv2@1.
- Full ctest -LE dui-red: 310 of 315 pass. Failing: pattern-cells (nuade output format differs), home-view (home_rows 14, want 13), three rig-m4 tests (need the Amiga rig). None touch this change; not verified against main.
