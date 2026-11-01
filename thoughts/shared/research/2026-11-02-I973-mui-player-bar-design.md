---
date: 2026-11-02
author: spot (design agent, Opus)
row: I973
status: design, no code changed
related:
  - thoughts/shared/research/2026-11-01-I949-mui-640x256-design.md (section 3 compact class, section 5 test pattern)
  - thoughts/shared/research/2026-10-30-mui-mac-drift.md (B1-B4, lines 75-78)
  - thoughts/spot/agent-reports/I973.md (screenshot read)
---

# I973: MUI player bar, design

Goal: the MUI player bar matches the Mac footer in structure. Every control sits on one centre line. Icon buttons are
equal squares. No stray status text. Spacing is the tight MUI tier, not the Mac spacing (Johan, I949).

Line numbers are from main at 7c937e31. The brief named mui_ui.c 732/763; those are stale. The player group is
`mui_player_group`, plugins/ui/mui/mui_ui.c:1100-1166.

## 0. What the screenshot shows

The screenshot (2026-11-02 00:02, about 796x79 points) shows the NON-compact tree. The right column has the volume
slider on top, then the extras row, then "Stopped". The two chevrons are Next Album / Previous Album; the plus and minus
are Pitch Up/Down and Tempo Up/Down (`mui_p8_extras_row`, mui_ui.c:1083-1095, actions from mui_p8.c:31).
`mui_player_group` builds that tree only when `mui_screen_compact()` returns 0 (mui_ui.c:1106, 919-924).

So one of these holds:
- the screen was not a 640x256 bitmap screen (an 800-wide shot points to RTG 800x600, or `g_screen_rtg` set), or
- the rig ran a build older than I949d (merged 1f9cfdfd, 2026-11-01 21:32).

Check through the `--log` lines "screen plan mode" and "pixel aspect" (open question Q1). The design below fixes both
trees, because the faults are in the tree shape, not in the compact switch.

## 1. Faults and causes

| # | Fault (Johan / screenshot) | Cause | Where |
|---|---|---|---|
| F1 | Transport row and right group are not on one centre line | The bar is an HGroup of three independent columns with different row counts: colL (art + titles VGroup), colC (row1 transport, row2 seek = 2 rows), colR (volume, extras, status = 3 rows). MUI centres each column inside the HGroup on its own, so no two rows share a centre line. | mui_ui.c:1124-1126 (colC), 1147-1158 (colR), 1159-1163 (hbar) |
| F2 | Icon buttons have unequal widths (heart about 162 px, transport about 102 px, extras about 76 px) | `aabtn_askminmax` sets MaxWidth = MUI_MAXMAX for every role except toggle, and every icon button has MUIA_Weight 100. Each icon button then stretches to share its own row: the heart shares the title row, the transport shares colC / 6, the extras share colR / 6. | mui_aabtn_class.c:109-120 (MaxWidth line 115), :301 (weight); mui_ui.c:1112 (transport weight 100), mui_make_button_icon around :745 (weight 100), :1088 (extras) |
| F3 | "Stopped" sits at an odd place, low right | The status text is left-aligned after an 8-space format text, in the bottom row of colR. The Mac bar has no status or format text. | mui_ui.c:1153-1154; mui_make_text aligns LEFT at :870-873 |
| F4 | Time labels are not centred on the seek bar | Text height is the text minh; slider height is m.height; the slider draws its track at (h - track_h)/2. The HGroup centres the two objects, but their heights differ, so the text baseline and the track centre can differ by a few rows. Also the clock "0:00 " is left-aligned inside a fixed width of "00:00:00", so the gap to the bar changes with the time. Unverified on the rig. | mui_aaslider_class.c:93-102, 146; mui_aatext_class.c:77-85; mui_controls.c:135-145; mui_ui.c:1135-1138 |
| F5 | Unequal margins left and right | Column weights 33/47/20 (MUI_PLAYER_W_LEFT/CENTRE/RIGHT, mui_map.h) share the slack. The slack goes to stretched buttons (F2), not to fixed gaps. Panel pad comes from `mui_aa_panel_pad`. | mui_map.h MUI_PLAYER_W_*; mui_aapanel_class.c:88-97 |
| F6 | Bar height not controlled | `mui_player_band` computes `band.height` (art_h + 2 * MUI_PLAYER_COMPACT_PAD), but `mui_player_group` never applies it. No MUIA_FixHeight is set on the bar, so the bar takes the sum of its tallest column (colR, 3 rows). | map/mui_layout.c:86-101; mui_ui.c:1106-1166 (band.height unused) |
| F7 | Cover art clipped | Unconfirmed. Suspect: art_h comes from the pixel aspect (`mui_art_height`, mui_layout.c:82-84) and the panel uses FixHeight art_h with pad 0 (mui_aapanel_wh, mui_ui.c:926-933), while colL is centred in a taller bar; or the art draw uses the wrong mtop. Needs a rig log of the art object's _mtop/_mheight against art_h. | mui_layout.c:82-90; mui_ui.c:926-933; mui_aapanel_class.c:88-97 |
| F8 | Extras and a remaining-time label the Mac does not show | The non-compact band sets extras = 1, right_rows = 3. row2 always adds MUI_ATTR_REMAINING. The Mac one-row footer has neither. | mui_layout.c:98-100; mui_ui.c:1142, 1149-1151 |

