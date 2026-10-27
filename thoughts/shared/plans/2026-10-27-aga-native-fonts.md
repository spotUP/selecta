---
date: 2026-10-27
author: spot
status: approved
ledger: I526
design_section: thoughts/shared/plans/2026-10-22-architecture-design.md section 12 (runtime machine scaling, planar tier) and M3d/M4 (one src/gui library, AGA backend, D-M4-5, D-M4-6); D-M4-3 (Amiga UIs full screen by default); desktop UI plan 2026-10-24-desktop-ui.md Q14 (640x256 is the AGA design target)
---

# I526: native 640x256 text on the AGA desktop (bitmap fonts + Inter headlines)

## Problem, measured

The AGA 640x256 screen gets `ydiv 2` (desktop_ui.c:8172) and the fit zoom clamps at the floor 0.625
(`desk_fit_q16`, desktop_ui.c:8196-8202: 640/1024 = 0.625). The layout therefore runs at
640/0.625 x 512/0.625 = **1024x819 logical** (`desk_relayout`, desktop_ui.c:8212-8214; asserted by
tests/dui/tall_pixels.cmake check 1). Every text token is then `lpx(size)` = size x 0.625 pixels tall and, being a
tall-pixel atlas, half that in rows (`desk_font`, desktop_ui.c:591-613 -> `desk_atlas_baked(.., lpx(size), 2)`):
body text 13 -> 8 px -> **4 screen rows**, list rows 15 -> 9 px -> 4.5 rows, page titles 32 -> 20 px -> 10 rows.
That is the "too small to read" Johan reported.

Units today (keep them): layout units are square pixels; on ydiv 2 one layout unit is one column and half a row.
Atlas convention (atlas_build.c:43-55, gui_text.c:104-138, gui_draw.c:968): glyph `w`/`h` are canvas columns/rows,
`bx`, `by`, `ascent`, `line_height` are layout units; `sel_gui_blit_mask` divides the pen row by ydiv. A bitmap font
fits this format unchanged (alpha 0/255), so `sel_gui_text_draw` needs no change.

## Decisions already made (Johan, 2026-10-27, final)

- 640x256 uses bundled bitmap fonts baked into the binary: mini4 (4x6, 4x8), mini6 (6x6, 6x8), mini8 (8x8) from
  /Volumes/home/fonts, shipped as they are.
- Two headline sizes from Inter SemiBold through gui_fontbake: page title about 16 rows, section heading about 11
  rows, 2:1 corrected, antialiased through the index8 palette shades.
- Text at native screen pixels; layout native at 640x256 instead of 1024x819 scaled down.

## 1. Font role table (640x256, ydiv 2, scale 1.0)

One rule, in one place: on the bitmap tier `desk_font(d, semibold, size)` maps the existing size token to a family.
No call site changes; every role keeps the token it has today. Rows = screen rows; a token's rows today are
token x 0.625 / 2.

