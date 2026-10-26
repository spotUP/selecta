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

## Final state (2026-10-26)
Design doc: thoughts/shared/plans/2026-10-24-desktop-ui.md Q14 (640x256 is the design target, fully laid out, goldens at
640x256), Q2 (AGA 640x512 unit) and section 2.11 (68020 and AGA constraints).

Commits: 0386628 (canvas ydiv), 54a4b2b (all primitives, baked fonts, icons, desktop layout in square pixels at ydiv 2,
mouse y doubled, `--ydiv N` option; aga backend picks ydiv 2 at height <= 300), b603359 (test dui-tall-pixels + golden).

Done: open items 1-8 of the earlier list. The play button is square in layout space and measures width = 2 * rows on
screen (14 columns x 7 rows walked from its centre; with ydiv forced to 1 the same test fails: 14x20). I457: the sidebar
region exists at 640x256, the Statistics strip lies inside the 512-row layout, missing_glyphs=0. ydiv 1 goldens are
byte-identical. `ctest -L dui -LE dui-red`: 55 of 56 pass.

Open:
- dui-keys fails (the `all` run: no `ui_stats id=desktop` after the UI switch closes the desktop). Cause unproven;
  my diff touches pointer routing (KEY events are copied unchanged), canvas creation, fonts and icons. Needs a bisect
  against 0386628~1 (d63e790).
- AGA cross build (`-DSEL_DESKTOP_AGA=ON`, build/amiga-aga) and the rig 640x256 check NOT run: `df -h /` showed 2.0 GiB
  free, the build needs about 3 GiB. Question for the rig: is the play button round, does the left rail show.
- Merge risk: another agent edits desktop_ui.c (Sources page); conflicts likely near desk_font, desk_make_canvas,
  desk_build_icons, desk_route_input and the desk_open options.

