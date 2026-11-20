# I1473g3 mui_grid_art

Changed:
- plugins/ui/mui/map/mui_grid_art.{c,h} (new, in mui_map): open/close/get/tick/decodes. get is a mui_art_fn (cache only). tick serves at most one miss per call (first uncached, non-bad tile): reads `thumbf144` by art_id (= album id), unpack, scale to edge, cache put; else decode, fit 144, pack, `put` back, scale. Albums without a usable cover go into a 64-entry bad ring (not retried).
- The decoder is injected (`mui_grid_decode_fn`, with frame) because stb_image lives in the Amiga-only plugin, not in host-testable mui_map. Plugin passes `mui_cover_album_image`.
- plugins/ui/mui/mui_cover_image.{c,h}: split out `mui_cover_album_image(art, album, frame, &w, &h)` (the album-id variant); `mui_cover_image` now calls it.
- fit_square moved to src/gui/image/gui_image.c as `sel_gui_image_fit_square` (declared in gui_image.h). Callers: only plugins/ui/desktop/art.c (2 sites), updated.
- tests/ui/mui_canvas_test.c mode `art`; tests/CMakeLists.txt `mui-grid-art`.

Searched: fit_square (only art.c), mui_cover_image callers (mui_ui.c x3, unchanged), thumbf144 (desktop art.c, tests), sel_gui_art_cache and sel_gui_thumb_pack/unpack (reused), desktop load_album (pattern copied).

Tests (host build/host, deleted after): mui-grid-art, mui-grid-tiles, mui-grid, mui-canvas-geom, mui-art-path all PASS.
Unverified: plugins/ui/desktop/art.c and mui_cover_image.c are not built in the host tree (desktop UI OFF / Amiga only); the desktop edit is a pure rename, the cover_image split is not compiled. dui art tests not run.
Open: art_id is assumed to be the album id (row art_id); g6 must pass mui_cover_album_image and choose slots (about 24).
Deleted: build/host (worktree build tree).