| Token (weight) | Family, size | Rows | Roles (file:line in plugins/ui/desktop/desktop_ui.c) |
|---|---|---|---|
| 32 semibold | Inter SemiBold headline H16 (px 32, ydiv 2) | 16 | Page titles: Library 4620, composer title 4601, Queue 2861, Playlists 3102, Visualizer 3587, Tracker 3679, Song Information 3840, Welcome back 1470, Now Playing page 6047, now-playing page song title 6055 |
| 18, 22, 24 semibold | Inter SemiBold headline H11 (px 22, ydiv 2) | 11 | Section headings: Recently played 1479, shelf heads 2300, Home empty section 5238 (token per call), Statistics 4707 (stats heading font 4701), search group heading 5135, detail big title 1150, "Nothing is playing" 6059, sidebar logo "Selecta" 5787 |
| 14, 15, 16 (both weights) | mini8 8x8 | 8 | Rail labels 5807 (semi 15); Library list rows 4613, 4666, 4689 (15) and their second column 4670/4672/4691 (14); library tabs and mode 4634/4636, crumb 4642, lib_tab_w 2503, style labels 2545 (LIB_STYLE_LABEL_PX 14); search field 1422/1424/1427 (14) and search rows 5143; home/library tile titles 2343, 4435, 4441, 5282 (semi 14); quick rows 2280; queue rows 2899/2914 (14); playlists rows 3118; scan line 2612; empty state 2843; stats values 4716-4740; "More/Less" 4724; sources 4830-4833; dialog title 2078 (16) and body 2079; dialog buttons 2094-2095, Add Folder / Clear Library / Sync 4626, 4627, 4835 (14); overlay body 6599; player bar title 5832 (semi 14); now-playing panel title 5917 (16) and page artist 6056 (16) |
| 13 semibold | mini8 8x8 | 8 | Song rows 1321 and drop targets 1369; buttons and chips via draw_button 745 at 13 (home chips 1474, 1442, queue buttons 2866, playlist buttons 3106, tracker tabs 3682, song info buttons 3844, search facets 5118/5089, Browse files 5240); song menu 1979; queue headers 2874, 2893; tracker channel heads 3698; toast 6620; widgets semibold 1054; text sheet 975 |
| 12, 13 regular; 12 semibold | mini6 6x8 | 8 | Secondary columns and meta: tile artist lines 2346, 4437, 4443, 5284 (13); draw_label 755; player bar artist 5833 (12); now-playing artist 5920, values 5990, lyrics 5999-6001, queue preview 6021; now-playing page format 6057, Next in Queue 6062; song information rows 3306-3338; tracker cells 3694-3746 (13); signal path 3508; viz pane names 3576 and viz buttons 3539 (12); queue Play Next 2900 (12); tooltip 6162/6185; overlay rows 6573-6588, command status 6604; widgets body 1053; text sheet 976 |
| 11 (both weights) | mini4 4x8 | 8 | Captions: transport times 6088, now-playing keys 5972/5989, "Next in Queue" heading 6018, widgets caption 1055 |
| any, box shorter than the line | same family's 6-row size (mini4 4x6, mini6 6x6; mini8 falls to mini6 6x6) | 6 | Transport queue badge "1" 6145 (box 12 layout units = 6 rows); any box under 8 rows |
| any token not listed (regular 18+, future tokens) | nearest row above: mini8 up to 16, H11 up to 24, H16 above | | none today; keeps `font_misses` at 0 |

All roles listed above were collected from every `desk_font(`, `draw_text*(` and `draw_button(` call in
desktop_ui.c (133 call lines, grouped by enclosing function); `draw_frame_text` 762 passes its caller's token.

The 6-row fallback is the only box-aware rule; it lives in `draw_text` (desktop_ui.c:628) as "line_height > box.h
-> the family's short size", so it is one rule, not a per-role override.

## 2. Reaching the native layout

Today: `desk_make_canvas` (8150) sets ydiv 2 for an AGA canvas up to 300 rows; `desk_scale_for` (8205) takes the
fit zoom `desk_fit_q16` (8196), which clamps at 0.625; `desk_relayout` (8212) runs
`sel_uim_layout_compute(&d->layout, px_to_logical(w), px_to_logical(cvh))` = 1024x819; `lpx` (553) scales every
layout length by 0.625 and `desk_font` bakes text at `lpx(size)`.

Smallest correct change (reuses the one layout engine):

1. A predicate `desk_bitmap_text(d)` = `d->canvas->ydiv > 1` (the 640x256 tall-pixel tier; same switch I468
   already uses, so `--ydiv 2` on the mem backend exercises it headless).
2. `desk_fit_q16` returns 65536 on that tier: scale 1.0, layout 640x512 square = 640x256 screen, one layout unit =
   one column. All existing geometry (row heights, margins, icons baked at x1, tall icon resample 8277) is reused.
3. Breakpoint: at 640 logical `sel_uim_breakpoint_for` (src/uimodel/layout.c:22) picks mini-suggestion, which has
   no sidebar and breaks I457 (tall_pixels check 1 asserts the rail). Fix in the layout engine, not in the plugin:
   `sel_uim_layout_compute` gets a tier argument (new `sel_uim_layout_compute_tier(l, w, h, tier)`, the old
   function calls it with the default tier) whose bitmap tier maps 600-899 to a rail layout. Rail width is decision
   D1 below. `desk_relayout` passes the tier.
4. `desk_font` (591): on the bitmap tier, look the (weight, token) pair up in a static role table (section 1) and
   return a baked bitmap or headline atlas; no stb build, no `lpx`. The atlas cache `d->atlas[w][i]` holds it as
   today; `desk_atlas_free` already leaves baked atlases alone.

