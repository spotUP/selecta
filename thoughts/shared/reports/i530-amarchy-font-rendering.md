---
date: 2026-10-27
ledger: I530
feeds: thoughts/shared/plans/2026-10-27-aga-native-fonts.md (I526)
source: answer from the vtcon-a1 session (Amarchy GUI), 2026-10-27
---

# How the Amarchy GUI draws small, antialiased, readable text

Code lives in `~/Code/UP-Term/vtcon`, branch `prefs-inline` (worktree `.claude/worktrees/prefs-a1`):
`render/ttf.[ch]`, `render/aa_ink.[ch]` (+ `aa_ink_rule.h`), `amarchy/menu_text.c`, `amarchy/menu_font.c`.
Plan: `vtcon/thoughts/shared/plans/2026-10-26-amarchy-gui-aa-font.md`.

1. **Font source.** The user picks "name:size" (size in pixels). A .ttf found in the terminal's font drawers
   (`vo_ttf_spec`) is drawn antialiased; anything else falls back to an Amiga bitmap .font through OpenDiskFont
   and Text(). No fixed sizes; the menu default is about 8-11 px.
2. **Rasteriser.** Their own, at runtime, not baked. `render/ttf.c` reads the TTF through callbacks glyph by
   glyph (no full load), font-rs style signed-area accumulation, fixed point only (68020 without FPU works),
   coverage 0..255 per pixel. Glyphs are cached per code point for the open size (`amarchy/menu_text.c`).
3. **Antialias on a limited palette** (`render/aa_ink.c`). A 256-entry table of fg over bg per coverage.
   - RTG screen deeper than 8 bits with cybergraphics: one WriteLUTPixelArray through that table.
   - Pen screen: coverage is cut into 3 levels (1/4, 1/2, 3/4 of the way to fg). Each level gets a shade ink:
     ObtainBestPen for that RGB, or the nearest existing pen when none is close. One masked template blit per
     level, lightest first, over a background whose pen is known per row state (normal, selected). Each fg/bg
     pair has its own shade set. No dither for text.
4. **Pixel aspect.** `menu_screen_aspect()` (`amarchy/menu_font.c`) reads DisplayInfo Resolution x/y for the
   screen mode and passes rx/ry to the TTF init (`mtc_ttf_init`), so glyphs are scaled for non-square pixels
   (lowres, hires, interlace). Bitmap fonts pick a square or tall variant from the same flag.
5. **What made small sizes readable.** No hinting (TTF instructions skipped), no gamma. Instead: a font
   designed for screens at small sizes, real per-row background pens so shades blend against the true
   background, and 3 shade levels rather than a 2-colour threshold.
6. **Failed or avoided.** ttf.library 0.8.5 (cannot reach code points above U+FFFF). Open issue on their side:
   the ASL font requester in amarchy-gui gurus #80000004 on the real A1200, believed unrelated to the rasteriser.

## What this means for I526 (to settle in the plan)

- Our headlines are baked offline from Inter; Amarchy shows a runtime rasteriser is feasible on 68020, but an
  offline bake is cheaper at runtime and needs no TTF on the Amiga. The deciding points are the shade-pen
  scheme (3 levels per fg/bg pair, known background pen per row) and the aspect correction from DisplayInfo,
  which apply to a bake as much as to a runtime rasteriser.
- Reuse rather than copy: if Selecta adopts runtime TTF, `render/ttf.c` and `render/aa_ink.c` are the
  candidates; licence and repo boundary (UP-Term vs MusicPlayer) must be checked first.
