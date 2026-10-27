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

## P5 gap closure

Test: tests/dui/aga_native_text.cmake (dui-aga-native-text), passes. 2 of 4 gaps closed, 1 closed without a code change, 1 open.

- G1 (bitmap_hits_mini4 > 0): OPEN, not asserted. Tried the now-playing view with `OPEN` and `OPEN` + `PLAY` + `wait 200` (status=Playing): mini4 stays 0. Cause: at 640x512 the bitmap tier has only the sidebar, main and player regions, so the "detail" region draw_nowplaying needs (key labels, "Next in Queue", desktop_ui.c ~6156-6204) is never drawn, and `st.caption = desk_font(d, 0, 11)` (line 1089) is looked up before the tier is on, so it never counts. Remaining mini4 users: the subsong "n/m" label (6274, needs a multi-subsong song) and the "1" badge (6331); the caption of gui_widget.c 231/288/406 is not reached by these views. Needs a view that draws one of those or a product decision on the detail region at this tier.
- G2 (font_misses=0): font_misses is not a ui_stats key, so a dui_stat assert is impossible. It is printed on the `np_page_text ... font_misses=N` line of every run; the existing check matched "font_misses=0" anywhere, now anchored to that line (`np_page_text [^\n]*font_misses=0\n`). The plan item was already met in substance; now exact.
- G3 (library row ink <= 8 rows, pitch 8): DONE. No existing printed row geometry, so no new flag: the row is the first ink band at frame rows 96..219 (below the statistics strip, above the player region at layout row 440 = frame row 220). Measured: ink rows 106..113 = 8 rows. Pitch: 7 of the 8 column offsets (from the first ink column) carry ink (0-5 and 7), offset 6 is blank in every cell; a different pitch fills all 8 offsets. The row-start row 96 is a literal derived from the layout constants (title 24+40, tab 32, strip 48, halved).
- G4 (title cap 11 to 16): DONE. Ink of the first glyph run of "Library" (the capital L, columns 190..205) measured rows 17..28 = 12 rows, asserted 11 to 16 (no widening).

Revert results: forcing desk_bitmap_text to 0 was not run on its own (the stats and layout asserts fail first, as before). Disabling only the role lookup in desk_font (`if (0 && desk_bitmap_text(d))`) fails the test at the bitmap_hits_mini6 assert (library: bitmap_hits_mini6=0). The PPM asserts G3/G4 were NOT shown to fail on their own cause: with the role lookup off the dump still gives cap L = 12 rows (the headline falls back to the same baked Inter 32), so G4 does not discriminate that revert; the G3 numbers on the reverted run were not measured (budget). Treat G3/G4 revert-sensitivity as unproven. Code restored (git checkout of desktop_ui.c), binary rebuilt, test green.

## I537 items 1-5

All changes in plugins/ui/desktop/desktop_ui.c; assertions in tests/dui/aga_native_text.cmake section 4 (test dui-aga-native-text).

1. Title covered: lib_add_rect/lib_clear_rect used fixed 120/132 widths, Clear Library sat at x=256 over the title (ends ~288). Header buttons now sized by lib_header_btn_w (measured label + pad); Clear Library x=292. Test: Clear Library x >= 289. Revert check: with the old desktop_ui.c the test fails "button at x=256 covers the title". Pixel version was not usable (button outline merges with the title tail), so the check compares layout numbers.
2. Rail labels ellipsized: label box started after icon + 16 px gap (~86 px). Gap is now 8 px on the bitmap tier, box runs to the rail edge, and a door with a shorter form (Song Information -> Song Info) uses it only when the full label does not fit. Test: last 8 ink columns of Your Library, Now Playing, Song Info span >= 4 rows. Revert check not run separately (the item 1 assertion fails first).
3. Banner buttons: fixed 112 px buttons; now measured label + 16 (Sync 48, Not now 72), head text ends 8 px before Not now. Test (SDL banner run): widths 48 and 72. Centring is not pixel-verified (frame dump predates the banner redraw). Revert check not run separately.
4. Tab row overflow: tab pad shrinks (lib_tab_pad) until six tabs fit the main region; the style chooser moves to its own row when it does not fit right of the tabs (lib_style_below). Test: every tab ends <= 640, tabs in order without overlap, chooser segments not on the tab row inside it. Revert check not run separately.
5. Stats strip under transport: banner grew by 32 per due source. Now at most 4 rows tall, laid out in columns (nag_cols). Test (SDL banner run, 8 due sources): stats rect ends <= player region top (440). Revert check not run separately.

Goldens updated (intended layout change): library-rgba32-640x256-ydiv2, library-sources-rgba32-1280x800, grid-view-rgba32-1280x800. Existing P5 row scan in the same test now starts below the stats strip from the stats rect instead of frame row 96.
Run: dui-aga-native-text, dui-library*, dui-sync-banner, dui-tall-pixels, dui-layout, dui-grid-coverflow, dui-cover*, dui-style*: 13 of 13 pass.