Not touched: `sel_gui_text_draw`, blit_mask, input mapping (7656), the RTG and Mac paths (ydiv 1 keeps Inter at
the existing scales). 640x512 interlaced AGA (ydiv 1) stays on Inter at 0.625: decision D4.

## 3. Font bake

Searched: `tools/` (gui_fontbake.c, gui_iconbake.c, gen_*.c), `scripts/`, src/, plugins/, include/ for hunk or
diskfont parsing (`0x3f3`, `HUNK_`, `tf_CharLoc`, `DiskFontHeader`, `OpenDiskFont`): nothing in this repo reads
Amiga `.font` files; UP-Term only opens them at run time through diskfont. So the reader is new, but it goes into
the existing bake path, not a new one.

- Sources: copy the five bitmap files plus the three `.font` contents files into `third_party/fonts/amiga-mini/`
  with a README stating origin (/Volumes/home/fonts, Johan 2026-10-27: old public domain, shipped as is). The build
  never reads /Volumes/home.
- `tools/gui_fontbake.c` grows an `--amiga <family>:<size file>` input. Reader: HUNK_HEADER 0x3F3, HUNK_CODE 0x3E9;
  the body starts `moveq #-1,d0; rts`, then DiskFontHeader (Node 14 bytes, dfh_FileID 0x0F80 checked, revision,
  segment, name[32]), then TextFont (Message 20 bytes, tf_YSize, tf_Style, tf_Flags, tf_XSize, tf_Baseline,
  tf_BoldSmear, tf_Accessed, tf_LoChar, tf_HiChar, tf_CharData, tf_Modulo, tf_CharLoc, tf_CharSpace, tf_CharKern).
  Pointers are hunk offsets (HUNK_RELOC32 applies to them). CharLoc gives (bit offset, bit width) per char;
  the extra last entry becomes `at->missing`. Output per glyph: w = bit width, h = tf_YSize rows, alpha 0/255,
  adv = CharSpace or tf_XSize, bx = CharKern or 0, by = -(tf_Baseline + 1) x 2, ascent = (tf_Baseline + 1) x 2,
  line_height = tf_YSize x 2 (layout units, ydiv 2). Latin-1 0x20-0x7E and 0xA0-0xFF map to the same code
  points; the ellipsis falls back to "..." (gui_text.c ellipsis_of).
- `desk_baked_font` (atlas_build.h:30) keys on `semibold` today; it becomes a family id (Inter Regular, Inter
  SemiBold, mini4, mini6, mini8). `desk_atlas_baked` (atlas_build.c:157) looks up (family, px, ydiv).
- Headlines: gui_fontbake already bakes tall Inter (`desk_atlas_build_tall`, ydiv 2) but at the 0.625 scale
  (`DESK_FONT_BAKE_TALL_SCALE_Q16` atlas_build.h:26). Replace that set with `DESK_FONT_BAKE_TALL_PX 22, 32` at
  scale 1.0, SemiBold only: px 32 -> 16 rows, px 22 -> 11 rows. Antialiasing is the existing coverage mask; INDEX8
  blends it through the palette (gui_draw.c:363, 415), so no new path.
- CMake (plugins/ui/desktop/CMakeLists.txt:54-73): the custom command gets the eight font files as inputs and
  DEPENDS; the Amiga cross build keeps using the host tool via `SELECTA_FONTBAKE_EXE`.

## 4. Phases and tests

Checklist IDs I526-P1 .. I526-P5. Test runs follow 6a-1 (nice 19, 2 workers, affected tests only).

- **I526-P1 Bitmap font reader in gui_fontbake.** Fonts vendored, reader, family id in `desk_baked_font`.
  Regression test `dui-amiga-font` (tests/dui/amiga_font_unit.c, host only): bakes mini8/8, mini6/8, mini4/6 and
  asserts glyph_n = 224 (0x20-0xFF minus gaps, counted from tf_LoChar/HiChar, not assumed), adv = tf_XSize, rows =
  tf_YSize, line_height = 2 x YSize, and the bytes of 'A' and 0xE4 equal a hand-decoded copy of the CharData rows.
  Revert check: reading CharData without tf_Modulo as the row stride fails the 'A' compare.
