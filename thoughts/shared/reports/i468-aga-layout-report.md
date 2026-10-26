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


## I521 dui-keys

Cause: apps/selecta/main.c ui_resolve() stored the caller's `id` pointer in shell_ui.id. For a UI_SWITCH the caller passes
`want[16]`, a stack buffer inside the wait loop; after the switch the slot's id dangled, so the exit `ui_stats id=<garbage>`
line no longer read `id=desktop` and dui-keys (keys.cmake:94, dui_stat refired) found no `ui_stats id=desktop`. Not caused by the
tall-pixel commits (no change in apps/selecta); the stack layout change exposed an old bug. Fix: ui_resolve keeps the registry's
own SEL_PLUGIN_ID string. dui-keys is the regression test: fails with the fix reverted (checked, rebuilt selecta), passes with it
(11 of 11 runs after the fix; one run early on ended in a 120 s Timeout that did not recur, unverified cause).
Result: `ctest -L dui -LE dui-red` 56 of 56 in build/host (reconfigured with SELECTA_UI_DESKTOP=ON; it was OFF).
Open: AGA cross build and rig check (disk).

## I468 rig check

Run 2026-10-26 on rig 2 (FS-UAE A1200, 68020 `--a1200`, Workbench on native AGA via `rig.py aga`, window title purpose "MusicPlayer AGA agent: 640x256 desktop layout check"). Build: `build/amiga-aga` from b0ed7ec, `-DSEL_DESKTOP_AGA=ON` with the host gui_iconbake and gui_fontbake, 746 steps, no errors. Rig stopped afterwards.

Answers (screen 640x256, Selecta 8-bitplane AGA screen, ui_logical=1024x819, ydiv 2):
- Play button round: YES (circle at the transport centre, seen in both screenshots).
- Left rail visible: YES (Selecta header and seven entries Home to Song Information; stats line `rail 0 0 125 467`).
- Statistics strip clipped: NO (Library page, `--view library`: Statistics, Songs in library, Total plays, Total play time and the More control all inside the main region).
- Glyphs missing: NO (`missing_glyphs=0` on Home and Library; `glyphs=79` on Home).

Evidence: screenshots /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/cbd1c5e4-7b39-4744-b423-20042bbd174d/scratchpad/i468-rig-aga3.png (Home) and /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/cbd1c5e4-7b39-4744-b423-20042bbd174d/scratchpad/i468-rig-lib.png (Library), stats /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/cbd1c5e4-7b39-4744-b423-20042bbd174d/scratchpad/i468-rig-home.out and /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/cbd1c5e4-7b39-4744-b423-20042bbd174d/scratchpad/i468-rig-lib.out. Home first frame took 9.4 s on the 020 (present_ms 9028, paint_ms 380); atlas_baked_hits=5, atlas_builds=0.

Defects found (not fixed):
1. `--backend aga` alone does not open. `sel_uim_budget_for` (plugins/ui/desktop/desktop_ui.c:8233) takes the format from `--format` only, so the default is rgba32; `desk_aga_open` (plugins/ui/desktop/amiga/backend_aga.c:~137) then refuses with "the AGA screen shows an INDEX8 canvas only" on stdout (not the --log file) and the shell prints "the UI could not open". It works with `--format index8`. Root fix: default the budget format to index8 when the backend is aga (the rtg line in RULES.md has no such flag either). Not fixed here: the AGA branch is compiled only in the Amiga build, so a regression test needs the vamos/rig path; left for a row.
2. The default 16 KiB shell stack overflows (`amiga_start: stack overflow, canary`): the run needs `Stack 262144` first. RULES.md run line does not say so.
3. The backend open failure message goes to stdout, so `--log` (stderr only) hides the reason.
