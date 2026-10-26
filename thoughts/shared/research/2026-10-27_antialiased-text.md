---
date: 2026-10-27
topic: Antialiased text for Selecta (UP-Term engine vs current path vs SDL_ttf)
status: research
ledger: I375
---

# Antialiased text (I375)

## Headline

Selecta already draws antialiased text on every true-colour path. The one place it is NOT antialiased is the INDEX8 canvas, where `sel_gui_blit_mask` thresholds coverage to 1 bit (src/gui/draw/gui_draw.c:529-531). Copying the UP-Term engine would not change the Mac output: it is a TrueType coverage rasterizer, the same class of thing as the stb_truetype Selecta already uses. Evidence below. Unverified: whether Johan's "not smooth" comes from INDEX8, from whole-pixel glyph positioning, or from stb vs hinted look; he has to say which screen (Mac or Amiga, which depth) he saw.

## 1. The UP-Term engine (read only, ~/Code/UP-Term/vtcon)

- It is `render/ttf.c` (846 lines) + `render/ttf.h` (110): "UP-Term's own TrueType reader and rasterizer" (ttf.h:1). Font source = TTF read at runtime through callbacks (ttf.h:14-27), glyf read per glyph, no hinting, simple + composite glyphs (ttf.h:7-9). Not prebaked atlases, not bitmap-font filtering. The Symbols Nerd Font (2.6 MB) is the font it was written for.
- API: `ttf_open/ttf_close`, `ttf_glyph_index`, `ttf_advance`, `ttf_render(font,gid,xscale,yscale,ox,baseline,cov,w,h)` (ttf.h:59-60), plus `ttf_threshold[_at]`, `ttf_fit`, `ttf_place`, `ttf_embolden`.
- Output: 8-bit coverage 0..255 per pixel, non-zero winding (ttf.h:13). Algorithm: font-rs signed-area accumulation, fixed point only, no FPU needed, so it runs on a plain 68020 (ttf.h:11-13). Portable C89, no OS calls; tested on the host (tests/test_ttf.c, oracle tools/fonts/ft_oracle.c = FreeType unhinted reference, ft_oracle.c:1-5).
- Blending is NOT in ttf.c. It lives in `render/emoji.c:94 ce_mix(fg,bg,a)` (a 256-entry table made once per fg/bg pair, amiga_render.c:2383-2391) and in `render/amiga_render.c` aa path: true colour / CGX: one LUT write of the coverage through the 256-entry table (amiga_render.c:2453, `vr_cgx_write_lut`). Pen screens (8-bit CLUT): up to 3 shade pens, found by `nearest_pen` and used only if closer to the wanted shade than fg or bg (amiga_render.c:2395-2417); the glyph is drawn as 4 threshold passes with BltTemplate (amiga_render.c:2463-2477, levels 64/128/192/255). Sentinel `n_aa` (amiga_render.h:210). Profile knob `outline-antialias` default on (prefs_core.h:61).
- Licence/author: MIT, "Copyright (c) 2026 spotUP" (vtcon/LICENSE:1-3). Same author, copying is free.
- amarchy/menu_font.c (74 lines) is NOT antialiasing: it opens Amiga bitmap fonts via OpenFont/OpenDiskFont. amarchy/gui.c only mentions coverage for icons (gui.c:350). Glyph code in handler/ is the consumer of ttf.c, not a second engine.
- Size to copy: ttf.c + ttf.h ~ 950 lines, no dependencies.

## 2. How Selecta draws text today

