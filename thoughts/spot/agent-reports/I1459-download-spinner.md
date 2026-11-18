# I1459 cover spinner during download

Design: library owns the state. `src/library/db/art_scrape.c` sets a relaxed atomic `g_fetching` around the HTTP request in `scrape_get` (all scrape GETs run on the library job thread). Accessor `sel_library_cover_fetching` (db.h) -> `sel_engine_cover_fetching` (engine.h/engine.c). UI (`desktop_ui.c`) only reads it: helper `cover_pending(d)` = art_busy || fetching, used by `cover_draw` and the Now Playing fallback. Existing spinner reused. Dump line `cover_spinner` gained `fetching_seen=`.
Test seam: env `SELECTA_SCRAPE_HOLD_MS` (non-Amiga) sleeps inside scrape_get to hold a fetch open.

Searched: art_scrape, art_busy, cover_spinner, scan_busy accessor pattern, srcmirror atomics, net test seams (SELECTA_NET_*, SELECTA_ART_*_URL). No sleep helper or fake-fetch hook existed, hence the hold env.

Tests: `dui-cover-download-spinner` PASS (held drawn=12 fetching_seen=12; quick drawn=6 fetching_seen=1; asserts held > quick on both).
`dui-cover-slideshow` FAILED "fixture sql failed" - probably sqlite3 shell helper target not built (only --target selecta built). Unconfirmed, not rerun.

Unverified: live repaint at fetch start when UI idle; Mac visual; Amiga build of usleep guard; text UI and MUI twins not done; slideshow regression.
