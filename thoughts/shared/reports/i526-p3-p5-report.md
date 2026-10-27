# I526 P3-P5 report (AGA native fonts)

Design: thoughts/shared/plans/2026-10-27-aga-native-fonts.md sections 4-6. Branch worktree-agent-aef30ca469ddcc5ed. Not pushed, not merged.

## P3 layout tier - done (696041b)
- sel_uim_layout_compute_tier with SEL_UIM_TIER_BITMAP. Bitmap tier: widths 600-699 map to the icon-sidebar breakpoint with a 160 wide labelled rail (decision D1); 700 and up unchanged. The layout id still prints as icon-sidebar.
- desk_fit_q16 returns 65536 on the bitmap tier (ydiv > 1): layout is 640x512 in 1:1 pixels, no fit zoom.
- Tests: dui-layout (new tier case: ydiv 2 at 640x512 gives icon-sidebar with sidebar w=160; default tier at 640 stays mini-suggestion), dui-tall-pixels (now --resize 640x512, expects icon-sidebar and sidebar region; play button column extent now measured walking in from the background because the glyph shares the fill colour).
- Revert (desk_bitmap_text forced to 0): dui-layout FAILS ("the bitmap tier at 640x512 must give the labelled rail"). Restored, passes.

## P4 role table - done (257d3bf)
- plugins/ui/desktop/font_roles.c/.h: roles size>=25 H16 (Inter SemiBold 32), 18-24 H11 (SemiBold 22), >=14 or semibold 13 mini8/8, >=12 or regular 13 mini6/8, below mini4/8. Fallback when the line is taller than the box: next smaller headline, then mini8, then mini6/6; mini4/8 to mini4/6 (6-row rule).
- Five Amiga fonts baked: mini4/6, mini4/8, mini6/6, mini6/8, mini8/8. fontbake entry array 64 to 96.
- Counters (tags 0x50DC-0x50E0): bitmap_hits_mini4/6/8, headline_hits_22/32 on the ui_stats line.
- Test: dui-font-roles (unit). Measured line heights (layout units): H16 39, H11 27, mini fonts 16. Revert (role start forced to mini8): FAILS "fitting box keeps the role", "headline steps down". Restored.

## P5 end-to-end test - done (f1e3fcf)
- tests/dui/aga_native_text.cmake (dui-aga-native-text): library and home at 640x256 ydiv 2 fit, asserts layout 640x512 icon-sidebar, all four non-mini4 counters > 0, atlas_builds=0, font_misses=0, missing_glyphs=0, title ink rows 11-18.
- Stats line P5, library: missing_glyphs=0 atlas_builds=0 bitmap_hits_mini4=0 bitmap_hits_mini6=6 bitmap_hits_mini8=260 headline_hits_22=5 headline_hits_32=2.
- Stats line P5, home: missing_glyphs=0 atlas_builds=0 bitmap_hits_mini4=0 bitmap_hits_mini6=9 bitmap_hits_mini8=177 headline_hits_22=4 headline_hits_32=2.
- Native glyph measurements: Library title ink spans screen rows 16..32 = 17 rows (includes the y descender; a 32 px Inter L cap is 12 rows, the 22 px one 9). Bitmap glyph pitch is the font's 8 (mini8). Not measured: per-row library text ink (the library rows' album art tiles share the bands, so a clean ink walk needs a text-rect dump; open item).
- Found by the test: the home view drew U+2013 (en dash), missing in the Latin-1 only Amiga fonts (missing_glyphs=1). Root-cause fix in amiga_font.c: seven punctuation aliases (en/em dash, curly quotes, bullet) share the Latin-1 glyph; glyph_n is 224+7. dui-amiga-font asserts it.
- Revert 1 (bitmap tier off): dui-aga-native-text FAILS (layout assertion). Revert 2 (alias cp disabled): dui-amiga-font FAILS "en dash / bullet alias missing". Both restored.

## Goldens (b139652)
Updated once with SELECTA_GOLDEN_UPDATE=1: 13 hashes (11 index8 views plus testcard 640x400/640x512 and library-rgba32-640x256-ydiv2). dui-aga-budget passes after the update, so palette_colours_ui <= 32 on all views after P2b. Passing: dui-tall-pixels, dui-aga-budget, dui-draw, dui-text, dui-layout, dui-font-roles, dui-font-baked, dui-amiga-font, dui-aga-native-text.

## Open items
- Rig 2 (real AGA check) not done, deferred by instruction.
- Ellipsis (U+2026) and other non-Latin-1 punctuation still missing in Amiga fonts if they appear; only the seven aliases exist.
- The 5-bit palette snap caveat from P2b still applies to headline shades.
- desk_font role path bypasses the per-token atlas cache: counters count lookups, not unique atlases.
- No `lint` target in build/host; type-check = the build, clean.
- Per-row library text ink (at most 8 rows) not asserted.