- **I526-P2 Headlines at scale 1.0.** Tall bake set becomes SemiBold px 22 and 32 at ydiv 2. Regression:
  `dui-font-baked` (tests/dui/font_baked_unit.c) gains the case baked(semi, 32, 2) == build_tall(semi, 32, 2) and
  asserts glyph 'L' ink height 15-16 rows (px 32) and 10-11 rows (px 22). Fails before (no such bake).
- **I526-P3 Native layout.** `desk_bitmap_text`, fit zoom 1.0 on that tier, `sel_uim_layout_compute_tier` with the
  rail breakpoint. Regression: tests/dui/tall_pixels.cmake check 1 changes to `layout width=640 height=512` with the
  sidebar region present (I457 kept) and the play button square; `dui-layout` gains the tier case (640 wide,
  bitmap tier -> rail; default tier unchanged -> mini-suggestion). Fails before (1024x819 line). Golden hash
  updated once, before the push (ratchet rule).
- **I526-P4 Role table in desk_font + 6-row fallback in draw_text.** Counters `bitmap_hits[family]`,
  `headline_hits[px]` in the stats block (next to `atlas_baked_hits`, 9048). Regression: `dui-font-roles` unit over
  the table: every token in DESK_FONT_SIZES x both weights maps to a family (no NULL), and the fallback picks 4x6
  for a 6-row box. Revert check: removing the table sends text back to Inter and the unit fails.
- **I526-P5 Reachability (the one test).** `dui-aga-native-text` (tests/dui/aga_native_text.cmake) runs the real
  binary: `selecta --ui desktop --backend mem --canvas 640x256 --ydiv 2 --zoom fit --clock fixed --stats`, with the
  INDEX8 budget flag dui-aga-budget uses (unverified which flag; read tests/dui/aga_budget.cmake first), fixture
  library, views home and library, `--frame-dump`. Asserts:
  - layout 640x512 (native, not 1024x819);
  - sentinel: `bitmap_hits` > 0 for mini4, mini6 and mini8, `headline_hits` > 0 for px 22 and 32, `atlas_builds=0`
    (nothing built with stb at run time), `font_misses=0`, `missing_glyphs=0`;
  - native sizes from the PPM, walked as tall_pixels does: the "Library" title ink is 11-16 rows tall, a library
    row's text ink is at most 8 rows and its glyph pitch is 8 columns.
  Revert check: forcing `desk_bitmap_text` to 0 fails the sentinel and the 640x512 line.
- **Rig 2 (Amiga, live):** start `selecta --ui desktop --backend aga --stats` on rig 2 (purpose "Selecta session:
  I526 native fonts"), check the same stats line through the amiga MCP, then Johan reads every label on Home and
  Library (ledger acceptance).

## 5. Decisions (Johan, 2026-10-27: D1-D6 all as recommended)

Answered: D1 labelled rail 160; D2 no synthetic bold; D3 keep row metrics, tighten after the rig 2 check; D4 640x512 out of scope; D5 6-row fonts are fallback only; D6 reserve 6 palette entries for headline shades. The options below are kept for the record.

- **D1 Rail at 640 wide.** (Recommended) labelled rail 160 layout units with mini8 labels: "Song Information" (16
  chars x 8 = 128) fits beside the icon; main column 480. Alternative: icon-only rail 56 (exists today,
  tooltips name the doors): main 584, but no rail labels to read.
- **D2 Semibold on bitmap fonts.** (Recommended) no synthetic bold: weight shows through family (semibold 13 ->
  mini8, regular 13 -> mini6) and ink colour. Alternative: bake an Amiga-style smear bold (FSF_BOLD, +1 column):
  stronger hierarchy, every bold string one column per char wider, more ellipsis.
- **D3 Row density.** (Recommended) P1-P5 keep today's layout metrics (LIB_ROW_H 48 = 24 rows for an 8-row font,
  about 6 Library rows visible); a follow-up row tightens row heights on the bitmap tier after Johan sees rig 2.
  Alternative: tighten now (rows 24 units = 12 rows, about 12 visible): denser, but touches every *_ROW_H enum
  and their hit tests in this row.
- **D4 640x512 interlaced AGA.** (Recommended) out of scope: ydiv 1 keeps Inter at 0.625; the bitmap tier is
  keyed on ydiv 2 only. Alternative: bitmap fonts there too (doubling rows): flicker-prone mode, extra layout case.
