# I1465m - MUI source-independent metadata panel

Design: thoughts/shared/designs/2026-11-20-i1465-i1468-source-independent-metadata.md sections 5, 6, 7.

## Searched
sel_uim_np_lines (nppanel.h), sel_uim_si_* (siinfo.h), csdb.h, text_ui.c tui_stills/tui_detail, mui_cover_tick, mui_detail_refresh, user.c (meta key, meta_twin, hvsc_twin), deepsid_info.c (youtube_rows), schema.c.
Finding: MUI already renders the engine panel rows and rotates covers through art->find(path), which is meta-key aware (engine rows I1465a-d, I1468a on main). No logic re-implemented. Real gaps: video link line, row truncation, click-to-open.

## Changes
- include/selecta/uimodel/siinfo.h, src/uimodel/siinfo.c: new shared sel_uim_si_url(value, out, cap) reads the http(s) URL out of a row value.
- plugins/ui/mui/mui_ui.c: info_buf 192 bytes per row and panel val 176 (YouTube capture rows no longer cut before the video id); "Demo video: %s" row from artwork kind dz_video (fetched in mui_cover_tick on song change, forces detail rebuild); click on a URL row opens it via mui_open_url.
- tests/ui/mui_contract_test.c: test_metadata_panel, mode metadata-panel (source checks, sel_uim_si_url unit checks, sel_uim_np_lines keeps the YouTube capture pair for a Modland path).
- tests/CMakeLists.txt: mui-metadata-panel next to mui-cover-image.
- plugins/ui/mui/map untouched.

## Tests
NOT RUN. The worktree host build configure sat in the buildq light lane for 15+ minutes behind other agents' jobs (see buildq --status). Neither build nor ctest ran; the code is uncompiled.
Run: nice -n 19 ctest -j2 -R 'mui-metadata-panel|mui-cover-image|mui-songinfo-panel|mui-open-url|uim-nppanel'
mui-cover-image reportedly failed before at mui_contract_test.c:3460 (likely pre-existing, unverified).

## 68k cost note (query plan from schema, no rig run, time estimate unmeasured)
- meta_twin (user.c) scans the whole songs table on lower(replace(name,'_',' ')): an expression, not sargable, no source filter. hvsc_twin scans the hvsc rows (~61k). Both run once per song; the result is cached in song_meta_key.
- A (source, name) index does not help either query. Recommended: expression index songs_meta_name ON songs(lower(replace(name,'_',' '))), needs a schema bump.

## Open points
- Compile and run the tests (above).
- Expression index decision (schema bump).
- Cover rotation unchanged on purpose; checked by source string only, no rig.