- Font: embedded Inter Regular and SemiBold (third_party/fonts, `sel_font_inter_*`, atlas_build.h:21-22), sizes 11,12,13,14,15,16,18,22,24,32 (desktop_ui.c:44), multiplied by the integer UI scale (desktop_ui.c:386-387).
- Rasterizer: stb_truetype (stb_impl.c, atlas_build.c:43-68 `stbtt_MakeGlyphBitmap` into an 8-bit alpha page; float, confined to atlas_build.c, atlas_build.h:2-4). Glyph advance rounded to whole pixels (`g->adv`, atlas_build.c:52), kerning rounded (atlas_build.c:133-134). So: 8-bit coverage, antialiased, but unhinted-grey at whole-pixel pen positions, no subpixel position, no gamma.
- Draw: `sel_gui_text_draw` (src/gui/text/gui_text.c:126-128) calls `sel_gui_blit_mask` per glyph (gui_draw.c:515-535). RGBA32: real alpha blend in `put` (gui_draw.c:330-338). So the Mac SDL path blends coverage already; backend_sdl just presents the RGBA32 canvas. Amiga rtg on a 16/24-bit screen uses the same canvas (rig 2 opened depth 16, thoughts/shared/research/2026-10-26_amiga-desktop-look.md:49), so also antialiased.
- INDEX8: `put` blends against the palette entry then re-quantises to the colour cube (`cube_index`, gui_draw.c:340-348), BUT blit_mask forces alpha to 0 or 255 first (gui_draw.c:529-531). So edge shades never exist on INDEX8; that is the only 1-bit path. Palette cost of enabling it: the canvas stat `palette_colours_ui` budget is 32 (2026-10-25_i286-amiga-readiness.md:20); text shades are ink-over-background blends, and with cube quantisation they snap to existing cube entries, so the cost is cube cells touched, not new palette slots. Not measured; measure with palette_colours_ui before and after (I369 is the open INDEX8 acceptance).

## 3. Recommendation: one shared path, keep it in src/gui

1. Do NOT copy ttf.c for the Mac: it duplicates stb_truetype (two rasterizers = the duplicate the rules forbid) and gives the same coverage. Do NOT add SDL_ttf: it needs FreeType, does not exist on the Amiga build, renders to SDL_Surface/texture outside the canvas (so canvas_hash, dirty rects, frame dump and the INDEX8 canvas all bypass it), and would be a second text path. It also cannot serve the Amiga at all.
2. The shared mechanism already exists: alpha atlas -> `sel_gui_blit_mask` -> canvas. Make it smooth everywhere by fixing that path:
   a. INDEX8: remove the threshold in blit_mask and let `put` blend + cube-quantise (dither already available via the g_bayer argument). Budget check via `palette_colours_ui`. If 32 is exceeded, quantise text shades to the 3-shade scheme UP-Term uses (shades at coverage 64/128/192) rather than 256 levels: adopt that idea (levels, nearest-pen only when closer than fg/bg) inside the canvas `put`, not a new draw path.
   b. Only if the true-colour look is still judged rough: pick improvements in atlas_build.c: 2x horizontal oversample (stbtt_MakeGlyphBitmapSubpixel or stb oversampling) and/or a coverage gamma LUT applied once at atlas build. These are atlas-build changes, same single path.
3. Optional later, only if Johan wants the Amiga to avoid floats in atlas_build (stb uses float; a 68k without FPU is slow at first atlas build): replace stb_truetype in atlas_build.c with UP-Term ttf.c (copy render/ttf.c, ttf.h into src/gui/text/ as gui_ttf.c/.h, glue = ttf_io callbacks over the embedded blob, ttf_render into the same `page`). That swaps the rasterizer behind `desk_atlas_build` for BOTH backends; it removes the float and vendored stb_truetype, it does not add a path. This is a separate row (cost: ~950 lines, port test against the existing atlas output), not needed for smoothness.

## 4. Test idea

dui frame dump assertion (ledger I375 acceptance): render a fixed string ("Selecta Home" at 13 px semibold) on a flat background via the existing dui frame dump, for both RGBA32 and INDEX8 canvases. Take the glyph bounding box, count distinct colours; assert > 2 on RGBA32 (already true, guards regression) and, after fix 3a, > 2 on INDEX8 (fails today because of gui_draw.c:529). Add a sentinel: count of pixels with 0 < alpha < 255 passed to `put` (new ui_stat `text_edge_px`) must be > 0 so the test proves the blend ran, per the reachability rule; and assert `palette_colours_ui <= 32` on INDEX8. Pure-function unit test where it lives: blit_mask with a 1-row mask {0,64,128,255} on both formats.

## Open question for Johan

Which display looked rough: Mac window, Amiga 16-bit rtg, or Amiga 8-bit? The answer decides between 3a (INDEX8 only) and 3b (oversample/gamma for all).
