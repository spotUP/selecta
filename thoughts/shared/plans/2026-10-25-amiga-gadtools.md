---
date: 2026-10-25
author: spotUP
status: draft, waiting for review (section 7 questions)
milestone: M4 Phase 10 (GadTools), moved here from the M4 plan
implements: M4 plan section 5 (GadTools column), M4 Phase 10 (M4-P10-1..3), section 3.4 display modes, Q10, Q24-Q27, 8a; ledger row I344
related: thoughts/shared/plans/2026-10-24-m4-amiga-ui.md, thoughts/shared/plans/2026-10-25-amiga-mui.md, thoughts/shared/plans/2026-10-24-desktop-ui.md, thoughts/shared/research/2026-10-22_parity-hippoplayer.md, thoughts/shared/research/2026-10-22_parity-bbmp.md
---

# Amiga GadTools UI plugin (`plugins/ui/gadtools/`)

Sibling of the MUI plan (`2026-10-25-amiga-mui.md`). Shared findings C1-C6 are not rediscovered here; section 6 lists them by reference and adds C7-C10.

## 1. Goal and finish line
`selecta --ui gadtools` opens an Intuition window with GadTools gadgets on an OS 3.0+ Amiga and carries every desktop-UI feature that GadTools can carry, with no private logic in the plugin. The plugin consumes `src/uimodel/` (frame strings, action table, layout rectangles, display modes, budget) and `src/gui/draw` (custom-drawn areas only: viz, planar art). Done means all items in section 5 tick, each with a ctest or a rig test that drives the top-level entry point.

Finish line, measurable:
1. `selecta --ui gadtools` on the rig `a1200-aga-020` (68020, AGA, 2 MB chip plus fast RAM) opens a window covering the Workbench screen area below the title bar; the sentinel stat `ui_open_gadtools=1` is read back (GT-P1-5). This is the one reachability test.
   Decision (Johan 2026-10-26): OS 3.0 with gadtools.library 39, as recommended.
2. NewMenu entry count equals the count of `sel_uim_action_at` rows with a non-empty `menu` path (GT-P2-3).
   Decision (Johan 2026-10-26): custom-drawn info strip, as recommended.
3. Gadget rectangles equal the rectangles solved by `sel_uim_layout_compute` for the Workbench size; they are recomputed on IDCMP_NEWSIZE with no gadget leaked (GT-P2-1, GT-P2-2).
   Decision (Johan 2026-10-26): drag reorder (not the recommended answer). The queue list supports drag reorder of rows; this needs the custom list gadget of Q4 (GT-P3-9) and amends GT-P4-4.
4. 640x256 PAL hires is fully laid out in the compact one-column layout with the bottom icon row (Home, Library, Search, Queue, Now Playing) above the transport; 640x200 scrolls; below 640 px wide the plugin refuses and the shell keeps the text UI (Q24, Q26; GT-P6-2, GT-P6-3).
   Decision (Johan 2026-10-26): custom-drawn list with true columns (not the recommended answer). LISTVIEW_KIND is replaced by one custom list gadget `gt_list` (GT-P3-9) with real columns from the F48 set, proportional font support, its own keyboard handling and drag reorder; GT-P3-1, GT-P3-4 and GT-P3-8 are amended to it.
5. Offered view styles are exactly 4 on every tier: regular, tree, drilldown, compact. Grid, cover flow and artwork are never offered (GT-P3-5).
   Decision (Johan 2026-10-26): land M4 Phase 6 AGA pen mapping first and reuse it, as recommended.
6. Display modes full, window, mini, micro, icon switch through `ui.gadtools.mode` with cursor, selection and playback kept; lowered is refused (GT-P6-1).
   Decision (Johan 2026-10-26): the shared layout tree gets a viz region, as recommended.
7. Every Selecta colour token maps to a pen obtained with `ObtainBestPen` and released at close, `ui_pens_held=0` after close (GT-P7-1).
   Decision (Johan 2026-10-26): the status line shows the tooltip text on focus, as recommended.