- **D5 mini6 6x6 and mini4 4x6.** (Recommended) used only by the box-fit fallback (badge 6145, short boxes).
  Alternative: assign them to a role (for example tracker cells) for more rows per screen at lower legibility.

## 6. Amarchy comparison (ledger I530)

Source: thoughts/shared/reports/i530-amarchy-font-rendering.md (answer from the Amarchy session, 2026-10-27),
code in ~/Code/UP-Term/vtcon branch prefs-inline (render/ttf.c, render/aa_ink.c, amarchy/menu_text.c,
amarchy/menu_font.c), plan vtcon/thoughts/shared/plans/2026-10-26-amarchy-gui-aa-font.md.

How Selecta blends text on INDEX8 today (src/gui/draw/gui_draw.c:722-760, `blit_mask_raw`): full 0-255 coverage,
blended against the real destination byte and resolved to the nearest entry of the 32-entry UI palette region
(cube built in `set_palette_raw`, gui_draw.c:75-90). So Selecta already blends against the true background, like
Amarchy's per-row background pens. What it does not do is guarantee that intermediate shades exist: if the UI
region holds no colour between text and surface, a coverage pixel snaps to fg or bg and the edge becomes a
2-colour threshold.

| Amarchy part | Selecta I526 | Why |
|---|---|---|
| Runtime TrueType rasteriser on the Amiga (render/ttf.c, glyph cache per size) | Reject | Inter is baked offline by gui_fontbake (atlas_build.h); no TTF file or rasteriser on the Amiga, zero first-draw cost on the 68020, `atlas_builds=0` is the sentinel. Selecta has a fixed size set, Amarchy a user-picked size, which is why they need runtime. |
| Antialias by 3 shade inks per fg/bg pair (1/4, 1/2, 3/4), known background per row | Adopt the shade guarantee, not the pen allocator | Selecta owns its whole palette (no ObtainBestPen), so the equivalent is: the UI region of `sel_gui_palette_budget` (gui_draw.c:127) reserves the 3 shades of text-over-surface (and text-over-selection) for the headline pairs. `blit_mask_raw` then resolves to them unchanged. New phase I526-P2b below. |
| Aspect from DisplayInfo Resolution x/y (`menu_screen_aspect`) | Adopt the idea, already covered | Selecta keys the tall bake on `ydiv` (desk_make_canvas, desktop_ui.c:8172): 640x256 PAL hires = 2:1, which is what DisplayInfo reports for that mode. Reading DisplayInfo on the AGA backend to pick ydiv is a follow-up only if a non-2:1 mode becomes a target (D4). |
| No hinting, no gamma | Same | gui_fontbake uses stb_truetype coverage without hinting; no gamma step is added. |
| Bitmap .font drawn plainly with Text() behind one measure/draw API | Adopt the single-API rule, reject Text() | Bitmap fonts become atlases in the same `sel_gui_atlas` format, so `sel_gui_text_draw` measures and draws both, ellipsis included (gui_text.c:104-138). Text() would split measure and draw paths and bypass the canvas. |

Body text: stays on the mini bitmap fonts, no antialiasing. At 8 rows on a 2:1 screen an AA Inter body face is
4-5 rows of x-height with 3 shade levels; Amarchy's readable sizes are 8-11 px on square-ish rows (report point 1),
which on ydiv 2 are 16-22 layout units, i.e. headline sizes here. Johan's decision already fixes this; the
comparison supports it.

- **I526-P2b Headline shade guarantee.** `sel_gui_palette_budget` reserves 3 entries per headline fg/bg pair
  (text on surface, text on selection) at 1/4, 1/2, 3/4. Regression test in the gui_draw unit: a px 32 headline
  on INDEX8 has at least 3 distinct non-fg, non-bg indices along its edges; fails before when the UI region has
  no intermediate shade (to be measured first: unverified whether today's budget already holds them).
- **D6 Shade entries in the 32-entry UI region.** (Recommended) reserve 6 entries (3 shades x 2 pairs): edges
  match Amarchy's 3-level look. Alternative: rely on whatever the theme puts there: no palette cost, but headline
  edges can collapse to a 2-colour threshold depending on theme.
