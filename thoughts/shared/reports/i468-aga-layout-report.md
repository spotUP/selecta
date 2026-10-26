# I468 / I457 AGA 640x256 layout: status report (2026-10-26)

Result: I468 is started, not done. I457 is open. The primitive layer is in; the rest is open.

## Design (Johan's decision, I468)
Lay out in a 640x512 square-pixel space, draw y at half scale. Mechanism: a vertical pixel aspect on the canvas
(`sel_gui_canvas.ydiv`; 0 or 1 = square, 2 = pixels twice as tall as wide). Callers keep working in layout space;
the rasterizer entry points halve y and h. This extends the existing primitive layer instead of adding a second scale
mechanism to desktop_ui.c, where 384 `lpx()` call sites use one scale for both axes.

## Done
- `ydiv` on the canvas; honoured by `sel_gui_fill_rect`, `sel_gui_round_rect` (corners become ellipses, 32-bit
  integer coverage, `k` = row height in columns), `sel_gui_clip_push`, `sel_gui_dirty_add`, `sel_gui_blit_mask`
  (mask drawn as baked, at the halved y).
- Unit test `test_tall_pixels` in tests/dui/draw_unit.c (run by `dui-draw`): a 40x40 layout circle measures 40 wide
  and 20 drawn rows on both canvas formats; layout y 8 maps to row 4; fill and clip map. Revert check: with `ydv()`
  forced to 1 the test reports 6 failed checks; restored, it passes.

## Open (all needed before I468 closes)
1. Remaining primitives with ydiv: gradient, line, scrim, round_frame, blit_image(_round), shadows, genart, icons.
2. `round_cov` (frame, image-round, shadow) is still circular.
3. Text: atlas builder (plugins/ui/desktop/atlas_build.c, src/gui/text) needs separate x/y scale; `tools/gui_fontbake.c`
   must bake Inter at half height and `desk_font()` pick them; text metrics in layout space vs drawn.
4. Icons: `gui_iconbake` half-height bakes and the icon draw sites.
5. desktop_ui.c: choose ydiv=2 and a 640x512 layout size for backend aga at height <= 300 (canvas 640x256 vs layout
   640x512); mouse y doubled in the input path; present/dirty mapping.
6. I457: with the 640x512 layout space the full layout applies, so the left rail should appear; re-measure the
   Statistics clipping after item 5.
7. Golden at 640x256 through `selecta --ui desktop --backend mem --canvas 640x256` with a round play button measured
   (width == 2 * rows); AGA build (`-DSEL_DESKTOP_AGA=ON`, build/amiga-aga) and rig 640x256 check.
8. One reachability test through the shell proving ydiv reached the draw (sentinel).

## Not done in this run
No Amiga build and no rig run: nothing in the shell sets ydiv yet, so a rig check would show the old layout.
The fresh worktree had no submodules; I ran `git submodule update --init --reference <main checkout>` for
third_party/nuade and third_party/libopenmpt.