Commands (rig tests only on the caller's go-ahead; nothing here runs on any Amiga or rig until then):
```
cmake -S . -B build/host -G Ninja
nice -n 19 ctest --test-dir build/host -L gadtools -j2 --output-on-failure
nice -n 19 ctest --test-dir build/host -R ratchet
cmake --build build/host --target lint
ctest --test-dir build/m68k -L rig-m4-gadtools   # needs rig a1200-aga-020; prints SKIPPED when it is not running
```
Rig facts: target is the FS-UAE config `a1200-aga-020`. It may not exist yet (M4-P0-2 is open): GT-P0-1 creates or confirms it and every rig item waits for it. Rig driver is `tests/amiga/run_rig.cmake` (`tests/CMakeLists.txt:992`). The rig window title carries the purpose text, for example `gadtools plugin check` (global rule 6a-2).

## 2. Architecture

### 2.1 Reuse table (nothing new where something exists)
| Need | Reused API | Where |
|---|---|---|
| Plugin descriptor and hooks | SEL_UI_OPEN, SEL_UI_WAIT, SEL_UI_STATS, SEL_UI_VIEW_STYLES, SEL_PLUGIN_ID as at `plugins/ui/text/text_ui.c:1346-1355`, `plugins/ui/desktop/desktop_ui.c:6480-6489` | `include/selecta/ui.h:13-40` |
| Open args | `sel_ui_open_args` | `ui.h:22-33` |
| Shell registers and switches; style mask check | `main.c:149-154`, `ui_switch` `main.c:204`; `set_ui_view_styles` refuses names outside the mask | `apps/selecta/main.c`, `engine.h:461` |
| Frame, clock, status words, dirty bits | `sel_uim_frame`, `sel_uim_update`, `sel_uim_take_dirty`, `sel_uim_clock`, `sel_uim_play_word` | `include/selecta/uimodel.h:12-80` |
| Menus, buttons, tooltips, keys | `sel_uim_action{id,label,tooltip,icon,menu,key}`, `sel_uim_action_count/at` | `uimodel.h:85-96` |
| Layout rectangles, breakpoints, hit test | `sel_uim_layout_compute(l,w,h)`, `sel_uim_layout{regions[5]}`, `sel_uim_region`, `sel_uim_breakpoint_for`, `sel_uim_hit`, `sel_uim_layout_check` | `uimodel.h:105-134`, `src/uimodel/layout.c:47-80, 81, 106` |
| Governor | `sel_uim_budget_for/allows/motion_allowed` | `uimodel.h:140-150` |
| Theme tokens | `sel_gui_token_*`, `sel_gui_tokens_diff` | `include/selecta/uimodel/tokens.h:115-130` |
| Lists | `sel_view_state`, `sel_view_row`, `sel_view_style`, `sel_engine_view_cursor_key` | `include/selecta/engine.h:66-113, 421` |
| Commands | `sel_engine_send`, `sel_engine_send_line`, `sel_engine_state` | `engine.h:275-291` |
| Drawing for custom areas | `sel_gui_canvas_init`, `fill_rect`, `sel_gui_text_draw`, `sel_gui_genart_draw`, `sel_gui_text_width` | `include/selecta/gui/gui_draw.h`, `gui_text.h`, `gui_genart.h` |
| Viz hosting | `sel_viz_render_fn`, `sel_surface` INDEX8 and planar, `find_plugin(SEL_PLUGIN_KIND_VIZ, id)` as at `desktop_ui.c:2218` | `include/selecta/viz.h:86-120` |
| AGA pens, planar path | the AGA backend of the custom UI (M4-P6-1, `ObtainBestPen` mapping of M4-P6-6): the pen table and c2p for viz are lifted, not rewritten | M4 plan Phase 6 |
| Rig driver | `tests/amiga/run_rig.cmake` | `tests/CMakeLists.txt:992` |
| Amiga mode facts | `pick_mode`, `desk_rtg_default_size` | `plugins/ui/desktop/amiga/backend_rtg.c:88, 128-146` |
| Shared Amiga parts (public screen, ToolTypes, AppWindow, AppIcon, asl) | the M4 Phase 7 pieces under `src/core/amiga/` (M4-P7-1..P7-6) | M4 plan Phase 7 |

Searches done: `grep -rn "gadtools\|GTLV_\|CreateGadget\|CreateContext" src plugins apps include` found nothing (verify again at GT-P0-6 from the CMake target list, not by grep alone); `plugins/ui/` holds only `desktop` and `text`. No `ui_open_` stat tag exists (shared with MUI C6); `ui_open_gadtools` is added to `include/selecta/tags.def` after the last tag, next to `ui_open_mui` (coordinate the tag order with MUI-P1-5 and the ReAction plan).

### 2.2 Shape
- `plugins/ui/gadtools/gadtools_ui.c`: descriptor and SEL_UI_OPEN / WAIT / CLOSE / STATS hooks. m68k-only target. All mapping code lives in `plugins/ui/gadtools/map/*.c` with no `<proto/gadtools.h>` or Intuition include, so host tests build it.
- Pure mapping functions (host-testable): `gt_menu_build` (action table to a NewMenu array), `gt_gadget_plan` (layout regions + font metrics to gadget rectangles and kinds), `gt_pen_for_token` (token to pen request), `gt_mode_flags` (mode pref to window flags and size), `gt_style_mask` (always four styles), `gt_list_rows` (view rows to ListView node strings, with indent), `gt_scroll_plan` (window larger than screen, scroll offset).
- Placement: GadTools has no layout engine. The plugin takes the regions of `sel_uim_layout_compute`, subdivides each region into gadget slots in `gt_gadget_plan` (transport row, seek slider, volume slider, search string, list, icon row), creates gadgets with `CreateGadgetA` on a `CreateContext` list, and on IDCMP_NEWSIZE does `RemoveGList`, `FreeGadgets`, recompute, create, `AddGList`, `RefreshGList`, `GT_RefreshWindow`. Gadget slots are integer pixel rectangles; the font comes from `GetScreenDrawInfo` (screen font), so character cell metrics feed `gt_gadget_plan`.
- Update path: `sel_uim_update`, then `sel_uim_take_dirty`; each dirty bit maps to one `GT_SetGadgetAttrs` call (clock text, progress slider, volume, status) or one text redraw in the info strip. List contents change only with `GTLV_Labels` detached (`~0`) during edit, as GadTools requires.
- Info strip (title, artist, clock, status word) is a custom-drawn text row via `Text()` in the window RastPort, not a TEXT_KIND gadget per field, because GadTools TEXT_KIND flickers on 60 Hz clock updates; the clock string comes from `sel_uim_clock`.
- Viz and planar art area: one custom-drawn rectangle in the window RastPort. `sel_viz` renders into a planar or INDEX8 surface; the plugin converts with the c2p lifted from the custom UI AGA path and `BltBitMapRastPort`. Governor `sel_uim_budget_for` caps frame rate.
- Event loop: SEL_UI_WAIT does `Wait` on the window user port signal OR-ed with the engine wake signal; `GT_GetIMsg` / `GT_ReplyIMsg`; returns 1 handled, 0 timeout, -1 quit (hook contract `ui.h:38`).
- OS floor 3.0: `ObtainBestPen` and `V39` GadTools tags. No dependency beyond `gadtools.library` 39, `intuition`, `graphics`, `utility`, `asl`, `iffparse` (clipboard). Everything else is the shared `src/core/amiga/` code.

### 2.3 Features GadTools cannot carry (and why)
- Grid, cover flow, artwork, generated cover art in lists: no chunky pixels and no per-row graphics in ListView; the plugin offers four styles only (Q10, section 5). Now Playing shows text metadata only.
- Columns in lists: ListView is one text column; the plugin pads fields to character cells with the screen font (monospace only) or the list shows title and artist in one string. Sortable column headers are not carried; sort is a menu action.
- Real tree widget: tree style is indented rows; expand and collapse is an action on the cursor row, not a click on a twisty.
- Drag and drop between rows: provided by the custom list gadget `gt_list` (GT-P3-9, Johan 2026-10-26); the ListView limitation no longer applies. Drop of files onto the window works through AppWindow.
- Tooltips: GadTools has no bubble help. The action label is the gadget label; the full tooltip text is in the status line on focus and in the help window.
- Right-mouse context menu on a row: Intuition owns the right button for the menu bar; the song menu is a menu entry set under a "Song" menu acting on the cursor row.
- Proportional fonts in lists: ListView uses the screen font; a proportional font works but columns will not align.
- Tweens, fades, hover animation, shadows, gradients, rounded corners, antialiased type: not available; the governor only caps custom-drawn areas.
- Inter font atlas and token font sizes: the screen font is used.
- Lowered screen mode: windows open on a public screen and do not own one (same as MUI).
- Screen-owning features: software pointer, copper gradients, kiosk takeover.
- Toast: no floating element; one status line (same answer as MUI).
- Layout dragging (Balance objects, user-resizable splitters): fixed proportions from the layout tree.
- Settings controls beyond GadTools kinds (colour picker, key rebinder): colour via `asl` is not planned; key rebinding page is carried as a ListView of actions plus a "Press key" requester (GT-P4-10).

## 3. Feature parity map (desktop UI, HippoPlayer, bbmp)
Each desktop feature is either carried (GT-id in section 5) or listed in 2.3. The first-pass map is section 8; HippoPlayer and bbmp rows are checked at GT-P8-1 (below), counted from the parity docs' tables.

## 4. Testing
- 4.1 Shape: host tests (label `gadtools`) cover the pure mapping functions and the `src/uimodel` pieces they use and run on every commit. Rig tests (label `rig-m4-gadtools`) cover the plugin on `a1200-aga-020` and print `SKIPPED rig not running` when the rig is down.
- 4.2 Reachability, one test per feature: `rig-m4-gadtools-open` drives `selecta --ui gadtools` through the real shell, asserts the sentinel `ui_open_gadtools=1` plus the spec numbers (window geometry, menu count, gadget count). Other behaviour is tested where it lives (host mapping tests).
- 4.3 Instrument checks (Gate 3): the sentinel is verified on a known-live case (`--ui gadtools`) and a known-dead case (`--ui text` reads `ui_open_gadtools=0`).
- 4.4 Nothing in this plan runs on any Amiga or rig until the caller says so. Manual checks are listed under GT-P7-7 and name the platform (Amiga, A1200 AGA).

## 5. Phased checklist (running count: 0 of 69)

### Phase 0: facts, rig, red tests
- [ ] GT-P0-1 Rig config `a1200-aga-020` exists (M4-P0-2): 68020 14 MHz, AGA, 2 MB chip, baseline fast RAM, Workbench 3.1+, with the title purpose option. Create under the rig tooling if missing; this item and M4-P0-2 close together. Test: `rig-m4-facts` asserts `CPU=68020` and `CPU_SPEED` within 10 percent of the recorded value (read-only; run on go-ahead).
- [ ] GT-P0-2 Fact: `gadtools.library` version on the rig (39 expected on 3.0, 40 on 3.1), Workbench screen mode id, size, depth, screen font (`GetScreenDrawInfo`), free pens. Record in section 9. Test: `rig-m4-gadtools-facts`: `ami.py exec Version gadtools.library` captured; asserts the version line exists and is >= 39.
- [ ] GT-P0-3 Fact: memory cost of the window with 6 gadgets, ListView of 40 rows and the planar viz area on 2 MB chip plus baseline fast RAM (M4-P5-13). Test: `rig-m4-gadtools-mem`: `Avail` before and after open; asserts the delta is written to section 9 and chip use of the viz bitmap = `w * h * depth / 8` bytes.
- [ ] GT-P0-4 Red test for the sentinel: `ui_open_gadtools` stat exists and is 0 for the text UI. Test: `gadtools-stat-tag`: host; asserts the tag value is greater than the last tag in `tags.def` and the text UI stats list lacks it.
- [ ] GT-P0-5 Red tests for the mapping functions (stubs returning 0): menu, gadget plan, pens, mode flags, style mask, list rows, scroll plan. Test: `gadtools-map-red`: asserts each fails before GT-P2/P3/P6/P7 land (documented red run).
- [ ] GT-P0-6 Toolchain: m68k SDK (`amiga-gcc`) has `<proto/gadtools.h>`, `<libraries/gadtools.h>`, `<intuition/intuition.h>`, OS 3.0 NDK includes; path recorded. Test: `gadtools-sdk-probe`: compiles a 10-line file calling `CreateContext` for m68k; asserts exit 0. Also re-count existing GadTools use from the CMake target list: asserts 0 targets named `*gadtools*` before GT-P1-1.

### Phase 1: plugin skeleton
- [ ] GT-P1-1 `plugins/ui/gadtools/CMakeLists.txt`, m68k-only target, built into the m68k image; host builds `gadtools_map` only. Test: `gadtools-build-split`: asserts the host target list has `gadtools_map` and no `gadtools_ui`.
- [ ] GT-P1-2 Descriptor with SEL_PLUGIN_ID "gadtools", SEL_UI_VIEW_STYLES from `gt_style_mask` (always regular, tree, drilldown, compact). Test: `gadtools-descriptor`: host; asserts id "gadtools" and mask popcount 4 for every tier including a fake RTG tier.
- [ ] GT-P1-3 `SEL_UI_OPEN` opens `gadtools.library` v39, locks the public screen, `GetVisualInfo`, creates a context and opens one window with IDCMP flags, `SEL_UI_CLOSE` frees gadgets, visual info and window in reverse order. Test: `rig-m4-gadtools-open`: asserts exit code 0 and no left-over windows (`amiga_windows` count back to the start count).
- [ ] GT-P1-4 Failure path: library missing, OS below 3.0 or window cannot open returns a distinct error, shows `EasyRequest` (M4-P7-6), shell keeps the text UI (Q26). Test: `rig-m4-gadtools-nolib`: `gadtools.library` renamed away; asserts requester text and the text UI running.
- [ ] GT-P1-5 Sentinel `ui_open_gadtools` stat tag in `tags.def`, incremented in `SEL_UI_OPEN` after the window opens, plus `SEL_UI_FRAMES`-style frame counter. Test: `rig-m4-gadtools-open`: asserts `ui_open_gadtools=1` with `--ui gadtools`, 0 with `--ui text`; frame counter > 0 after one `NEXT`.
- [ ] GT-P1-6 Registry entry: `--ui gadtools` in `UI_LIST`, hidden below OS 3.0 (M4-P11-2). Test: `gadtools-registry`: host with a fake OS version probe; asserts present at 39, absent at 37.
- [ ] GT-P1-7 `SEL_UI_WAIT` contract: 0 timeout, 1 handled, -1 quit; engine wake signal in the wait mask. Test: `gadtools-wait-contract`: host stub of the Wait loop; asserts the three return codes.

### Phase 2: layout, menus, transport
- [ ] GT-P2-1 `gt_gadget_plan`: regions from `sel_uim_layout_compute` (sidebar, main, detail, player, suggestion) to gadget rectangles with font metrics (border widths from the screen font); transport row, seek, volume, search string, list, icon row. Test: `gadtools-plan`: host; for widths 1280, 800, 640 asserts every gadget rect lies inside its region, none overlap, and `sel_uim_layout_check` returns 0.
- [ ] GT-P2-2 Relayout on IDCMP_NEWSIZE: tear down and rebuild the gadget list; no gadget leaked. Test: `gadtools-relayout`: host with a counting `CreateGadget`/`FreeGadgets` shim; 20 resizes assert created = freed and the final rects equal the plan for the last size. Rig: `rig-m4-gadtools-open` asserts rects equal the golden for the Workbench size.
- [ ] GT-P2-3 NewMenu from the action table: `gt_menu_build` splits `menu` paths into title / item / sub-item; shortcut char from `key`; commodity-free. Test: `gadtools-menu`: host; asserts item count = rows with non-empty `menu` and every `key` appears once as a shortcut or in the documented exempt list; reachability counted in `rig-m4-gadtools-open`.
- [ ] GT-P2-4 Menu activation (IDCMP_MENUPICK) sends the same command line the desktop sends, from the action table `id`. Test: `gadtools-menu-dispatch`: host; for every action id asserts the engine receives the same line as the desktop table.
- [ ] GT-P2-5 Transport row: Previous, Play Pause, Next, Stop, Repeat, Random as BUTTON_KIND with full English labels from the action table; seek SLIDER_KIND bound to `progress_q16` (0..65535 as GTSL_Min/Max scaled), volume SLIDER_KIND bound to `volume`. Test: `gadtools-transport-map`: host; asserts the button set equals action ids with transport icons, button labels have no abbreviations (lint list), seek range 0..65535.
- [ ] GT-P2-6 Frame display: title, artist, format, clock, remaining, length, status word from `sel_uim_frame` through dirty bits to one `GT_SetGadgetAttrs` or one strip redraw. Test: `gadtools-dirty-map`: host; each dirty bit 12..21 maps to exactly one target; asserts 10 mappings and that no unchanged bit triggers a call.
- [ ] GT-P2-7 Keyboard: `key` column to menu shortcuts and IDCMP_VANILLAKEY; Right Amiga + 1..5 and + I mode keys (M4 3.4); gadget underscore labels for GadTools key activation. Test: `gadtools-keys`: host; asserts every action key is bound once, the mode keys are bound, and underscored labels resolve to distinct keys within a window.
- [ ] GT-P2-8 Keyboard focus and Tab order = layout tree order (`sel_uim_hit` region order); Tab, Shift-Tab, Enter in the search string, Escape. Test: `gadtools-focus`: host; asserts the focus ring order equals region order and Escape in the string sends no search line.

### Phase 3: lists and view styles
- [ ] GT-P3-1 Regular list from `sel_view_state` rows into one `gt_list` gadget (GT-P3-9; LISTVIEW_KIND dropped by the Q4 decision), windowed by `window_start`/`row_n`; cursor in sync via `GTLV_Selected` and `GTLV_MakeVisible`. Test: `gadtools-list-regular`: host; fake state with 5000 rows; asserts only `window_rows` nodes are built and the node strings equal the golden for rows 0..2.
- [ ] GT-P3-2 Tree style: indented rows from `sel_view_row.depth` (two spaces per level), expand or collapse is an action on the cursor row sending the engine command. Test: `gadtools-list-tree`: host; asserts depth 0/1/2 indent and the toggle line.
- [ ] GT-P3-3 Drilldown style: two or three side-by-side ListViews from `drill[]`, plus a breadcrumb text. Test: `gadtools-list-drill`: host; asserts list count for `drill_n` 0, 1, 2 and the breadcrumb text.
- [ ] GT-P3-4 Compact style: title and artist in one row of `gt_list` as two columns (Q4 decision; no character-cell padding). Test: `gadtools-list-compact`: host; asserts row width <= list width / cell width for widths 80 and 60 characters.
- [ ] GT-P3-5 `gt_style_mask`: always 4 styles; `set_ui_view_styles` refuses grid and cover flow. Test: `gadtools-style-mask`: host; asserts popcount 4 on planar and RTG tiers and that grid is refused.
- [ ] GT-P3-6 Cursor key kept across style switches (property). Test: `gadtools-style-keep`: host; for each offered pair asserts `sel_engine_view_cursor_key` unchanged. Rig reachability: `rig-m4-gadtools-styles` asserts the same and offered count = 4.
- [ ] GT-P3-7 List scrolling and page keys; large lists never build more than `window_rows` nodes and `GTLV_ScrollWidth` follows the list. Test: `gadtools-list-scroll`: host; 10 000 rows, 100 page-downs, asserts `window_start` arithmetic and node count stays constant.
- [ ] GT-P3-9 Custom list gadget `gt_list` (Q3 and Q4 decisions): one BOOPSI gadget class drawn by the plugin: rows from the `sel_view_state` window, true columns from the F48 column set with per-column widths measured with the screen font (proportional fonts allowed), cursor and selection pens from the pen table, scroller through a PROPGADGET, keyboard handling (Up, Down, Page keys, Home, End, Return) on the gadget, and drag reorder (press and hold on a row, drag past another row, release sends QUEUE_MOVE with the two ids; Escape cancels). It replaces LISTVIEW_KIND in GT-P3-1, GT-P3-3, GT-P3-4, GT-P3-8, GT-P4-4 and GT-P4-17. Test: `gadtools-list-gadget`: host; a fake state of 5000 rows with a measured fake font asserts the column widths, that only `window_rows` rows are laid out, the cursor row after each key, and that a drag from row 2 to row 5 yields QUEUE_MOVE(id2, id5) and an Escape mid-drag yields nothing. Rig reachability: `rig-m4-gadtools-drag` on rig 2 drags a queue row with `ami.py` mouse events and asserts the queue order changed.
- [ ] GT-P3-8 Sort and column set (F48): sort is a menu action; fields shown follow the F48 column set as true columns of `gt_list` (Q4 decision). Test: `gadtools-columns`: host; asserts the field set per breakpoint and sort command line.

### Phase 4: views and features
- [ ] GT-P4-1 Home view from SEL_ROW_HOME rows. Test: `gadtools-home`: host; asserts rows map to play-song versus open-album.
- [ ] GT-P4-2 Library view with breadcrumbs (`browse_path`, `browse_leaf`). Test: `gadtools-library`: host; asserts leaf split on "/" and ":".
- [ ] GT-P4-3 Search view: STRING_KIND, live filter sends the search command. Test: `gadtools-search`: host; typing "abc" sends one search line with "abc".
- [ ] GT-P4-4 Queue view: drag reorder in `gt_list` (Q3 decision, GT-P3-9) plus move up, move down, remove, clear, add as buttons and menu actions. Test: `gadtools-queue`: host; asserts QUEUE_MOVE with ids.
- [ ] GT-P4-5 Playlists view: list, open, rename, delete. Test: `gadtools-playlists`: host; asserts commands per action.
- [ ] GT-P4-6 Song menu: a "Song" menu acting on the cursor row from the same action subset as the desktop (DUI-P4-16). Test: `gadtools-song-menu`: host; asserts entry set equals the desktop's.
- [ ] GT-P4-7 Likes and ratings: Like toggle button and menu entry; rating via CYCLE_KIND 0..5. Test: `gadtools-like-rating`: host; asserts LIKE command and rating 0..5 mapping.
- [ ] GT-P4-8 Now Playing: text panel with metadata, lyric line, up-next list; no artwork. Test: `gadtools-nowplaying`: host; asserts the field set and `lyric_slot` to line mapping.
- [ ] GT-P4-9 Song info panel (tags, format, subsongs; subsong CYCLE_KIND from `subsong`). Test: `gadtools-songinfo`: host; asserts fields and the subsong cycle command.
- [ ] GT-P4-10 Settings window generated from `prefs.def` (CYCLE_KIND, CHECKBOX_KIND, SLIDER_KIND, STRING_KIND, INTEGER_KIND); PREF_SET on each change; key rebinding as action ListView plus a press-key requester. Test: `gadtools-settings`: host; asserts one gadget per pref row with a UI type and PREF_SET sent on change; the rebound key triggers its action and the old key does not.
- [ ] GT-P4-11 Toasts: one status line at the window bottom with 3 s timer, errors 6 s, replace not stack. Test: `gadtools-toast`: host; asserts durations 3000 and 6000 ms and replace rule.
- [ ] GT-P4-12 Dialogs: confirm via `EasyRequest`; file pick via asl (M4-P7-3); text prompt via a small string window. Test: `gadtools-dialogs`: host; asserts confirm text and that Clear Library removes everything including playlists (DUI Q16).
- [ ] GT-P4-13 Command line and palette: a STRING_KIND popup line sending `sel_engine_send_line` (cmdline.h, palette.h). Test: `gadtools-cmdline`: host; asserts a typed line reaches the engine unchanged and history survives restart.
- [ ] GT-P4-14 Help window from `sel_uim_help_row`; About on the Help key. Test: `gadtools-help`: host; row count equals `sel_uim_actions_check` count.
- [ ] GT-P4-15 Empty, loading, error states per view (DUI-P4-21). Test: `gadtools-states`: host; asserts the text per state.
- [ ] GT-P4-16 Tracker tabs and chip registers pane as text rows via `sel_uim_cell_fields`, only when tracker data exists. Test: `gadtools-tracker`: host; asserts field text for one pattern cell.
- [ ] GT-P4-17 Activity, Downloads and Extra Files panels as ListView plus Stop and Continue buttons (M4-P12-10). Test: `gadtools-activity`: host; fake slow source with three jobs; asserts three rows and the Stop and Continue lines.
- [ ] GT-P4-18 ARexx: no second port; the shell owns the one ARexx port (M4 ARexx plan) and the window adds none. Test: `rig-m4-gadtools-arexx`: `amiga_arexx` PLAY while `--ui gadtools` runs; asserts the engine state changes and the info strip shows the new status word.

### Phase 5: custom area, viz
- [ ] GT-P5-1 Viz area: one window-RastPort rectangle from the plan; planar viz surface via `sel_viz`, c2p lifted from the custom UI, `BltBitMapRastPort`. Test: `gadtools-viz-surface`: host; asserts surface format INDEX8 with pitch >= w and the rectangle equals the plan's viz slot.
- [ ] GT-P5-2 Viz hosting per tick: `find_plugin(SEL_PLUGIN_KIND_VIZ, id)`, render when the governor allows. Test: `rig-m4-gadtools-viz`: asserts `VIZ_FRAME` count > 0 for scopes in the window and viz >= 25 fps for 30 s on the rig (M4-P6-5); no gadget flicker is judged in GT-P7-7.
- [ ] GT-P5-3 Viz does not draw over gadgets or the menu: clip to the viz rectangle with a layer clip rect, refresh on IDCMP_REFRESHWINDOW. Test: `gadtools-viz-clip`: host with a fake RastPort recorder; asserts every blit rect lies inside the viz slot.
- [ ] GT-P5-4 Frame budget: governor `sel_uim_budget_for` caps viz redraw rate. Test: `gadtools-budget`: host; asserts redraw count per second per tier.
- [ ] GT-P5-5 Generated art on planar: `sel_gui_genart_draw` shown in the viz slot as a mode of the viz area when no viz plugin runs (Q12 stays: no cover art). Test: `gadtools-genart`: host; same song key gives the same hash and two formats differ.
- [ ] GT-P5-6 Drop and clipboard: AppWindow drop = QUEUE_ADD (M4-P7-2); clipboard text. Test: `rig-m4-gadtools-drop`: drop one file; asserts the queue gains one entry.

### Phase 6: display modes, compact layout, screens
- [ ] GT-P6-1 Modes full, window, mini, micro, icon via `ui.gadtools.mode`; `gt_mode_flags` gives size and flags; lowered refused. Test: `rig-m4-modes --ui gadtools` asserts five modes accepted, `lowered` refused, cursor key and play state equal before and after each switch. Host `gadtools-mode-flags` asserts flags and sizes per mode.
- [ ] GT-P6-2 Compact one-column layout below the icon-sidebar breakpoint with the bottom icon row (five BUTTON_KIND gadgets in the order Home, Library, Search, Queue, Now Playing) above the transport (Q24); needs the new breakpoint in `src/uimodel/layout.c` (C1 in the MUI plan, tracked as DUI-P1-11; section 6 C7). Test: `gadtools-compact-layout`: host; at 640x256 asserts one column, five icons in that order, no overlap, list height >= 4 rows of the screen font.
- [ ] GT-P6-3 640x200 scrolls (window taller than the screen area; scroll via a PROPGCLASS and `ScrollWindowRaster`, `gt_scroll_plan`); below 640 px wide refuse with a message (Q26). Test: `gadtools-small`: host; asserts 640x256 no scroll, 640x200 scroll offset range, 600 wide refusal. Rig: `rig-m4-rtg-small` extended to `--ui gadtools` with 640x200.
- [ ] GT-P6-4 Mini player (~320x64) and Micro strip (280 px, one line) layouts: title, time, play/pause, next, seek. Test: `gadtools-mini-micro`: host; asserts gadget sets and widths.
- [ ] GT-P6-5 Iconified: AppIcon, drop = QUEUE_ADD, double click restores the previous mode (M4-P7-2). Test: `rig-m4-appdrop` extended to `--ui gadtools`.
- [ ] GT-P6-6 `PUBSCREEN=`, `MODE=` and ToolTypes via the shared parser in `src/core/amiga/` (M4-P7-1, P7-5, DUI-P1-3); no parser in the plugin. Test: `rig-m4-pubscreen`: `selecta --ui gadtools PUBSCREEN=Selecta`; asserts the window is on that screen.
- [ ] GT-P6-7 Window geometry and mode remembered per mode (M4-P12-5). Test: `gadtools-remember`: host; asserts stored and restored rectangle per mode.
- [ ] GT-P6-8 Low memory and AGA small screen (M4-P6-8, M4-P5-13): viz bitmap released when hidden, open fails with the Q26 refusal when chip RAM < the budget of GT-P0-3. Test: `rig-m4-gadtools-lowmem`: asserts open succeeds within the recorded budget, else the refusal.

### Phase 7: theme, performance, landing
- [ ] GT-P7-1 `gt_pen_for_token`: every colour token to an `ObtainBestPen` request; pens released at close; pens obtained <= free Workbench pens (M4-P6-6). Test: `gadtools-pens`: host with a fake pen allocator; asserts every colour token maps to a pen request or a documented exemption; `rig-m4-modes` asserts `ui_pens_held=0` after close.
- [ ] GT-P7-2 Theme presets collapse to the pens available; gadget colours stay the Workbench ones (GadTools draws its own frames with the screen DrawInfo); tokens apply only to the info strip, viz area and list highlight. Test: `gadtools-theme`: host; preview changes the strip hash, escape restores it.
- [ ] GT-P7-3 Fonts: gadgets use the screen font, custom areas use `sel_gui_text_width` on the same font metrics. Test: `gadtools-font`: host; asserts the strip text width uses screen metrics.
- [ ] GT-P7-4 Knob check: every pref declared for GadTools moves the output or carries a written exemption. Test: `gadtools-knobs`: host; asserts each `ui.gadtools.*` pref changes a mapped value.
- [ ] GT-P7-5 Switching with all UIs (M4-P11-1): text, gadtools, desktop and back keeps cursor, queue, playback. Test: `rig-m4-switch-all` includes `gadtools`; asserts `ui_switches` count, underruns 0, one song opened.
- [ ] GT-P7-6 Image size on m68k (M4-P11-3) and tier parity (M4-P11-7). Test: `gadtools-size`: asserts plugin size below the recorded budget (record at GT-P0-3).
- [ ] GT-P7-7 Landing: ratchet counts, lint, docs; manual check by Johan on the A1200 AGA rig (Amiga): window fills the screen, transport buttons respond, list scrolls, scopes run without gadget flicker. Test: `ratchet`: all GT rows ticked, none `[~]`.

### Phase 8: parity check
- [ ] GT-P8-1 Read the HippoPlayer and bbmp parity tables row by row; every row maps to a GT-id, an action-table row or a section 2.3 entry. Test: `gadtools-parity`: host; unmapped count = 0, counted from both docs' tables and from the action table's parity column (same method as M4-P2-5).

Count check: Phase 0: 6, Phase 1: 7, Phase 2: 8, Phase 3: 8, Phase 4: 18, Phase 5: 6, Phase 6: 8, Phase 7: 7, Phase 8: 1. Total 69, counted with grep per phase.

## 6. Contradictions between code and the M4 plan
C1-C6 of the MUI plan apply unchanged: C1 no compact breakpoint in `src/uimodel/layout.c:15-33`; C2 `uimodel.h:44` layout modes lack lowered, micro, icon; C3 style mask per tier; C4 window = public screen area; C5 stale `rtg-060` name (GadTools target is `a1200-aga-020`); C6 no `ui_open_*` sentinel exists.
- C7: M4 plan line 77 specifies `sel_uim_layout(uim, w, h, &metrics, mode)` with a mode argument; the code has `sel_uim_layout_compute(sel_uim_layout* l, uint32_t width, uint32_t height)` (`include/selecta/uimodel.h:130`, `src/uimodel/layout.c:47`), no metrics, no mode, five fixed regions (`SEL_UIM_LAYOUT_REGIONS 5`, `uimodel.h:110`: suggestion, sidebar, main, detail, player). GadTools needs the font metrics (gadget heights depend on the screen font) and a viz and icon-row region; `gt_gadget_plan` consumes the rectangles and adds metrics itself until DUI-P1-11 extends the layout.
- C8: the layout has no viz region, so a viz slot must be carved from the `detail` or `main` region inside the plugin map, which is private logic; resolve by adding the region in `src/uimodel/layout.c` (shared with ReAction and MUI) before GT-P5-1.
- C9: M4-P10-1 asserts a "layout golden for the Workbench size"; no layout goldens exist yet for 640x256 (`uimodel` layout tests are for the matrix of M4-P2-4 / DUI-P1-7). GT-P2-1 depends on those goldens.
- C10: M4 line 258 and M4-P6-6 assume the AGA pen mapping lives in the custom UI AGA backend (M4 Phase 6), which does not exist yet; GT-P7-1 should consume its pen table. If Phase 6 lands later, `gt_pen_for_token` must be written first and the AGA backend reuse it instead (open question 5).

## 7. Open questions (decided by Johan 2026-10-26; recommended answer first, decision below each)
1. Minimum OS 3.0 with gadtools.library 39. Recommended: 3.0 (Q10 stands). Trade-off: ObtainBestPen and the V39 tags only; 2.0 users would need a second pen path.
2. Info strip custom-drawn versus TEXT_KIND gadgets. Recommended: custom-drawn strip. Trade-off: no flicker at clock rate and one redraw per dirty bit; text is not a GadTools gadget so GadTools key navigation cannot focus it.
3. Queue reorder without drag. Recommended: move up and move down actions on keys and buttons. Trade-off: simple; drag reorder needs a custom list gadget, which is a second list implementation.
4. Columns. Recommended: one padded text column from the F48 set, monospace screen font assumed. Trade-off: GadTools baseline only; proportional fonts misalign; a custom-drawn list would give true columns but replaces ListView (and its keyboard handling).
5. Pen table ownership. Recommended: land M4 Phase 6 AGA pen mapping first and reuse it. Trade-off: one pen mapper; GadTools is delayed until Phase 6, otherwise the mapper is written here and moved later.
6. Viz region ownership. Recommended: add a viz region to the shared layout tree. Trade-off: all UIs get the slot, tests need new goldens; carving it in the plugin is quicker but private.
7. Tooltip substitute. Recommended: status line shows the tooltip text on focus. Trade-off: no bubble help; works with the keyboard.
8. Order of the three toolkit phases (Q16 accepted MUI, ReAction, GadTools). Recommended: keep. Trade-off: the A1200 AGA machine in this plan is a GadTools-first target; MUI waits on RTG users.
   Decision (Johan 2026-10-26): keep the order MUI, ReAction, GadTools, as recommended.

## 8. Coverage
- 8.1 Design rows: M4 section 5 GadTools column: baseline (P1-3, P1-4), layout (P2-1, P2-2), lists (P3-1..P3-8), grid and cover flow hidden (P3-5, 2.3), viz (P5-1..P5-4), Now Playing text (P4-8), menus (P2-3, P2-4), display modes (P6-1), theme (P7-1, P7-2).
- 8.2 Desktop parity: Home, Library, Search, Queue, Playlists, Now Playing, Song info, Settings, song menu, likes and ratings, toasts, dialogs, command line, help, states, tracker, activity panels (P4-1..P4-18). Not carried: grid, cover flow, artwork, tooltips, drag, columns headers (2.3). HippoPlayer and bbmp: carried through the desktop features; the row-by-row check is GT-P8-1 and is open.
- 8.3 Row counts: Phase 0: 6, 1: 7, 2: 8, 3: 8, 4: 18, 5: 6, 6: 8, 7: 7, 8: 1 = 69. To be verified by `grep -c '^- \[ \] GT-P'` and by awk per phase before landing (counted per phase by grep when written; recount at landing).
- 8.4 Older M4 rows replaced: M4-P10-1 (GT-P1-3, GT-P1-5, GT-P2-1, GT-P2-2), M4-P10-2 (GT-P3-1..P3-6), M4-P10-3 (GT-P5-1, GT-P5-2, GT-P6-1).

## 9. Recorded facts (fill in at GT-P0-1..3)
- gadtools.library version on the rig: unknown.
- Workbench mode id, size, depth, screen font: unknown.
- Memory delta and chip use: unknown.

## 10. M4 items that move
| M4 ID | Item | GT ID |
|---|---|---|
| M4-P10-1 | plugin with manual placement, relayout on NEWSIZE | GT-P1-3, GT-P1-5, GT-P2-1, GT-P2-2 |
| M4-P10-2 | regular, tree, drilldown, compact | GT-P3-1..P3-6 |
| M4-P10-3 | viz custom area and display modes | GT-P5-1, GT-P5-2, GT-P6-1 |
