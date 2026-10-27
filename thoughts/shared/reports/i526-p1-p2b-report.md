# I526 P1, P2, P2b report

Branch: `worktree-agent-aef30ca469ddcc5ed`. Worktree: `/Users/spot/Code/MusicPlayer/.claude/worktrees/agent-aef30ca469ddcc5ed`.

## Commits

- `e2ce3b7` Bake Amiga bitmap fonts and the SemiBold-only tall Inter set (I526 P1, P2). P1 and P2 share the tool rewrite and CMake edits, so one commit.
- `619ff6a` Reserve palette entries 24-29 for headline shades on INDEX8 (I526 P2b).

## What changed

- `plugins/ui/desktop/amiga_font.c`: reader for Amiga disk fonts (`desk_atlas_build_amiga`, `desk_amiga_font_ysize`). Layout in ydiv 2 units: ascent = (Baseline+1)*2, line_height = YSize*2, missing glyph = extra CharLoc entry.
- `atlas_build.h`: families `DESK_FAMILY_INTER_REGULAR=0, INTER_SEMIBOLD=1, MINI4, MINI6, MINI8`; `desk_baked_font.family` replaces `.semibold`; `DESK_FONT_BAKE_TALL_PX 22, 32`.
- `tools/gui_fontbake.c`: one entry list. Inter both weights at ydiv 1; Inter SemiBold only at ydiv 2 px 22 and 32; Amiga fonts at ydiv 2, size = YSize (mini4 6, mini6 8, mini8 8).
- Vendored fonts in `third_party/fonts/amiga-mini/`.
- `src/gui/draw/gui_draw.c` and `gui_draw.h`: `sel_gui_palette_headline_shades`, `SEL_GUI_HEADLINE_SHADE0 = 24`. Called from `desk_make_canvas` after `sel_gui_palette_budget` for TEXT on BACKGROUND and TEXT on SURFACE (assumed headline pairs, unverified until P4 maps roles).

## Tests and revert checks

- `dui-amiga-font` (new): metrics plus hand-decoded 'A' and 0xE4 rows for mini8/8, mini6/8, mini4/6 (decoded independently with python). Revert: row stride `modulo - 8` gave bad=6.
- `dui-font-baked` (extended): no Inter ydiv 2 atlas except SemiBold 22 and 32; 'L' ink 11-12 rows at px 32, 8-9 at px 22. Revert: `DESK_FONT_BAKE_TALL_PX 22, 24` gave bad=2.
- `dui-draw` unit `test_headline_shades` (new): coverage ramp of TEXT on BACKGROUND lands on all three exact shade entries; second pair midpoint equals the expected mix. Revert: removing the shades call gave 2 failed checks.
- Lint target clean; full build clean.

## Answers to the two budget points

1. INDEX8 budget run: `--ui desktop --backend mem --canvas 640x512 --format index8 --clock fixed --stats` in `tests/dui/aga_budget.cmake`. Limits (palette_colours_ui|art|tile|panel|flow): 32, 32, 64, 64, 64. Layout: 0-31 UI, 32-63 art, 64-127 tile, 128-191 panel, 192-255 cover flow. Goldens: `tests/golden/desktop/<view>-index8-640x512@1.hash`, updated only with `SELECTA_GOLDEN_UPDATE=1`, once before the push.
2. There is no dedicated text antialias ramp. Text edges are blended and snapped by `cube_index` (5 bits per channel) to the nearest UI entry. Entries 0-20 are theme tokens, 21-29 generic steps. P2b overwrites 24-29. Palette size is unchanged, so the UI limit of 32 holds. Caveat: 5-bit snapping means a nearby generic or token entry can still win over a shade (seen in the test with grey junk entries at 0x80); the shades help only where they are the nearest entry.

## Open items (not fixed, by plan)

- `dui-tall-pixels` FAILS: hash ec72b1c848057f8c vs golden 816c72009e7f28e4 (`tests/golden/desktop/library-rgba32-640x256-ydiv2@1.hash`). Renders at scale 0.625 and the Inter ydiv 2 atlases are now SemiBold 22/32 only, so other sizes fall to the runtime builder. Not proven whether the golden was already stale; no baseline binary. P3 changes the fit to 1.0 and regenerates this golden.
- `dui-aga-budget` FAILS on `testcard-index8-640x512@1.hash` (d8754f28854c75a5 vs bcde1f7d94c32706) and `dui-draw` FAILS on `testcard-index8-640x400@1.hash` (3ca02f92ad3a7da5). Both are the expected palette change from P2b. Goldens not updated; update once before the push. The run stops at the first golden mismatch, so the UI colour count after P2b is unverified.
- Plan says Inter 'L' ink is 15-16 / 10-11 rows; that is the em height. Measured ink is 12 rows at px 32 and 9 at px 22.

## Notes for P3

- Worktree needs `git submodule update --init third_party/nuade third_party/libopenmpt` to configure.
- Layout 640x512 logical via `desk_fit_q16` returning 65536 on the ydiv 2 tier; `sel_uim_layout_compute_tier` needs a rail breakpoint at 600-899.
- Only Inter atlases at ydiv 2: SemiBold 22 and 32. Everything else at ydiv 2 uses the runtime builder until P4's role table maps tokens to families.
- `desk_atlas_baked(family, px, ydiv)`. Amiga atlas glyph w/h are canvas columns/rows; by/ascent/line_height are layout units (2 per row). Kern table is one calloc'd dummy with kern_n=0. mini4/8 and mini6/6 are vendored but not baked.
- Palette entries 24-29 are headline shades; do not use them for tokens.