## 2. Mac truth (structure only)

plugins/ui/desktop/desktop_ui.c. `SEL_UIM_FOOTER_ONE_ROW_MAX_H` 600 (uimodel.h:193); BAR_ONE_ROW_H 40, BAR_H 72
(src/uimodel/layout.c:8-9). At both 640x256 and 800x600 the Mac footer is the ONE-ROW form.

One-row form (`footer_one_row` :1197, `desk_button_rect` :10010-10022, `seek_span` :1209-1231, `vol_bar_rect`
:1239-1243). Left to right, every element centred on one line:
1. subsong label (56 px, "n/m") plus subsong minus/plus buttons, only when the song has subsongs;
2. Shuffle, Previous, Play, Next, Repeat: equal squares TIP_BTN 32, same step;
3. clock (40) | seek bar (takes the rest) | length (40), starting at the last button + 2 * spacing;
4. speaker icon (16) and volume bar (96) at the right edge.

Bars are 4 px tall. Play is a round, filled button with a 24 px icon; other icons are 20 px. No Stop, no remaining time,
no format, no status, no album/speed extras. Art plus title/artist (`draw_player_left` :11081) shows only at player width
>= 900. The heart lives on the Now Playing panel (np_like, about :11157), not in the bar.

## 3. Target MUI 3.8 object tree

One tree for both the 256-row bitmap class and RTG. Only sizes change (from metrics), not the shape. MUI 3.8 features
only: HGroup/VGroup, MUIA_Weight, MUIA_FixWidth/MUIA_FixHeight, MUIA_Group_Spacing, RectangleObject spacers, MUIA_Frame
none. No MUI 4/5 attributes.

```
g = AAPanel(SURFACE)                       MUIA_FixHeight = pb.bar_h, pad = tight tier
 `- bar = HGroup                           MUIA_Group_Spacing = pb.gap, every child centred (all children have MaxHeight = own minh,
     |                                      and the group height is fixed, so MUI centres each child on the same line)
     |- left = HGroup  MUIA_Weight 0       (shown when pb.show_left; see below)
     |    |- art   = AAPanel_wh(RAISED, art_w, art_h)   FixWidth art_w, FixHeight art_h
     |    `- titles = VGroup MUIA_Weight 0, FixWidth pb.titles_w
     |         |- title  AAText (MUI_ATTR_TITLE)     [heart: see Q6]
     |         `- artist AAText (MUI_ATTR_ARTIST)
     |- sub   = HGroup MUIA_Weight 0       (subsong label + minus + plus; hidden until the song has subsongs; Q5)
     |- trans = HGroup MUIA_Weight 0, MUIA_Group_Spacing pb.btn_gap
     |    `- 5 (or 6, Q2) AAButton icon-only, FixWidth = FixHeight = pb.btn_px, Play role ACCENT
     |- clock = AAText fixed width "00:00:00", align RIGHT  (MUI_ATTR_CLOCK)
     |- seek  = AASlider MUIA_Weight 100   (the only stretching child; takes all slack)
     |- len   = AAText fixed width "00:00:00", align LEFT   (MUI_ATTR_LENGTH)
     |- spk   = AAButton icon-only "Volume" FixWidth = FixHeight = pb.btn_px (mute toggle if the action exists; else an AAImage)
     `- vol   = AASlider FixWidth MUI_VOLUME_FIX_W (96)
