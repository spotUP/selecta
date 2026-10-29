---
date: 2026-10-28
author: spotUP
status: draft for Johan review (Opus design, sonnet agents build one class per ledger row)
implements: ledger I667 (custom class family), I662 (AA text by screen size), I663 (light parts left), I665 rollout; plan 2026-10-25-amiga-mui.md section "MUI theme (I648, in-code half)"
related: thoughts/shared/plans/2026-10-25-amiga-mui.md, plugins/ui/mui/mui_aabtn_class.c (I665 spike f980dcbb), plugins/ui/mui/map/mui_aa.c, plugins/ui/mui/map/mui_pens.c, plugins/ui/mui/map/mui_theme.c, include/selecta/uimodel/tokens.h
---

# MUI custom class family (I667)

Goal (Johan 2026-10-28): the MUI UI looks as close to the rich desktop UI as possible: exact token colours, flat rounded
controls, antialiased text, same spacing. Stock MUI 3.8 cannot draw this (prefs-owned pens, bevel frames, bitmap fonts,
`MUII_SelectedBack` pressed face). Every visible control therefore gets a private class built into the Selecta binary with
`MUI_CreateCustomClass`, all sharing one draw module. MUI 3.8 only (no MUI 4/5 calls). Nothing is installed in MUI:Libs.

The I665 spike (`Selecta_AAButton`, f980dcbb) is the reference: RTG rig 3 measured fill 2A2A32, caption peak C8C8D0,
rounded, no bevel. Each class below copies its structure (asm dispatcher stub, per-object data, Setup obtains pens,
Cleanup releases them, Draw only in MADF_DRAWOBJECT/MADF_DRAWUPDATE).

## 1. Inventory

Counted from the tree builders in `plugins/ui/mui/mui_ui.c` (first count) and verified from the tables that feed them
(second count): `SEL_UIM_DOOR_COUNT` in `include/selecta/uimodel/doors.h` (7), `mui_tool_labels[MUI_TOOL_BTNS]` in
`map/mui_layout.c` (11), `k_extras[MUI_EXTRA_COUNT]` in `map/mui_p8.c` (6), `MUI_BTN_COUNT` (6), `MUI_CRUMB_MAX` (6),
`g_pref_rows[]` in `src/uimodel/features.c` (9 rows: 2 cycle, 4 check, 1 slider, 2 string).

| Control | Stock class now | Instances | Source of count |
|---|---|---|---|
| Button (icon + label, or icon only) | Group + Text + Bodychunk | 36: doors 7, transport 6 (Play is AAButton), extras 6, toolbar 11, crumbs 6 | tables above |
| Button still missing | - | +1 New Playlist (I661), +2 library prev/next (I670) | ledger |
| Slider | MUIC_Slider | 3: volume, seek, Stereo Width | builder + g_pref_rows |
| Cycle | MUIC_Cycle | 4: rating, subsong, Loudness, Crossfeed | builder + g_pref_rows |
| Check | MUIC_Image MUII_CheckMark | 4: Power LED Filter, Prefetch, Metered, Overscan | g_pref_rows |
| String | MUIC_String | 4: search, command line, Title Format, Status Format | builder + g_pref_rows |
| List | MUIC_List in Listview | 4 lists + 1 Floattext: main (Selecta_Listview), info, tracks, help; lyrics | builder |
| Text | MUIC_Text | about 27 (7 frame texts, status, crumb, state, suggestion, 3 now playing, Next in Queue, 3 up next, 9 pref labels) | builder only; second count open, the AAText row recounts from the builder |
| Gauge / progress | MUIC_Gauge (I663 list) | 1 | builder |
| Group panels | MUIC_Group with TH PANEL | sidebar, main, detail, player, settings | builder |
| Balance | BalanceObject | 2 | builder |
| Scrollbars | inside each Listview | 5 | follows lists |

