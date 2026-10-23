# m3 red tests: metadata-panel and loudness-fetch

## metadata-panel (commit ea9f15f)
Cause: shared-directory pollution, not the I211 key change. The browser pane of the screen golden lists the
directory it starts in (`build/host/tests/songs`), and sibling tests (stream-decode, stickers, folders, ...) leave
a/, b/, c/, album.stamp, sd_*.wav there, so the golden depended on run order. Fail before: the serial m3 set
failed metadata-panel with the polluted listing; alone in a fresh build dir it passed.
Fix (test is wrong): tests/m3/meta_screen.cmake copies the two .mod files into `${WORK}/songs` and runs there; both
goldens regenerated (only the listing rows changed). Passes alone, after the polluting tests, and in the full set.
Note: text-ui-folders-tree and others still use the shared dir; not touched.

## loudness-fetch (commit 9419800)
Cause (product bug): `sel_engine_pump` registers `&e->net_cancel` as the calling thread's cancel flag
(`sel_net_set_cancel`, thread-local, I195a 796af02) and `sel_engine_destroy` freed the engine without clearing it.
`selecta-cli loudness` analyses on an engine on the main thread, destroys it, then does the PUT on the same thread:
the dangling flag read freed memory (non-zero), every net wait returned cancelled, and sel_http_request returned
E_CONNECT (3) before any request reached the server (fixture log showed no PUT). Fix: destroy calls
`sel_net_set_cancel(NULL)` before freeing (every pump re-registers). Found by printing the sel_http_request error code.
Fail before: `want /loudness_get=1 loudness_put=ok/ got ... loudness_put=failed`; after: passes.
Introduced by 796af02 (I195a), not I211.

## Verification
`ctest -j1 -R 'metadata-panel|loudness|home-view|user-data|stats|sticker|song-key|stream-decode|stream-tags'`: 14 of 14 pass.
`selecta-cli commands --unimplemented | wc -l` = 8.
