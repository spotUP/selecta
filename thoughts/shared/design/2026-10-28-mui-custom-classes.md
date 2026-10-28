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
  notifications). Overrides Setup/Cleanup, AskMinMax, Draw, HandleEvent (click on track jumps, drag on knob).
- AAProgress: superclass MUIC_Area, attributes `AAP_Value`, `AAP_Max`, read only (gauge row of I663).
- Tokens: track TRACK 3A3A44, filled part ACCENT 3B82F6, knob TEXT C8C8D0 (hover HOVER_LIGHT E4E4EA), disabled fill
  TEXT_DISABLED.
- Sizes: track height 4 (S 2), radius 2, knob diameter 12 / 14 / 16 (S 8) drawn with `aa_round_fill` at radius d/2, min
  width 80, height = knob + 4.
- Host test: `mui-slider-geom`: value to knob x and x to value round trip for min, max, mid at each tier.
- Rig: volume track 3A3A44, fill 3B82F6, knob C8C8D0.

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
- Tokens: face SURFACE 1E1E24, outline DIVIDER 2C2C34, focus outline FOCUS_RING 93C5FD, placeholder TEXT_MUTED.
- Sizes: height as AAButton, inner pad 8, radius 6 (S 4).
- Host test: `mui-theme` extended: field role face and outline tokens.
- Rig: search field face 1E1E24, outline 2C2C34.

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

AACheck (4 pref rows) is built inside 3.1 as AAButton role `toggle`: 16 px rounded box, off face SURFACE_RAISED with
DIVIDER outline, on face ACCENT with a TEXT check icon.

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