```

Removed from the bar: MUI_ATTR_REMAINING text, MUI_ATTR_FORMAT text, MUI_ATTR_STATUS text, the extras row (menu only,
Q3/Q4), colL/colC/colR and their weights 33/47/20. The status stays visible through the Play button state (Play/Pause icon)
and the window title; format moves to Song Information (it is already there).

Rows at 256 rows vs RTG: there is ONE row in both. The difference is sizes only:

| Value | 640x256 bitmap (aspect q8 512, tight tier) | RTG 800x600 (aspect 256) |
|---|---|---|
| btn_px | max(icon_h + 2 * pad_y, text height + 2 * pad_y) from `mui_aa_metrics_aspect` | same formula, default tier |
| bar_h | max(btn_px, art_h) + 2 * MUI_PLAYER_COMPACT_PAD, capped by MUI_PLAYER_COMPACT_MAX_H 36 | same formula; no cap (RTG pixels are square, about 40 like the Mac) |
| show_left | 1 if the width left after trans + clock + MUI_SLIDER_MIN + len + spk + vol >= art_w + titles minimum; else 0 | same rule |
| art_h | 28 (56 * 256 / 512) | 56 |

The left art + titles block follows the Mac rule in spirit (Mac hides it below 900 px). MUI keeps it when it fits,
because the I949 compact class asked for art 56x28 plus title/artist in the band. Q7 asks Johan which rule wins.

Why this centres: in a fixed-height HGroup, MUI 3.8 gives each child its own height (MaxHeight = minh for AAButton,
AAText, AASlider) and centres it vertically. With one row and no nested multi-row columns, all centres coincide (fixes F1).
The text-vs-slider centre (F4) then only depends on the slider drawing its track at its own centre, which a pure function
asserts (section 4).

## 4. Pure functions (host-testable)

Searched before choosing: `mui_player_band`, `mui_art_height`, `mui_transport_row_width`, `mui_extra_row_width`,
`mui_tool_row_width` (map/mui_layout.c); `mui_transport_widest`, `mui_transport_icon_only`, `mui_player_transport_at`,
`mui_attr_fixwidth` (map/mui_controls.c); `mui_aa_metrics`, `mui_aa_metrics_aspect`, `mui_aa_rows`, `mui_aa_panel_pad`
(map/mui_aa.h); `mui_button_width` (mui_icons); desktop `desk_button_rect` / `seek_span` / `vol_bar_rect` (desktop-only
statics, not shared). No existing function returns a button square size or per-element x positions for the MUI bar.

Decision: EXTEND `mui_player_band` and `mui_player_band_t` (mui_map.h:80-150); do not add a parallel struct.

```c
typedef struct {
    uint8_t compact, band_n, extras, right_rows;   /* existing; extras becomes 0 in both forms, right_rows 1 */
    uint32_t art_w, art_h, height;                 /* existing; height becomes bar_h below */
    uint32_t btn_px;      /* new: square side of every icon-only bar button */
    uint32_t btn_gap;     /* new: spacing inside the transport group */
    uint32_t gap;         /* new: spacing between bar groups */
    uint32_t n_transport; /* new: 5 or 6 (Q2) */
    uint32_t show_left;   /* new: art + titles shown at this width */
} mui_player_band_t;