Not replaced: menus (Intuition menus, drawn by the screen's DrawInfo pens, already themed by I666 SA_Pens),
`MUI_Request` confirms (stock, themed by window background only), Bodychunk icons (kept, drawn inside the classes).

## 2. Shared base: `mui_aa_draw`

One module, two halves, same split as the rest of the MUI port:

- `plugins/ui/mui/map/mui_aa.{c,h}` (pure C, host tested, exists): geometry and colour maths. Add:
  - `mui_aa_state_rgb(face, state)`: face colour for `MUI_AA_NORMAL`, `HOVER`, `PRESSED`, `DISABLED`, `SELECTED`. Normal =
    the role token, hover = SURFACE_HOVER 34343E, pressed = SURFACE_PRESSED 3E3E4A, selected = SURFACE_PRESSED, disabled =
    the normal face with text TEXT_DISABLED 62626E. Replaces `mui_aa_pressed_rgb` (keep it as a call to this).
  - `mui_aa_ramp_pens(tier, fg, bg, out[])`: the coverage ramp for a tier (section 4), as RGB values.
  - `mui_aa_font_px(screen_rows, screen_cols)`: 13 px up to 600 rows, 15 up to 800, 17 above; 0 (bitmap topaz 8) for
    640x256 and 320x256 (I662 decision).
  - `mui_aa_metrics(font_px)`: control heights and paddings of section 3 from one table.
- `plugins/ui/mui/mui_aa_draw.{c,h}` (Amiga only, new, extracted from `mui_aabtn_class.c`, not a copy of it):
  - `aa_ctx`: per object state: ColorMap, tier, held pens, ttengine font handle, font px.
  - `aa_setup(obj, aa_ctx*, const uint32_t* rgb, n)`: ObtainBestPenA with OBP_Precision PRECISION_GUI for each RGB,
    keeps the list; on the SELECTA public screen (I666) the token pens resolve to the exact screen pens, so most obtains
    share. `aa_cleanup` releases every held pen with ReleasePen.
  - `aa_round_fill(rp, x, y, w, h, radius, pen)`: rows of RectFill with `mui_aa_round_inset` (spike code moved here). At
    radius 0 one RectFill.
  - `aa_round_frame(rp, ..., pen)`: 1 px rounded outline (focus ring, field outline), same inset maths.
  - `aa_text(ctx, rp, x, y, str, fg_rgb, bg_rgb)` and `aa_text_width(ctx, str)`: AA text with three backends, picked once
    in Setup: TC (depth 15 or more, ttengine TT_Text with TTE_Antialias on, TTE_Foreground/TTE_Background as RGB),
    PAL (palette screen, TT_GetPixmap 8-bit coverage drawn as runs through the ramp pens), PLAIN (no ttengine or no font:
    graphics Text with the MUI prefs font in the text pen).
  - Font: ttengine.library v6 or newer (7.x tested), Inter-Regular.ttf searched in `RAM:`, `Selecta:Fonts/`, `FONTS:`
    (spike order). One process wide font opened per px size, reference counted, closed at the last Cleanup.
  - Dependency and fallback: ttengine is freeware without a redistribution grant, so it is a user-installed dependency
    (kit install item, I662). Missing library or font: PLAIN backend and one log line
    `selecta: mui: ttengine or Inter missing, plain text` per run (not per object).
- Token reading: classes never hold colour literals. Each class takes its RGB values from `txt_theme_default()` through
  `mui_theme_look_for` (role colours) and `sel_gui_token` IDs named in section 3; `mui_ui.c` passes them as init
  attributes (as `mui_aabtn_new(caption, widest, face, text, id)` does now).
- States shared by every interactive class: hover (IDCMP_MOUSEMOVE inside the box via MUIM_HandleEvent with
  IDCMP_MOUSEMOVE requested only while the window is active), pressed (button down inside), disabled (MUIA_Disabled, MUI
  sets it; draw TEXT_DISABLED, no hover), focus (MUIA_CycleChain object active: 1 px rounded FOCUS_RING 93C5FD outline at
  inset 1). Redraw on state change with MUI_Redraw(obj, MADF_DRAWUPDATE), never the whole window.

Host test: `mui-aa-state` (new mode of `mui_aa_test`): each state returns the named token, disabled text is
TEXT_DISABLED, and `mui_aa_font_px(600,800)` = 13, `(800,1280)` = 15, `(1024,1280)` = 17, `(256,640)` = 0.

## 3. Classes, in build order

Pixel sizes per screen tier (from `mui_aa_metrics`, font px f): tier S = 640x256/320x256 (topaz 8, f = 8), tier M = up to
600 rows (f 13), tier L = up to 800 rows (f 15), tier XL above (f 17). Radius is RADIUS 6 except tier S (RADIUS_SMALL 4).

Rig pixel checks: RTG on rig 3 (UAE RTG, depth 8 SELECTA screen and a depth 16 run for the TC backend), AGA on the AGA rig
(depth 4). A check reads the screen pixel through the amiga MCP and compares to the hex; pass = within 1 step per channel on
RTG and TC; on AGA within the 12-bit-to-8-bit rounding of the exact SA_Colors32 value (the same hex, since I666 loads
32-bit colours).

### 3.1 AAButton rollout (I662 family; extends the I665 class)

- Superclass MUIC_Area (as spike). Methods: OM_NEW/OM_SET/OM_GET, MUIM_Setup/Cleanup, MUIM_AskMinMax, MUIM_Draw,
  MUIM_HandleEvent (replaces RelVerify: own press/hover tracking; on release inside, set `MUIA_Pressed` FALSE so existing
  notifications in `mui_ui.c` keep working unchanged).
- Attributes: `AAA_Caption` (string, settable), `AAA_Icon` (index into the baked Lucide set, drawn with the existing
  Bodychunk body through BltMaskBitMapRastPort, or none), `AAA_IconOnly`, `AAA_Widest` (width hint), `AAA_Role`
  (normal/accent/ghost), `AAA_Id`.
- Tokens: face SURFACE_RAISED 2A2A32, hover SURFACE_HOVER 34343E, pressed SURFACE_PRESSED 3E3E4A, caption TEXT C8C8D0,
  disabled caption TEXT_DISABLED 62626E, focus FOCUS_RING 93C5FD; accent role (Play, New Playlist): face ACCENT 3B82F6,
  caption ACCENT_TEXT E4E4EA; ghost role (sidebar doors, crumbs): face = parent panel PANEL 141418, selected door face
  SURFACE_RAISED.
- Sizes: padding MUI_BTN_PAD_X 10 / MUI_BTN_PAD_Y 6 (S: 6/3), icon MUI_ICON_PX 24 (S: 16), icon gap 6, height
  f + 2 * pad_y with a 24 px icon floor: S 14, M 36, L 36, XL 36. Radius 6 (S 4).
- Replaces `mui_make_button_i` body: one constructor, every button family above in one row (doors, transport, extras,
  toolbar, crumbs, New Playlist). Play/pause page group becomes one AAButton with `AAA_Icon` switched.
- Host test: `mui-aabtn-tokens`: for each role and state the face and caption RGB, and `mui_contract_test icon-buttons`
  extended so every button the builder makes reports the AAButton class (no stock Group button left; count 36).
- Rig: RTG and AGA, Next button at rest: face 2A2A32, caption peak C8C8D0; hover 34343E; Play face 3B82F6.

### 3.2 AASlider and AAProgress (slider, seek, gauge)

- AASlider: superclass MUIC_Numeric (keeps MUIA_Numeric_Value/Min/Max, MUIM_Numeric_Increase/Decrease keys and existing
  notifications). Overrides Setup/Cleanup, AskMinMax, Draw, HandleEvent (click on track jumps, drag anywhere on the track).
- AAProgress: superclass MUIC_Area, attributes `AAP_Value`, `AAP_Max`, read only (gauge row of I663).
- Tokens: track TRACK 3A3A44, filled part ACCENT 3B82F6, disabled fill TEXT_DISABLED. No knob (owner decision
  2026-10-29): only the track and the fill are drawn, like the Mac desktop seek bar.
- Sizes: track height 4 (S 2), radius 2 drawn with `aa_round_fill`, min width 80, hit height 12 / 16 / 18 / 20 (S 12). An update draw (MADF_DRAWUPDATE)
  first erases the full box with MUIM_DrawBackground (plan op 0), else earlier fills stay visible.
- Host test: `mui-slider-geom`: fill width and x to value for min, max, mid; update plan starts with the full-box erase and has no knob op.
- Rig: volume track 3A3A44, fill 3B82F6, no knob.

### 3.3 AACycle and popup

- Superclass MUIC_Area. Attributes `AAC_Entries` (STRPTR array), `AAC_Active` (notifies like MUIA_Cycle_Active, so the
  pref and rating code keeps its notifications). Methods: Setup/Cleanup, AskMinMax (widest entry + chevron), Draw,
  HandleEvent (click opens popup; Up/Down keys step without popup when focused).
- Popup: a borderless MUI window (`MUIA_Window_Borderless`, `MUIA_Window_DragBar` FALSE, placed under the object with
  `MUIA_Window_LeftEdge/TopEdge`) holding one AAList (3.6) with the entries; pick or Escape closes it.
- Tokens: face SURFACE_RAISED, value text TEXT, chevron TEXT_MUTED A0A0AC, popup background SURFACE 1E1E24, popup row
  hover SURFACE_HOVER.
- Sizes: as AAButton, chevron 8 px wide, gap 8.
- Host test: `mui-cycle-step`: Up/Down wrap and AAC_Active range for 2, 3 and 6 entries.
- Rig: Loudness cycle face 2A2A32, chevron A0A0AC.

### 3.4 AAField (string)

- Rewriting String editing is out of scope. AAField is a MUIC_Group subclass holding one stock MUIC_String with
  `MUIA_Frame` MUIV_Frame_None and the field background spec; AAField Draw paints the rounded field face and outline
  before DoSuperMethod. Text inside the gadget stays the MUI prefs font and pens (limit, section 4).
- Tokens: face SURFACE_RAISED 2A2A32, outline TRACK 3A3A44, focus outline FOCUS_RING 93C5FD, placeholder TEXT_MUTED.
  (I667h2, Johan 2026-10-29 rig 2: "its hard to see the text fields, their color is very similiar to the bg color".
  The first build used face SURFACE 1E1E24, the same colour as the SURFACE panels the fields sit on.)
- Sizes: height as AAButton, inner pad 8, radius 6 (S 4).
- Host test: `mui-theme` extended: field role face and outline tokens.
- Rig: search field face 2A2A32, outline 3A3A44.

### 3.5 AAScroller

- Superclass MUIC_Area with Prop-like attributes `AAS_First`, `AAS_Visible`, `AAS_Entries` (notify on First). AAList owns
  it, so no stock Prop gadget is drawn. Builder confirms the MUI 3.8 header has the List attributes used for wiring
  (MUIA_List_First, MUIA_List_Visible, MUIA_List_Entries, MUIM_List_Jump); if one is missing, AAList (3.6) wires its own
  scroller from its own row model, which it holds anyway.
- Tokens: track SURFACE (or transparent on the list face), thumb TRACK 3A3A44, hover thumb SURFACE_PRESSED 3E3E4A. No
  arrow buttons (removes the light arrow box of I663).
- Sizes: width 10 (S 6), thumb radius 4 (S 2), thumb min height 20.
- Host test: `mui-scroller-geom`: thumb y and height for first/visible/entries, clamp at ends.
- Rig: thumb 3A3A44 on 1E1E24.

### 3.6 AAList

- Superclass MUIC_Area, own row model (virtual list: count + row callback into the existing display hook data, so
  `mui_col_format_themed` and the column table stay the source of the texts). Replaces MUIC_List inside Selecta_Listview
  for main, info, tracks and help lists; the Amiga-1..5 song menu moves to AAList HandleEvent. Lyrics Floattext becomes
  AAList with one column and no selection.
- Methods: Setup/Cleanup, AskMinMax, Draw (only visible rows; MADF_DRAWUPDATE redraws changed rows), HandleEvent
  (click, double click, wheel, keys Up/Down/PageUp/PageDown/Home/End, Return activates).
- Attributes: `AAL_Count`, `AAL_Active` (notify), `AAL_First` (notify), `AAL_Columns`, `AAL_RowFn`, `AAL_Playing`.
- Tokens: list face SURFACE 1E1E24, row text TEXT, secondary columns TEXT_MUTED A0A0AC, hover row SURFACE_HOVER 34343E,
  selected row SURFACE_PRESSED 3E3E4A (replaces the blue-grey MUI selection of I663; desktop uses the same pair in
  `desktop_ui.c` st.hover/st.pressed), playing row title ACCENT 3B82F6, header text TEXT_MUTED, header divider DIVIDER.
- Sizes: row height f + 8 (M 21, L 23, XL 25, S 10), column gap SPACING 8, left pad 10.
- Host test: `mui-list-rows`: first/visible/active clamping and key moves for 0, 1, 100 rows.
- Rig: selected row 3E3E4A, its title C8C8D0; playing title 3B82F6.

### 3.7 AAText

- Superclass MUIC_Area. Attributes `AAT_Contents` (settable, redraw only on change), `AAT_Tone` (normal TEXT, muted
  TEXT_MUTED, title: f + 4 px), `AAT_Align`, `AAT_Ellipsis` (cut with "..." at the width). Replaces every MUIC_Text in
  the inventory; pen escapes from `g_pre` are dropped (tone attribute instead).
- Host test: `mui-text-fit`: ellipsis cut on a fixed advance table (host side uses `mui_button_width` metrics).
- Rig: title text peak C8C8D0 on panel 141418.

### 3.8 AAPanel (rounded panel groups)

- Superclass MUIC_Group. Attribute `AAG_Role` (panel, surface, raised). Draw paints the rounded face with `aa_round_fill`
  over the parent background, then DoSuperMethod draws children. Replaces TH PANEL groups and the 64x64 art frame.
- Tokens: panel PANEL 141418, surface SURFACE 1E1E24, window behind BACKGROUND 121214, header strip HEADER_TINT 2B2350.
- Sizes: radius RADIUS_LARGE 8 (S 4), inner spacing SPACING 8, outer gap 8.
- Host test: `mui-theme` extended: panel roles map to the tokens.
- Rig: sidebar 141418 with corner pixel 121214 at (0,0) of the panel box.

### 3.9 AATabs

- Superclass MUIC_Group of AAButton (ghost role) plus a 2 px underline. Attribute `AATab_Active` (notify). Used by the
  settings window sections and the detail pane (lyrics/info/tracks) instead of Register groups.
- Tokens: tab text TEXT_MUTED, active tab text TEXT, underline ACCENT 3B82F6.
- Host test: `mui-tabs`: active index set by click and by keys.
- Rig: active underline 3B82F6.

### 3.10 AABalance (pane dividers)

Johan 2026-10-29 on rig 2: "the drag bar doesnt look modern yet". The two stock BalanceObjects between sidebar, main and
detail (mui_ui.c window build, `s->balance[0..1]`) draw the MUI prefs 3D bar.

- Superclass MUIC_Balance (`MUI_CreateCustomClass(NULL, MUIC_Balance, ...)`), so dragging, the resize maths and the
  weights stay MUI's. Overrides AskMinMax and Draw only. Draw with MADF_DRAWOBJECT paints the full box in the window
  BACKGROUND, then a centred vertical line; it never calls DoSuperMethod for the draw, so no prefs frame or 3D bar shows.
  `MUIA_Frame` MUIV_Frame_None at create.
- Tokens: box BACKGROUND 121214 (the gap colour between AAPanels, 3.8), line DIVIDER 2C2C34. No hover colour: MUI 3.8
  Balance gives no hover event without an extra IDCMP handler, and the desktop handle is equally quiet.
- Sizes: box width = outer gap 8 (S 4), so the divider is the panel gap itself; line 1 px, inset 8 px top and bottom (S 4).
- Pure helper in map/mui_aa.c: `mui_aa_balance_line(box_w, box_h, small, out rect)` gives the line rect; host test
  `mui-balance-geom`: line x centred, inset, clamp for boxes shorter than two insets. Sentinel `mui_aabal_made()` equals 2
  after the window build (counted from the builder, like `mui_aabtn_made`).
- Fallback if the subclass cannot be made on MUI 3.8 (class not public): stock BalanceObject with `MUIA_Frame`
  MUIV_Frame_None and `MUIA_Background` the BACKGROUND pen spec; report it, do not ship a 3D bar.
- Rig: a pixel in the divider box 121214, a pixel on its centre line 2C2C34; no light pixels (above 80 grey) in the box.

AACheck (4 pref rows) is built inside 3.1 as AAButton role `toggle`: 16 px rounded box as the desktop d_checkbox
(I667b-3): radius RADIUS_SMALL, corners SURFACE, off face SURFACE with a TEXT_MUTED outline, on face ACCENT with an ACCENT_TEXT tick.

### 3.11 Icon size follows the text size (I667m)

Johan 2026-10-29 (Amiga rig 2): "the icon vs text sizes in the mui ui are off the icons looks big and the texts small
compare to the mac ui".

- Cause (measured): `mui_aa_metrics` (map/mui_aa.c) sets `icon_px` to 24 on every tier except S (16), while
  `mui_aa_font_px` gives 13 px text on a 600-row screen. Icon:text is 1.85. The desktop UI draws 16-20 px icons next to
  13-14 px text (desktop_ui.c: draw_widget 16, draw_nav_arrows 18, tl_draw 18, draw_topbar 20, draw_transport 20;
  draw_text px counts: 13 x36, 14 x35, 12 x23). Desktop icon:text is about 1.4.
- Rule: `icon_px` comes from the tier, in one table in `mui_aa_metrics`: S 12 (font 8), M 18 (font 13), L 20 (font 15),
  XL 24 (font 17). The button height formula stays `max(font, icon) + 2 * pad_y`, so M buttons shrink from 36 to 30.
- Icon bodies: `aabtn_icon` resamples the 24 px body by nearest neighbour; 24 to 18 drops rows and columns and breaks
  1 px Lucide strokes. Bake one body set per tier size instead: `tools/gui_iconbake.c` emits bodies for 12, 16, 18, 20
  and 24 px (`mui_icon_body(icon, px)`, about 25 KB for the whole set), and `aabtn_icon` picks the exact size. Resampling
  stays only as the fallback for a size with no body. `MUI_ICON_PX` in map/mui_icons.h and its users in map/mui_screen.c
  and map/mui_icons.c take the tier size from `mui_aa_metrics` instead of the constant; the contract check
  `mui_icon_body_px == MUI_ICON_PX` becomes "a body exists for every tier icon size".
- AACheck `check_px` and the AASlider knob stay as they are (they follow the font already).
- Text: 13 px on 600 rows is already a larger share of the screen than 13-14 px in the Mac window, so the text size
  stays. Johan's "texts small" is the ratio. The rig look decides; if text still reads small next to 18 px icons, the
  next step is M font 14, not bigger icons.
- Host test `mui-icon-ratio`: for each tier, icon_px / font_px is between 1.3 and 1.5 (S excepted: 12 / 8), and
  `mui_icon_body(icon, icon_px)` is not NULL for every icon id. Sentinel: the bodies drawn count by size, so a test sees
  the 18 px body used on tier M.
- Rig 2: toolbar and transport icons 18 px tall at 13 px text; Johan look.

## 4. Screen limits

AGA (depth 4, 16 pens, SELECTA screen from I666):

| Pen | Use |
|---|---|
| 0-6 | the 7 `mui_pens` slots (BACKGROUND, TEXT, SURFACE, RAISED, PANEL, ACCENT, DIVIDER), exact |
| 7 | TEXT_MUTED A0A0AC |
| 8 | TEXT_DISABLED 62626E |
| 9 | TRACK 3A3A44 |
| 10 | SURFACE_HOVER 34343E |
| 11 | SURFACE_PRESSED 3E3E4A |
| 12 | FOCUS_RING 93C5FD |
| 13-15 | icon pens (`mui_icon_palette`, 3 opaque) |

`mui_pens` grows from 7 to 13 slots (MUI_PEN_COUNT 13) so all of these are loaded with SA_Colors32 and held exact.
MUI itself obtains its prefs pens with ObtainBestPen and shares the nearest of these. HOVER_LIGHT, ACCENT_TEXT, HEADER_TINT
fall back to the nearest held pen on AGA (TEXT, TEXT, PANEL).

AA on AGA: no per-face 8-step ramp (16 x 2 pens per face would exhaust the palette). The PAL backend uses a 4-level ramp
built from held token pens: coverage 0-63 face, 64-127 TEXT_DISABLED (about 35 percent text on RAISED), 128-191
TEXT_MUTED (about 75 percent), 192-255 TEXT. Zero extra pens. `mui_aa_ramp_pens(AGA, ...)` returns these; host test
`mui-aa-ramp` asserts the 4 pens and their order.

RTG (depth 8 SELECTA screen): 256 pens, PAL backend with the 8-step ramp per (fg, face) pair through ObtainBestPen
(spike). Depth 15 or more (RTG truecolour screen or Workbench fallback): TC backend, no ramp pens.

Workbench fallback (SELECTA screen failed): pens are shared, ObtainBestPen gives nearest; the contrast gate (section 5)
decides per role, AA falls to PLAIN when a ramp pen is further than 16 steps from its target.

## 5. Contrast gate

Every class asks `mui_theme_readable(fg, bg)` (linear sRGB luminance, tenths, pass at 40 or more, 76107bae) for its text
on its face in Setup, against the RGB of the pens actually obtained (not the token values). When it fails, the class uses
`mui_theme_pick_pen` to find the obtained pen nearest TEXT that is readable; when none is readable it draws with the MUI
prefs text pen and logs once. Host test: `mui-theme` covers the maths; the classes add no new maths.

## 6. Build order (proposed ledger rows, one class each)

- I667a: shared base `mui_aa_draw` + `mui_aa` state, metrics, font px, ramp (sections 2, 4, 5); AAButton spike moved
  onto it with no visible change.
- I667b: AAButton rollout to all 36 buttons + AACheck + New Playlist (3.1; closes I661 and the I665 rollout).
- I667c: `mui_pens` 13 slots and AGA pen table (section 4); AGA rig pixels (needs I671 fixed).
- I667d: AAText (3.7) with font by screen size (closes I662 text half).
- I667e: AAPanel (3.8) rounded panel groups.
- I667f: AASlider and AAProgress (3.2).
- I667g: AACycle and popup (3.3).
- I667h: AAField (3.4).
- I667i: AAList and AAScroller (3.6, 3.5; closes I662 AAList and the I663 list parts).
- I667j: AATabs (3.9).
- I667l: AABalance pane dividers (3.10).
- I667k: rig pass RTG and AGA, every hex in section 3, Johan look (one rig run per batch at the end).

## 7. Open points

- MUI 3.8 header names in 3.5 (List attributes) are unverified here; the 3.5/3.6 builder checks them first.
- AAField text is stock MUI (no AA inside String gadgets).
- Window title bar and borders stay DrawInfo pens (I666 SA_Pens), not drawn by these classes.

## I671 findings (2026-10-28, rig 2 AGA 640x256 depth 4, stopped at the tool-call budget)

Johan on rig 2, verbatim: first requester "the mui window could not open, using text ui", then after OK: "the ui could not open, no terminal".

- MUI is installed and complete on rig 2: LIBS:muimaster.library 19.35 (MUI 3.8, 02/12/97), assign MUI: = System:MUI, MUI:Libs holds the classes, MUI.key is in SYS:S/. Not an install problem.
- The second requester is the shell's text UI fallback. It needs a console; started with Run >NIL: (or from an icon) it has none, so the fallback is a dead end. Separate row proposed (I671b).
- The two MUI_FAIL_NO_WINDOW sites in mui_open now pass distinct details. The rig log of the fixed build reads "the MUI window could not open. Using the text UI. (the layout does not fit the screen)": ApplicationObject is created; MUIA_Window_Open reads back 0. Same class of failure as I656 (window minimum larger than the screen, there on width at 800).
- Stack size is not the cause: with Stack 200000 the failure is the same (the default Run stack also overflows the canary, which is a separate warning).
- Fix applied (necessary, NOT sufficient): mui_open applied the 1280x720 three-column layout only after the open, so the open measured sidebar and detail too. It now applies the layout of the screen it opens on before MUIA_Window_Open (mui_layout_for_screen; 640x256 maps to the mini-suggestion breakpoint, sidebar and detail hidden). Host regression: mui_contract_test icon-rows (test_layout_before_open). On the rig the window still does not open with sidebar and detail hidden.
- Remaining cause, unverified: the minimum HEIGHT of main column plus player group exceeds 256 (minus the Workbench title bar, 242 usable). Width rows are proven <= 640 by test_icon_button_rows; nothing models height. Next step: a height model in mui_map (player rows + toolbar rows + list minimum + suggestion banner, 24 px icon rows) checked against 242, then a compact player for screens under ~400 high, or an own SELECTA screen in a taller mode (PAL interlace 640x512) on AGA.

### I671a (2026-10-28, second pass, rig 2 AGA): root cause confirmed, fixed, window up

Rig 2 facts: Workbench mode 0x00029000 (PAL hires, not laced) 640x256 depth 4 (`build/rig2/vtc/screenmode.aga16.prefs`); the own SELECTA screen inherited that mode, so the window had 256 px (242 under the title bar) to fit into.

- Root cause: height. The mini layout (640 px breakpoint, sidebar and detail hidden) stacks the tool group (string row, two 40 px icon button rows, framed status text), the list (title plus two rows) and the player (three 40 px button rows, seek slider, title row). An icon button is MUI_ICON_PX 24 + 2 x MUI_BTN_PAD_Y 6 + MUI_BTN_FRAME 4 = 40 px high; five of them alone are 200 px. The host model `mui_layout_min_height()` (plugins/ui/mui/map/mui_screen.c) sums the rows from the builder constants to 334 px: taller than 256, inside 512. MUI 3.8 refuses a window whose minimum does not fit its screen (MUIA_Window_Open reads back 0), the same class of failure as I656 on width.
- Fix (root, not a workaround): `mui_screen_plan()` (same file) plans the own screen from the Workbench mode, size, DIPF_IS_FOREIGN flag and the model height. A native mode that is too short and not yet laced opens in its interlaced variant (mode | 0x4, height doubled: 0x29000 640x256 becomes 0x29004 640x512); a laced, tall enough or RTG mode is kept (RTG keeps depth 8). `mui_screen_open` (mui_ui.c) applies the plan, checks `ModeNotAvailable()` on the laced id (falls back to the Workbench mode with a log line), and logs "selecta mui: screen plan mode ... (layout needs N px high, Workbench is H)". After the open attempt `mui_open` logs "selecta mui: window minimum WxH, screen WxH, open N" (`_minwidth`/`_minheight` of the root group) and the failure requester now names the measured minimum instead of the fixed text. Design goal kept: full MUI layout, full screen (D-M4-3), MUI 3.8 only.
- Host regression: `mui_contract_test screen-plan` (ctest `mui-screen-plan`, label mui, tests/ui/mui_contract_test.c `test_screen_plan`): model 256 < h <= 512, the four plan cases, sentinels `mui_screen_plans`/`mui_screen_laced`, and the wiring strings in mui_ui.c. Revert proof: with main's mui_ui.c in place the test fails on the three wiring checks (lines 996-998), with the fix it passes.
- Rig proof (rig 2 AGA, Kickstart 40.68, MUI 3.8, `Run >NIL: Execute RAM:mui.script` = `Stack 200000` + `Selecta --ui mui --output paula --start-dir RAM: --exec "QUEUE_ADD RAM:meme.mod" --exec PLAY --log RAM:mui.log`, meme.mod = virgill-that_meme_again.mod): wasabi screen walk reads `screen "Selecta" 640x512 depth 4 | window "Selecta" 640x512 at 0,0` next to `Workbench Screen 640x256 depth 4`; tests/amiga/mui_list_rows.py prints the RAM: rows (Clipboards, ENV, T, meme.mod) from the Selecta List. The window opens full screen on AGA.
- Open risks: interlace flickers on a real PAL monitor without a flicker fixer (the AGA default here is already a 640x256 Workbench, so the laced own screen is the only full-layout option at 4 planes; a compact player for short screens stays a possible follow-up). NTSC 640x200 laced gives 400 px, enough for the 334 px model but with little slack. The Workbench-screen fallback (own screen failed to open) is unchanged and would still hit the 256 px refusal; its requester now says the measured minimum. The model constants (MUI_STRING_H 14, MUI_TEXT_FRAMED_H 12, MUI_SLIDER_H 12, MUI_GROUP_SPACING 4, MUI_WIN_CHROME_H 24, MUI_LIST_MIN_ROWS 2) are topaz 8 estimates; the measured `window minimum` log line is the calibration source (see the rig numbers below).
- Measured on rig 2 (RAM:mui.log of the fixed build, c4ee98f9): `screen plan mode 00029004 640x512 depth 4 (layout needs 334 px high, Workbench is 256)` and `window minimum 517x314, screen 640x512, open 1`. The root group's minimum is 517x314 px (without the window title bar and borders, which the model counts as MUI_WIN_CHROME_H 24: model 310 + 24 = 334 against measured 314 + chrome). Height 314 > 256 is the refusal MUI 3.8 made on the Workbench-mode screen; width 517 fits 640. Model and measurement agree within 4 px; no recalibration needed. Selecta ran for three minutes with the song queued and `arexx_port=SELECTA` in the stats, then stopped on `Break`.

## I672 (2026-10-28, rig 2 AGA): MUI text is not white, it is the TEXT token

Johan on rig 2, verbatim: "can we make the texts slightly toned down, they are white make them light grey".

- Measured, not guessed: rig 2 AGA, own SELECTA screen 640x512 depth 4 (mode 0x29004 from I671a), build of main 35581baa, `Run >NIL: Execute RAM:mui.script` with meme.mod (virgill-that_meme_again.mod) queued and playing. Full-screen grab histogram: 1E1E24 153021 px, 2A2A32 110890, 121214 34964, 141418 12719, 2C2C34 8824, C8C8D0 7105, 93939B 78, 5E5E66 79. Zero FFFFFF pixels. Every light pixel band, row by row from y 1 to y 498 (window title, string gadget, button captions, list rows, player labels), is C8C8D0 = TEXT exactly; the only other light colour is 93939B (anti-aliased icon edges, about 1 percent).
- So none of the suspects holds on this rig: MUI 3.8 default pen specs (rig 2 has no ENV:MUI prefs for Selecta) resolve to the DrawInfo pens that mui_pens.c sets (TEXT at slot 1), the depth 4 palette load (SA_Colors32, 7 slots) lands exactly, and no label uses a fixed white pen. The pen wiring matches the design (section 4); there is no pen bug to fix.
- What Johan sees as white is C8C8D0 on 121214/1E1E24 (contrast about 11:1) drawn by topaz 8 in one flat pen with no anti-aliasing. The desktop UI draws the same TEXT token only for primary text (titles, selected rows) and TEXT_MUTED A0A0AC for everything secondary (artists, hints, section labels, durations; about 20 sites in plugins/ui/desktop/desktop_ui.c). The MUI port has one text pen, so every label is primary-bright; that is the visible difference from the rich UI.
- Not changed (decision for Johan): (a) give MUI a TEXT_MUTED pen slot (design section 4 already lists pen 7 TEXT_MUTED) and draw secondary MUI text in it, matching the desktop per label; or (b) lower the TEXT token itself, which changes the desktop and every golden. (a) follows the "MUI matches rich UI" goal; (b) is what the request says literally. Both need a pen budget check on depth 4 (7 of 16 pens used today).
