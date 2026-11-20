# I1465t text UI metadata twin

Finding: no text UI code change needed. `tui_detail` (plugins/ui/text/text_ui.c) already draws the shared `sel_uim_np_lines`
(src/uimodel/nppanel.c, same as the desktop panel) and, via `tui_stills`, the stills count and `Demo <dz_video>` line through `art->find`
/ `art->kinds` / `art->get`. `art->find` is `art_find` (src/library/db/art.c) which resolves through `sel_meta_key` (I1465a), so the text UI
is source independent already. Searched: `dz_video`, `art->find`, `sel_uim_np_lines`, `sel_meta_key`, `sel_art_dz_id` in plugins/ src/ include/.

Added test `text-ui-meta-panel`:
- tests/m3/text_meta_seed.c (helper): two songs rows, the played mod and a title+composer twin with the lower key; stills + dz_video stored
  under the twin's meta key only (helper asserts `SEL_META_TWIN`).
- tests/m3/text_meta_panel.cmake: runs `selecta --screen-dump <mod>` against that db, asserts `2 screenshots` and `Demo https://x.test/demo`.
- tests/CMakeLists.txt: registered next to the other text-ui tests (label m3).