/* changed signature: width added; metrics come from mui_aa_metrics_aspect(font_px, aspect_q8) */
void mui_player_band(mui_player_band_t* out, uint32_t compact, int aspect_q8, uint32_t bar_w, uint32_t font_px);

/* new: the vertical centre (in rows, from the bar top) of a child of height h in the bar; host test asserts that the
 * button, the clock text, the seek track (h - track_h)/2 + track_h/2 and the volume track all return the same value +-1 */
uint32_t mui_player_centre_y(const mui_player_band_t* pb, uint32_t child_h);

/* new: the minimum bar width: transport + clock + MUI_SLIDER_MIN + length + speaker + volume + gaps; drives show_left and
 * the root minimum (I949: root minimum <= 600x220) */
uint32_t mui_player_min_width(const mui_player_band_t* pb);
```

Reuse: `mui_transport_row_width` keeps its job for the old tests but its row 0 sum becomes n_transport * btn_px plus gaps
(the labels no longer matter because every transport button is icon-only, `mui_transport_icon_only` returns 1 for all).
`mui_attr_fixwidth` keeps "00:00:00" for clock and length. `mui_player_band_compact_builds` stays the compact sentinel; add
`mui_player_bar_builds()` counting every `mui_player_group` construction of the one-row bar (sentinel for both forms).

## 5. Build rows (sonnet, in order)

Each row: edit, build host tests, run only the named tests (nice -n 19, --maxWorkers per project config), commit.
Tests live in tests/ui/mui_contract_test.c (pure map calls plus strstr checks on MUI_UI_SOURCE, pattern `test_screen_plan`),
registered in tests/CMakeLists.txt (add_test near 497-516, LABELS list near 618). Existing tests to keep green:
mui-player-bar, mui-compact-player, mui-transport-icons, mui-icon-buttons, mui-tight-pad, mui-transport-map, mui-weights.

**I973a: band sizes.** Extend `mui_player_band_t` and `mui_player_band` (section 4); add `mui_player_centre_y`,
`mui_player_min_width`, `mui_player_bar_builds`. Update all callers (grep `mui_player_band(`).
- Host test `mui-player-band-sizes`: at (640, 256 rows, aspect 512) btn_px equals the metrics formula, bar_h <= 36,
  art_h 28, extras 0, n_transport per Q2, `mui_player_min_width` <= 600; at (800x600, aspect 256) art_h 56, bar_h
  = max(btn_px, 56) + 8. Centre test: button, text and slider heights from metrics all give the same centre +-1.
- Sentinel: none yet (pure row).

**I973b: fixed square icon buttons.** In mui_aabtn_class.c `aabtn_askminmax` (109-120): when the button is icon-only
and the spec carries a fixed side (new spec field `fixed_px`, set from pb.btn_px), MinWidth = MaxWidth = DefWidth =
fixed_px and MinHeight = MaxHeight = fixed_px. Keep MUI_MAXMAX for labelled buttons (tool rows still stretch).
`mui_make_button_i` gains the fixed side. Weight 0 for fixed buttons.
- Host test `mui-icon-square`: strstr in the aabtn source for the fixed branch; a pure helper `mui_aabtn_minmax(spec,
  metrics, &minw, &maxw, &minh, &maxh)` extracted from askminmax so the host asserts maxw == minw == btn_px for icon-only
  fixed, maxw == MUI_MAXMAX for labelled.
- Sentinel: the extracted helper is what askminmax calls (strstr check that askminmax calls `mui_aabtn_minmax`).

**I973c: one-row tree.** Rewrite `mui_player_group` (mui_ui.c:1100-1166) to the section 3 tree. Remove colL/colC/colR,
row2..row5, the extras row call and MUI_PLAYER_W_* uses there. Clock text RIGHT-aligned, length LEFT-aligned (add an
align argument to `mui_make_text` or a `mui_make_text_al`; check `mui_make_text` callers first, do not change their
alignment). Increment `mui_player_bar_builds()` once per construction.
- Host test: extend `mui-player-bar`: strstr that MUI_UI_SOURCE no longer adds MUI_ATTR_REMAINING / MUI_ATTR_FORMAT /
  MUI_ATTR_STATUS / `mui_p8_extras_row` inside `mui_player_group`, and that it uses `pb.btn_px` and `MUIA_FixHeight`.
  Check the dirty-attribute update code (`mui_dirty_attrs` users) tolerates a NULL object for the dropped attributes.
- Sentinel: `mui_player_bar_builds()` > 0 after the reachability test opens the UI (row I973e).

**I973d: bar height.** Apply `pb.height` as MUIA_FixHeight on the bar panel `g`. Root minimum height check against the
I949 limit (root minimum <= 600x220 at the compact class).
- Host test `mui-compact-player` extended: band height <= MUI_PLAYER_COMPACT_MAX_H at the compact class; strstr for
  `MUIA_FixHeight, pb.height` (or the chosen spelling) in `mui_player_group`.

**I973e: reachability.** One test through the real entry (`mui_open` path used by mui-route-reach / mui-tiles-reach):
open at 640x256 bitmap and at 800x600 RTG, assert `mui_player_bar_builds()` incremented on each, and the band values
reached the tree (art_h 28 vs 56). If `mui_open` cannot run on the host, follow the existing reach tests' pattern exactly.
- Test name `mui-player-bar-reach`, LABELS mui.

**I973f: rig check (end of batch, one run).** Rig 2, `rig.py start --ram 256 --purpose "<session>: I973 MUI player bar
640x256 and 800x600"`, real song virgill-that_meme_again.mod, silent. Log lines "screen plan mode" and "pixel aspect" must
show the expected mode. Check: transport, time labels, seek track, speaker and volume share one centre line; transport
squares equal; no "Stopped"; art not clipped (log art _mtop/_mheight vs art_h for F7). Stop the rig 5 s after the last
command. Johan confirms on his screen (Amiga).

F7 (art clip) has no build row until the rig log shows the cause; I973f collects it.

## 6. Open questions for Johan

- Q1: The screenshot shows the non-compact tree (volume, extras, "Stopped" stacked right). Was it 640x256 native, or RTG
  800x600, and which build? The answer decides whether I949d's compact switch also has a fault.
- Q2: Stop button: the Mac has none. Drop it from the MUI bar (Recommended, matches the Mac; Stop stays in the menu and
  on a key) or keep 6 buttons?
- Q3: Remaining-time label and the status/format texts: drop them from the bar (Recommended, Mac has none)?
- Q4: Album and speed extras (chevrons, plus/minus): menu only at RTG too (Recommended; already menu only at 640x256)?
- Q5: Subsong label plus minus/plus at the left of the bar when the song has subsongs, as on the Mac (Recommended), or keep
  subsong only in the Cycle above the list?
- Q6: Heart beside the title: the Mac bar has none (it is on Now Playing). Drop it from the bar (Recommended) or keep it?
- Q7: Art + title/artist in the bar: keep it whenever it fits (Recommended, I949 asked for it at 640x256) or hide it below
  900 px like the Mac?

### 6a. Johan's answers (2026-11-02, AskUserQuestion)

- Q1: screenshot taken at 640x256. The tree in it is the non-compact one, which `mui_player_group` builds only when
  `mui_screen_compact()` returns 0, so the shot came from a pre-I949d build or a screen that was not 640x256. Not re-asked;
  the I973 rig check at 640x256 confirms the compact switch.
- Q2: drop Stop from the bar (menu and key keep it).
- Q3: drop the remaining-time label and the status/format texts.
- Q4: album and speed extras menu only, at RTG too.
- Q5: add the subsong label with minus/plus buttons at the left, as on the Mac, shown when the song has subsongs.
- Q6: drop the heart from the bar.
- Q7: hide art and title/artist below 900 px, like the Mac.

Build rows I973a-f follow these answers; where a row above still names a dropped element, the answer wins.
