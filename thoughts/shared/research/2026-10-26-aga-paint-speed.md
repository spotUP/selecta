---
date: 2026-10-26
ledger: I463
status: partial (host measured, rig 2 not run)
---

# Why the AGA desktop UI paints a frame in 23.7 s on the Replay

## Evidence

Replay run (`--backend aga --format index8 --stats`, report in the I452 scratchpad): frames=2, paint_ms_min=3570, paint_ms_max=43769,
avg=23669, painter_ms main=22202 sidebar=13322 song=11487, atlas_builds=9, icon_resampled=3, ui_logical=1075x432 on 672x270.

The per-primitive timers (`paint_ms_*`) add up to about 4.0 s over both frames (fill 1579, round 1111, text 1026, icon 34,
gradient 149, dirty 75). The two frames cost 47.3 s. So roughly 41 s is spent outside every drawing primitive: in the painters'
own code, before the first pixel is drawn. The slow frame is frame 0 (max 43.8 s); frame 1 is 3.6 s, which matches the primitive total.

Host (Mac, same view, `--backend mem --format index8 --canvas 672x270`, the sdl backend refuses index8): frame 0 takes 48 ms,
painter_ms_main=55, atlas_builds=8. The Replay is about 900 times slower on frame 0 and only about 100 to 200 times slower on
pure integer pixel work (frame 1), which points at floating point work as the outlier.

Candidate found by reading the painters: `desk_font()` (plugins/ui/desktop/desktop_ui.c ~line 551) calls `desk_atlas_build()` on the
first draw of each (weight, size) pair, inside draw_sidebar / draw_nowplaying / the main painter. `desk_atlas_build`
(plugins/ui/desktop/atlas_build.c) runs stb_truetype: float scale, `stbtt_MakeGlyphBitmap` (float scanline rasteriser) for every glyph
of the ranges, and a 95 x 95 `stbtt_GetGlyphKernAdvance` table per atlas. The plugin CMake comment calls it "the only code using
stb_truetype (float)". The Amiga build is `-m68020`, no FPU in the build flags, so every float op is a library call.

Counter harness (scratchpad i463/h, builds atlas_build.c alone, host -O2): 20 atlases (10 sizes x 2 weights) = 32 ms, 6500 glyphs,
at both scale 1 and scale 0.625. At 9 atlases that is about 14 ms, 30 percent of the 48 ms host frame 0, and none of it appears in
any `paint_ms_*` class because the build runs between primitives. On the 68k the soft-float share is far larger than 30 percent.

A fractional fit scale (0.625) does not add work to the atlases (same glyph count); it only changes the pixel size.
It does cause 3 icon mask resamples (`icon_resampled=3`, commit 5be95d3); on the host these cost under 1 ms and were not separated further.

## Not measured (open)

- Rig 2 (UAE) was not started: only rig 8 was running and this task may not touch it; with the budget used on the host side no rig 2 run
  was made. So "paint_ms on rig 2 is also large and drops with the fix" is UNVERIFIED. The I439b AGA tree is also a Debug build
  (CMAKE_BUILD_TYPE=Debug, `-m68020 -mcrt=nix20`), which inflates every number; the Replay binary's build type is not recorded.
- The 41 s is attributed to atlas building by elimination (primitives sum to 4 s, painters hold the rest, atlas_builds=9 in exactly
  the painters that are slow) and by the host cost share, not by a sampled 68k profile. Next step is a stat that times
  `desk_atlas_build` (one `desk_prof_clock` pair around the call, a new `atlas_ms` stat) in the same Replay or rig-2 run; if it reads
  about 40 s the cause is confirmed.

## Violated invariant

M-DUI 2.11 / DUI-P6-3: the code that runs on the 68020 tier has no floating point ("src/gui and src/uimodel cross-build with no
floating-point symbols"). The text atlas builder is the exception that was placed in the plugin, outside src/gui, on the argument that
the desktop host does it once. On the Amiga it runs on the first paint, on the UI thread, with soft float, once per (weight, size).

## Level of the fix

Data level (build time), not the painter: the atlas is a pure function of (font, pixel size). Bake it into the binary with the
same host tool pattern as the icons (`gui_iconbake`, `SELECTA_ICONBAKE_EXE`): a host `gui_fontbake` rasterises Inter regular and
semibold at the pixel sizes the Amiga screen modes can ask for and emits `sel_gui_atlas` tables (glyph table, kerns, alpha page);
`desk_font()` takes a baked atlas when the requested pixel size exists and builds only for a size that was not baked (host, unchanged).
Sizes needed: lpx(size) for the 10 size tokens at the supported fit scales (1.0 for 640x512 style screens, 0.625 for 672x270, plus
the RTG and 2x cases already listed for the icon bake in 5be95d3). Same mechanism as `icon_buf` / `sel_gui_icons_baked`, so no new concept.

## Recommended fix and expected paint_ms

Bake the atlases (above). Expected effect: frame 0 falls from 43.8 s to roughly the steady frame cost, 3.6 s (the primitive total), i.e.
paint_ms_avg about 3.6 s on the Replay and `paint_ms_max` below 4 s. That is still far from interactive: the second cost is the pixel
work itself, about 1.08 M span pixels in 3.6 s (fill 1.6 s, round rect 1.1 s, text 1.0 s), which a release build and a
dirty-rect/index8 span fill (word writes, no per-pixel blend for opaque fills, precomputed rounded-corner coverage) should cut
further; that is a separate row once the atlas cost is confirmed gone. Expected sizes: the baked atlases are about 6500 glyph entries
and alpha pages per (weight, size) pair; cap the baked set to the pairs the UI uses (9 per scale seen in the Replay run).

## Reachability for the fix (when built)

Entry point `selecta --ui desktop --backend aga --format index8 --stats`; sentinel `atlas_builds=0` and a new `atlas_baked_hits>0`;
assert `paint_ms_max` under the budget on rig 2 and on the Replay.
