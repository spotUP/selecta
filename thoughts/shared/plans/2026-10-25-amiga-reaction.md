---
date: 2026-10-25
author: spotUP
status: draft, waiting for review (section 8 questions)
milestone: M4 Amiga UI, Phase 9 (ReAction), expanded
implements: M4 plan section 5 "Downscale plan: MUI, ReAction, GadTools" (ReAction column) and Phase 9 (M4-P9-1 to M4-P9-3, moved here); M4 plan section 3.4 (display modes), section 8a answers Q9, Q16, Q24, Q27; ledger I344
related: thoughts/shared/plans/2026-10-24-m4-amiga-ui.md, thoughts/shared/plans/2026-10-24-desktop-ui.md, thoughts/shared/research/2026-10-22_parity-hippoplayer.md, thoughts/shared/research/2026-10-22_parity-bbmp.md, ~/.claude/REACHABILITY_PROTOCOL.md
---

# ReAction UI plugin (`plugins/ui/reaction/`)

## 1. Goal and finish line

Goal: `selecta --ui reaction` opens a native OS 3.2 ReAction window on the Workbench public screen that carries the same views, actions and display modes as the desktop UI, as far as the OS 3.2 classes allow. The plugin is glue: every string, action, layout rectangle, mode and colour comes from `src/uimodel/`; every pixel of a custom area (grid, cover flow, visualizers, artwork) comes from `src/gui/draw/` and the viz plugins. The plugin owns no player logic, no formatting and no layout arithmetic (CLAUDE.md single source of truth; M4 plan section 5 last sentence).

Baseline (M4 Q9, accepted in section 8a): OS 3.2 ReAction classes only: `window.class`, `layout.gadget`, `listbrowser.gadget`, `chooser.gadget`, `clicktab.gadget`, `space.gadget`, `scroller.gadget`, plus the images and gadgets that ship with 3.2 and that Phase 0 proves present on the rig (button, string, integer, checkbox, slider, label, glyph, bitmap, penmap). Anything beyond that list needs a Phase 0 fact and an open question (section 8, Q3).

Targets (Johan, 2026-10-24 and 2026-10-25, M4 section 8a): the window covers the whole public screen area below the screen title bar by default; compact modes are switchable; 640x256 PAL hires is the design target and is fully laid out; 640x200 uses the same layout and its lists scroll; below 640 px width the ReAction UI refuses to open and the text UI keeps running (reason printed and shown in a requester when started from Workbench). Compact layout: one column, a bottom icon row (Home, Library, Search, Queue, Now Playing) above the transport.

Finish line, measurable. This plan is done when all of these hold:

1. Every ID in section 6 (RA-P0-1 to RA-P7-3) is ticked with its test green. Host tests are labelled `ra`; rig tests are named `rig-ra-*` and run on FS-UAE rig 2 (OS 3.2, uaegfx) and, for the final check, on the Replay (Johan's machine).
2. One reachability test, `rig-ra-open` (RA-P1-3), drives `selecta --ui reaction` on the rig and asserts the sentinel `ui_open_reaction=1` (section 4.1), the window size equal to the public screen area, and the menu item count equal to the action-table rows with a menu path. It is the only test that proves WIRED; every other behaviour is tested where it lives (host tests of the mapping functions, section 4.3).
3. The ratchet `tests/ratchet/unimplemented.txt` holds zero RA IDs and M4-P9-1 to M4-P9-3 stay `[>]`.
4. Every feature of the desktop plan that ReAction cannot carry is named in section 5 with the reason, and that list matches what the plugin hides at run time (RA-P3-12).
5. Johan has run the manual check RA-P7-3 on the Replay and said yes.

Commands (from RULES.md `## Commands`; host part only, nothing runs on a rig from this plan):

```
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/host
nice -n 19 ctest --test-dir build/host -L ra -j2 --output-on-failure
nice -n 19 ctest --test-dir build/host -R ratchet --output-on-failure
cmake --build build/host --target lint
```

Cross build and rig runs use the existing Amiga toolchain target and the `amiga` MCP / `ami.py` tooling; rig 2 is `UPTERM_RIG=2` (uaegfx, OS 3.2). The M4 plan's rig name `rtg-060` is stale; this plan uses "rig 2" and "the Replay".

Depends on (must be landed or in flight; the RA item that needs each is named): DUI-P1-4 to DUI-P1-9 and DUI-P1-10 (`src/uimodel/`, done: `include/selecta/uimodel.h`), DUI-P1-7 (layout matrix including 640x256), DUI-P4-20 (compact full-screen layout), DUI-P2 (draw, text, image, genart), DUI-P4-5 (grid and cover flow draw code), DUI-P4-11 (`sel_surface` viz), the display-mode set of M4 section 3.4 in `src/uimodel/` (M4-P5-3 owner, see section 7 finding F1), M4-P7-1 to M4-P7-6 (OS integration shared by all Amiga UIs).

## 2. Reuse table (searched before designing)

Searched: `include/selecta/uimodel.h` and `include/selecta/uimodel/*.h`, `src/uimodel/*.c`, `include/selecta/ui.h`, `include/selecta/gui/*.h`, `plugins/ui/desktop/desktop_ui.c` and `plugins/ui/desktop/amiga/`, `plugins/ui/text/text_ui.c`, `include/selecta/engine.h`, `viz.h`, `prefs.def`, `tags.def`. Grep terms: `reaction`, `ReAction`, `space.gadget`, `listbrowser`, `ui_open_`, `sel_uim_`, `SEL_UI_VIEW_STYLES`. Result: no ReAction code, no `ui_open_*` stat and no `ui.reaction.*` pref exists today (all new, section 4.1). Everything else is reused:

| Need | Reused API | Where | What the plugin does with it |
|---|---|---|---|
| Plugin descriptor, hooks | `SEL_UI_NAME`, `SEL_UI_OPEN`, `SEL_UI_ON_EVENT`, `SEL_UI_WAIT`, `SEL_UI_CLOSE`, `SEL_UI_STATS`, `SEL_UI_VIEW_STYLES` hooks; `sel_ui_open_args` | `include/selecta/ui.h:20-44`; descriptor pattern `plugins/ui/desktop/desktop_ui.c:6479-6491` | same descriptor shape, name "ReAction" |
| Frame strings | `sel_uim_frame` (title, artist, clock, remaining, status, progress_q16, queue, volume, subsong, repeat, random, lyric_slot, panel_serial), `sel_uim_update`, `sel_uim_take_dirty`, dirty bits `SEL_UIM_DIRTY_*` | `include/selecta/uimodel.h:13-61` | one `sel_uim_update` per pump (as `desktop_ui.c:4474`); a dirty bit gives one `SetGadgetAttrs` batch for that region only |
| Display strings | `sel_uim_clock`, `sel_uim_play_word`, `sel_uim_stage_status`, `sel_uim_queue_title`, `sel_uim_tag_label`, `sel_uim_cell_fields`, `sel_uim_note_name` | `uimodel.h:63-82` | text of labels, list titles, pattern cells |
| Action table | `sel_uim_action_count/at`, fields id, label, tooltip, icon, menu, key, line; `sel_uim_help_row`; `sel_uim_actions_check` | `uimodel.h:85-102`; consumer pattern `desktop_ui.c:3912-3918` (`desk_action_by_id`) | menu strip, toolbar buttons, hint text, help window; activation = `sel_engine_send_line(line)` (`engine.h:285`) |
| Layout | `sel_uim_layout_compute`, `sel_uim_layout` regions (name, x, y, w, h, min_w, min_h), `sel_uim_breakpoint_for`, `sel_uim_hit`, `sel_uim_layout_check` | `uimodel.h:104-134`, `src/uimodel/layout.c:22-78`; consumer `desktop_ui.c:5739` | region rectangles give the weights and the breakpoint of the layout.gadget tree (section 4.2) |
| Budget | `sel_uim_budget_for("index8")`, `sel_uim_budget_allows`, `sel_uim_motion_allowed` | `uimodel.h:136-150` | ReAction runs no tween; budget gates the custom-area effects (blur, shadow off) |
| Tokens and theme | `SEL_GUI_TOKENS` X-macro, `sel_gui_token_find/default`, `txt_theme_parse`, `txt_theme_default` | `include/selecta/uimodel/tokens.h:15-30,115-130`, `uimodel/theme.h:45-54` | token to pen table (RA-P5-4) |
| Key bindings | `txt_bindings_default/load/lookup/key_for`, `txt_key_parse/name`, `txt_action_known` | `include/selecta/uimodel/keybind.h:41-69`, `src/uimodel/keybind.c` | raw key to action name; menu shortcut text via `txt_bindings_key_for` |
| Raw input to neutral event | `sel_gui_input_map(st, raw, out)`, `sel_gui_raw`, `sel_gui_input` | `include/selecta/gui/gui_input.h:51-87` | custom areas (space.gadget) feed IDCMP mouse and key data through the same mapper as every backend |
| Command palette, command line | `txt_palette_open/key/picked`, `txt_cmdline_open/key/text`, history load and dump | `uimodel/palette.h:46-54`, `uimodel/cmdline.h:39-49` | string gadget plus listbrowser window driven by these models (RA-P5-2) |
| Engine view model | `sel_view_state`, `sel_view_row`, `sel_view_source`, `sel_view_style`, `sel_view_style_label`, `sel_engine_set_ui_view_styles`, `SEL_VIEW_STYLES_ALL`, `sel_engine_view_cursor_key` | `engine.h:62-120,414-421,463` | list rows come from `sel_state.view` (`engine.h:161`); the plugin declares which styles it offers |
| Commands | `sel_engine_send_line` | `engine.h:285` | the only way the plugin acts; no direct engine calls except reads |
| Pattern, tracker, fade | `sel_engine_pattern_request`, `sel_engine_tracker_info`, `sel_engine_fade` | `engine.h:453,438,432` | tracker tabs, fade marker |
| Drawing for custom areas | `sel_gui_canvas`, `sel_gui_canvas_init(INDEX8/RGBA32)`, `sel_gui_set_palette`, `sel_gui_present`, dirty rectangles, `sel_gui_fill_rect`, `sel_gui_blit_image`, `sel_gui_hash` | `include/selecta/gui/gui_draw.h:16-96` | grid, cover flow, artwork and viz drawn into a chunky buffer, then copied into the space.gadget RastPort |
| Widgets for custom areas | `sel_gui_widget_draw`, `sel_gui_style` | `include/selecta/gui/gui_widget.h:44-96` | only inside custom areas (tile captions, cover flow labels); never to replace ReAction gadgets |
| Text and images | `gui_text.h`, `sel_gui_art_cache_*`, `sel_gui_image_scale`, `sel_gui_genart_draw`, `sel_gui_thumb_unpack` | `include/selecta/gui/gui_image.h:14-37`, `gui_genart.h:15-20` | artwork, generated art for songs without covers |
| Viz | `sel_surface`, `sel_pixfmt` (INDEX8, RGBA32), `sel_viz_colors`, `sel_viz_render_fn`; plugin lookup `sel_ui_open_args.find_plugin` | `include/selecta/viz.h:93-116`, `ui.h:41` | space.gadget hosts the plugin through the same call the desktop UI makes |
| Art source | `sel_ui_art` (`find`, `get`, `put`) | `ui.h:12-17` | cover bytes for artwork |
| Prefs | `PREF_GET/PREF_SET/PREFS_CHANGED`, `prefs.def` (`ui.desktop.mode` at `prefs.def:12`) | `include/selecta/prefs.def` | `ui.reaction.mode` and the other `ui.reaction.*` keys, same row shapes |
| Amiga screen and window helpers | `LockPubScreen(NULL)` size read, Workbench mode handling | `plugins/ui/desktop/amiga/backend_rtg.c:128-146`; Picasso96 bits `backend_rtg.c:88-112` | copy the Workbench-size read; the plugin opens no screen of its own (Q6 accepted) |
| Stats | `SEL_UI_STAT_*` list, `STAT_BACKEND` | `desktop_ui.c:6442-6454`, `tags.def:242` | `STAT_BACKEND` = "reaction"; the sentinel is derived (section 4.1) |
| OS integration shared by all Amiga UIs | AppWindow, AppIcon, asl, PUBSCREEN, ToolTypes parser, Workbench-start requester | M4-P7-1 to M4-P7-6 | the plugin calls them; it does not re-implement them |

New code in the plugin (section 4): the class glue only (object creation, attribute mapping, IDCMP/WMHI handling, the space.gadget render hook, the row adapter between `sel_view_row` and listbrowser nodes).

## 3. Views and what carries each

All views read `sel_state` through `sel_uim_frame` and `sel_view_state`, and act only through the action table lines or `sel_engine_send_line`. Source of the view list: desktop plan section 3.

| View | ReAction construction | Carried? |
|---|---|---|
| Frame (sidebar, content, Now Playing panel, transport) | `window.class` holding a `layout.gadget` group tree built per breakpoint from `sel_uim_layout` (4.2) | yes, three breakpoints plus the compact one-column layout |
| Home | page of `layout.gadget` shelves: each shelf a heading label plus a horizontal row of `space.gadget` tiles (artwork) or, when no artwork is possible, a `listbrowser` | yes; tiles need RTG (space.gadget bitmap), planar screens show the list form |
| Library, browse | `listbrowser.gadget` with columns from the F48 column format; tree style through `LBNA_Generation` hierarchy; drilldown as `clicktab.gadget` over listbrowsers (M4-P9-2) | yes: regular, tree, drilldown, compact; grid and cover flow only on RTG (space.gadget) |
| Search | `string.gadget` (debounced) plus `chooser.gadget` scope plus result `listbrowser` grouped by type with separator rows | yes; typed filters are plain text (`format:mod`) |
| Now Playing | group: artwork `space.gadget`, labels from `sel_uim_frame`, metadata `listbrowser` (label / value columns from `sel_state.panel`), lyrics `listbrowser` whose selected row follows `lyric_slot`, rating chooser, like button | yes |
| Queue | `listbrowser` with the `QUEUE` source, drag reorder through the listbrowser drag support if Phase 0 shows it, otherwise Move Up / Move Down actions | yes, reorder method fixed by RA-P0-3 |
| Playlists | sidebar tree listbrowser (folders as generations) plus detail listbrowser | yes |
| Song info | window or page: `listbrowser` of panel pairs, note `string.gadget`, flags as checkboxes | yes |
| Visualizer area | `space.gadget` with a render hook calling the viz plugin (4.4); pattern view in a space.gadget drawn with `sel_uim_cell_fields` text | yes on RTG chunky; planar path in RA-P4-2 |
| Settings | separate window: `clicktab.gadget` pages generated from `prefs.def` groups, each control a ReAction gadget chosen by pref type | yes |
| Overlays | palette, command line, help, dialogs as separate windows or requesters; toasts as a status label plus a requester for errors | partly (section 5) |

## 4. Design

### 4.1 Plugin structure and the sentinel

Path `plugins/ui/reaction/`: `reaction_ui.c` (descriptor, open, wait, close, stats), `ra_window.c` (window and layout group construction), `ra_lists.c` (row adapter), `ra_area.c` (space.gadget render hook and input), `ra_menu.c` (menu strip from the action table), `ra_prefs.c` (settings pages), `ra_os.c` (AppWindow, AppIcon, asl; thin calls into the M4-P7 helpers). Host build: the class glue is behind `#ifdef __amigaos__`; the pure mapping functions (4.3) are compiled on the host so they can be tested.

Registry: plugin id `reaction`, UI name "ReAction", `SEL_UI_VIEW_STYLES` = regular, tree, drilldown, compact always; grid and cover flow when the opened screen depth is 15 bits or more (RTG). The count the plugin offers is 6 on RTG and 4 on planar, as M4-P8-2 and M4-P9-2 state.

Sentinel: `grep` found no `ui_open_*` stat anywhere in `plugins/`, `src/`, `tests/` or `tags.def` (the M4 plan's `ui_open_mui=1` / `ui_open_custom=1` have no code behind them). This plan reuses `STAT_BACKEND` (`tags.def:242`, filled at `desktop_ui.c:6452`): the plugin reports "reaction", and the test harness prints `ui_open_reaction=1` when the plugin id list in the registry matches and `STAT_BACKEND` is "reaction" and `STAT_FRAMES` is above 0. Whoever implements M4-P8-1 uses the same helper (open question Q9).

Prefs added to `prefs.def` (same row shape as `prefs.def:12-15`): `ui.reaction.mode` (full, window, mini, micro, icon; see F1), `ui.reaction.viz`, `ui.reaction.font`, `ui.reaction.reduce_motion` is not added: ReAction has no tweens, so a pref with no effect would break the no-dead-knob rule (RA-P7-1 sweeps this).

### 4.2 Layout: region rectangles to layout.gadget

`sel_uim_layout` gives a flat list of rectangles, not a tree with weights (`uimodel.h:114-126`, F2). The plugin converts: for each breakpoint it builds one fixed group tree keyed by region name:

- three-column: horizontal group [sidebar | vertical group [main] | detail], over the player row;
- two-column and icon-sidebar: the same without detail; the sidebar shows labels or icons only;
- compact (DUI-P4-20, name and regions from that item): one vertical group [main, icon row, player].

Weights come from the region sizes of the model (`LAYOUT_WeightBar`/weight attributes set from `w` and `h`), minimum sizes from `min_w`, `min_h`. On `IDCMP_NEWSIZE`/window resize the plugin calls `sel_uim_layout_compute` with the new inner size; if the breakpoint changed it swaps the group (method fixed by RA-P0-4), otherwise it only re-weights. The pure function `ra_layout_plan(const sel_uim_layout*, ra_plan*)` (host-testable) turns a layout into the list "group kind, region name, weight, min size"; the Amiga code walks that list and creates objects. No other code computes sizes.

Fonts: the layout is solved in pixels; row heights follow the window font (`ui.reaction.font`, default system default font), so on topaz 8 at 640x256 the lists have the row count RA-P2-6 asserts.

### 4.3 Where each behaviour is tested

| Behaviour | Lives in | Test kind |
|---|---|---|
| Layout to group plan | `ra_layout_plan` | host `ra-layout-plan` (all breakpoints, 640x256, 640x200, 700x400, 1280x800) |
| Action table to menu strip, toolbar, hints | `ra_menu_plan` (host, builds a neutral menu description) | host `ra-menu-plan` |
| `sel_view_row` to listbrowser node description (columns, generation, flags, pens) | `ra_row_adapt` | host `ra-rows` |
| Token to pen index mapping | `ra_pen_plan` | host `ra-pens` |
| Pref type to gadget kind for settings | `ra_pref_widget` | host `ra-settings-plan` |
| Mode to window geometry | `ra_mode_geometry` | host `ra-modes` |
| Class glue really runs on OS 3.2 | the plugin | one rig test per item that names `rig-ra-*`; only `rig-ra-open` is the reachability test |

### 4.4 Custom areas

`space.gadget` with `SPACE_RenderHook`: the hook draws a dirty rectangle from a chunky `sel_gui_canvas` (`gui_draw.h:25-43`) into the window RastPort with `WritePixelArray` (RTG) or the planar convert (AGA path, RA-P4-2). Grid, cover flow, artwork, viz and the pattern view all use this one area class; the hook is `ra_area_render`. Mouse and key events on the area go through `sel_gui_input_map` (`gui_input.h:87`) and then the same view code the desktop uses (`src/gui/views/`, DUI-P4-5). The area asks `sel_uim_budget_allows` before blur or shadow (INDEX8 refuses them, `uimodel.h:145-148`).

### 4.5 Tokens to pens

Tokens (`tokens.h:15-30`) map to pens as M4 section 5 says. Facts about what ReAction obeys: gadget colours follow the system pen scheme (screen DrawInfo), not tokens. So tokens colour only (a) custom areas and (b) per-cell/ per-node pens of the listbrowser where the class has them (to be confirmed in RA-P0-2). Theme presets other than the screen's own colours therefore change less than on the desktop (section 5, row "theme preview of whole window"). On a screen of 8 bits or fewer the pen plan uses `ObtainBestPen` from the Workbench palette; on 15 bits or more it uses the token RGB directly in custom areas.

## 5. What ReAction cannot carry (and what the plugin does instead)

| Desktop plan feature | Why ReAction cannot | Behaviour |
|---|---|---|
| Rounded corners, shadows, blur, gradient header tint from the dominant artwork colour (DUI-P4-7) | system gadgets draw bevel frames from the screen DrawInfo; only custom areas are free-form | custom areas keep them on RTG; gadgets keep OS look; header tint dropped |
| Tweens, hover animation, kinetic scroll, smooth scroll (DUI-P3-4/5) | ReAction gadgets redraw immediately; no animation hook in the classes | none; `ui.desktop.reduce_motion` has no ReAction counterpart |
| Per-token whole-window theming and live theme preview of all gadgets (DUI-P5-3) | gadget colours come from the screen pens | tokens apply to custom areas and listbrowser cell pens only; the four Amiga presets still work because they are the screen look |
| Artwork thumbnails inside list rows (Spotify-style rows) | listbrowser rows have one height set by the font; per-node images are small glyph-style images (to confirm RA-P0-2) | rows are text; artwork is shown in Home tiles, grid, cover flow and the Now Playing panel |
| Pixel-exact layout goldens shared with the desktop (DUI goldens) | gadget borders, fonts and pens differ per OS pref | the ReAction UI is checked by layout plans (host) and rig geometry, not by pixel hashes, except custom areas |
| Mini scope in the transport bar | needs a space.gadget; fits only when width allows | present when the transport row is at least 480 px wide, else hidden |
| Grid and cover flow, tile Home shelves | need a truecolor-capable bitmap area | RTG only; planar screens offer 4 styles |
| Tooltips with 600 ms delay (DUI-P3-6) | window.class has gadget hint info; delay is the system's | hints from `tooltip` field; delay not asserted (RA-P2-8) |
| Drag from list to sidebar playlist (DUI-P4-9) | depends on listbrowser drag support (RA-P0-3) | if absent: Add to Playlist menu and song menu only |
| Context menu on secondary click | Intuition has no context menu; menus are on the right mouse button | the right button opens the screen menu strip; the song menu is a menu item group built from the same table plus a popup chooser opened by an action (RA-P3-8) |
| Spotify sidebar look with icons and labels | icons need images (lucide names do not exist as Amiga images) | sidebar entries are text (labels), icon sidebar breakpoint shows the first letters? No: full English words are required, so the icon-sidebar breakpoint uses the compact one-column layout instead (RA-P2-2) |
| Clipboard URL watch (DUI-P5-10) | clipboard.device text is available but no change notification at class level | not in this plan (Q7) |
| Media keys (DUI-P5-12) | no media keys on Amiga keyboards | commodity hotkeys (M4-P12-12) instead |

HippoPlayer and bbmp parity: rows with Selecta column 7/9 that are UI are mapped in section 8 coverage. Rows named ARexx, ToolTypes, `LoadSeg`, players are not UI and belong to other plans.

## 6. Phased checklist

IDs are stable. Host tests are labelled `ra`; every item names its test and the numbers it asserts. Items marked (rig) cannot be run until the Phase 0 facts exist. Nothing here is run on a rig by this plan's author.

### Phase 0: facts and red tests

- [ ] RA-P0-1 Class and library versions on rig 2 (OS 3.2, uaegfx): `window.class`, `layout.gadget`, `listbrowser.gadget`, `chooser.gadget`, `clicktab.gadget`, `space.gadget`, `scroller.gadget`, `button.gadget`, `string.gadget`, `integer.gadget`, `checkbox.gadget`, `slider.gadget`, `label.image`, `glyph.image`, `bitmap.image`, `penmap.image`, `requester.class`, `intuition.library`, `gadtools.library`, `asl.library`, `workbench.library`, `commodities.library`, `datatypes.library`, `diskfont.library`, `picasso96api.library`. Written to `thoughts/shared/research/2026-10-25_reaction-class-facts.md` with the exact commands (`Version FULL` and `Avail` output). Class versions on rig 2 are unknown today. Test: `rig-ra-facts` prints each name with its version through `selecta-cli machine`/`version`; asserts all 7 baseline classes found (7 of 7) and records the rest as present or absent. Also run on the Replay by Johan if he agrees (Q2).
- [ ] RA-P0-2 Capability probe, one tiny program on the rig per capability, results into the same research file: (a) listbrowser per-cell FG/BG pen and per-node image, (b) listbrowser hierarchy depth and expand/collapse event, (c) listbrowser row height rule, (d) space.gadget render hook and mouse events, (e) clicktab on a 640x256 hires screen, (f) bubble help (hint info) delay. Test: `rig-ra-probe` prints six yes/no lines; asserts 6 lines present (the answers drive section 5 and are not assumed).
- [ ] RA-P0-3 Drag and drop and reorder: probe whether listbrowser supports drag of rows and drop on another gadget. Test: `rig-ra-probe-drag` prints `drag=yes|no`; asserts the line exists; RA-P3-6 (queue reorder) and RA-P3-7 (playlist drag) pick their method from it.
- [ ] RA-P0-4 Live relayout: probe how to swap a layout.gadget group inside an open window at a breakpoint change (LAYOUT_RemoveChild/AddChild plus RethinkLayout, or close and reopen). Test: `rig-ra-probe-relayout` swaps three groups 100 times; asserts no Enforcer-style leak (`AvailMem` before equals after within 4 KB) and window stays open; prints method used.
- [ ] RA-P0-5 Screen facts for the target sizes: Workbench title bar height, window border widths and the system default font on rig 2 at 640x256 hires (PAL), 640x200 (NTSC) and the Replay sizes already seen (680x270, 640x400). Test: `rig-ra-probe-screen` prints inner window width and height and text rows at the default font per size; asserts the 640x256 inner height is at least 200 (else compact layout needs a change, reported not hidden).
- [ ] RA-P0-6 Red tests: every test named in this section exists and fails with a missing-feature reason. Test: `ctest -L ra -N` lists every test name parsed from this plan (count compared with the parsed table, expected = number of distinct test names in section 6); each fails red.
- [ ] RA-P0-7 Ratchet and ledger: RA IDs appended to `tests/ratchet/unimplemented.txt` when each phase starts (not before, as the ledger note on I344 says); M4-P9-1 to M4-P9-3 stay `[>]`. Test: `ratchet` passes with the RA IDs counted and the M4 IDs not counted.

### Phase 1: plugin skeleton and reachability

- [ ] RA-P1-1 Descriptor and CMake: `plugins/ui/reaction/` with the kind-5 descriptor (`SEL_UI_NAME` "ReAction"), option `SELECTA_UI_REACTION`, Amiga-only class glue, host build compiles the pure mapping functions. Test: `ra-descriptor`: the registry lists plugin id `reaction` with hooks OPEN, ON_EVENT, WAIT, CLOSE, STATS, VIEW_STYLES = 6 hooks; host `--ui reaction` fails with the reason "ReAction needs AmigaOS" (not a crash).
- [ ] RA-P1-2 Open with class checks: open libraries and classes with `OpenLibrary`/`OpenClass`-style lookups, version floors from RA-P0-1; any missing item gives a registry "unavailable" reason naming it, and the text UI keeps running (M4-P11-2 shape, Q26). Test: `rig-ra-missing`: with `space.gadget` renamed away, `selecta --ui reaction` prints the reason, shows a requester when started from Workbench, exit code 0, text UI running (`STAT_FRAMES` of text UI above 0).
- [ ] RA-P1-3 REACHABILITY: the window opens through the shell. Test: `rig-ra-open` on rig 2: `selecta --ui reaction`; asserts `ui_open_reaction=1` (sentinel, 4.1), window inner size equals the Workbench screen area below the title bar (full default), menu item count equals the number of action-table rows with a menu path, `STAT_FRAMES` above 0 after one `NEXT`, and the window title shows the song title from `sel_uim_frame`.
- [ ] RA-P1-4 UI_WAIT loop on window and timer signals: wait on the window signal mask, the engine event signal and a timer; progress updates at most 4 per second while playing, 0 while idle (as DUI section 1 item 4). Test: `ra-wait-budget` (host, fake Amiga shim for signals): 10 s of idle gives 0 redraw batches after the first; playing gives at most 40 in 10 s.
- [ ] RA-P1-5 One pump per loop: `sel_uim_update` once per pump and a gadget update only for a set dirty bit. Test: `ra-dirty-map` (host): the dirty bit to gadget set mapping table; asserts 6 regions map to disjoint gadget sets and a song change updates exactly the SONG and PROGRESS sets.
- [ ] RA-P1-6 Close paths: Ctrl-C, the window close gadget and `UI_CLOSE` release window, objects, libraries, timer and message ports and print the stats line (M4-P5-10 shape for the ReAction plugin). Test: `rig-ra-close`: Ctrl-C ends the process in 2 s, stats line printed, `amiga_windows` lists no Selecta window, `AvailMem` after equals before within 4 KB.

### Phase 2: frame, layout, modes, menus

- [ ] RA-P2-1 `ra_layout_plan`: layout to group plan for the three breakpoints. Test: `ra-layout-plan`: at 1280x800 the plan has 3 columns and 1 player row; 1000x600 has 2 columns; 800x500 has 1 sidebar column (icon-sidebar breakpoint, see RA-P2-2); weights are proportional to the layout regions within 1 unit; `sel_uim_layout_check` passes for every size.
- [ ] RA-P2-2 Compact full-screen layout (Q24 answer): one content column, bottom icon row with five buttons labelled Home, Library, Search, Queue, Now Playing above the transport; chosen below 700 px width and at the 640x256 and 640x200 sizes (DUI-P4-20 owns the region list). Labels are full words as buttons in a row, not icons. Test: `ra-compact-plan`: at 640x256 the plan has exactly 3 rows (content, icon row, transport) and 5 icon-row buttons whose labels equal the five names; at 640x200 the content row is shorter and its list is flagged scrolling; at 639x256 `open` returns the "below 640 px: text UI only" reason.
- [ ] RA-P2-3 Menu strip from the action table: menu path "Menu/Item" gives menus and items, key field gives the shortcut letter or text through `txt_bindings_key_for`, `line` is the activation. Test: `ra-menu-plan`: item count equals action rows with a menu path (count from `sel_uim_action_at`, not from a literal); every item's command line is a COMMANDS verb or PREF_SET (`sel_uim_actions_check` returns 0).
- [ ] RA-P2-4 Transport bar: previous, play/pause, next, shuffle, repeat, seek `slider.gadget` bound to `progress_q16`, volume slider, clock labels from `clock` and `remaining`, subsong chooser. Test: `rig-ra-transport`: clicking play sets engine state playing, next advances `queue_playing` by 1, shuffle toggles `random_on`, slider drag to 50 percent seeks to within 1 second of half the length.
- [ ] RA-P2-5 Keyboard: window RAWKEY goes through `sel_gui_input_map` and `txt_bindings_lookup`; unbound keys reach the focused gadget. Test: `ra-keys` (host, event list golden): Down, Down, Return, Tab, Esc, Space map to the same action names as the desktop golden (`dui` key golden reused).
- [ ] RA-P2-6 Row count at the design size: on the 640x256 compact layout with the system default font, the list shows the rows computed by the plan. Test: `rig-ra-small`: on a 640x256 screen asserts window inner size from RA-P0-5, list row count equals `floor(list_height / row_height)` from the plan, no gadget overlaps another (gadget rectangles from `amiga_ui_list` vs plan), and at 640x200 the list scroller is enabled.
- [ ] RA-P2-7 Display modes Full, Window, Mini player, Micro strip, Iconified (all except Lowered screen, M4 section 5): geometry from `ra_mode_geometry`, switch by `PREF_SET ui.reaction.mode`, by Right Amiga + 1 to 5 and Right Amiga + I, by the zoom gadget and by a Display Mode submenu. Test: `ra-modes` (host): geometry per mode (mini height at most 64 plus border, micro height one title-bar high, 280 wide); `rig-ra-modes` cycles all five through the four entry points and asserts each geometry and, in Iconified, `ui_appicon=1`; shares M4-P5-4's invariant: cursor key unchanged (`sel_engine_view_cursor_key`), underruns 0, `position_ms` monotonic.
- [ ] RA-P2-8 Hints from the action table: each toolbar button carries its `tooltip` string; the label is the `label` field. Test: `ra-menu-plan` also asserts hint count equals button count and every hint equals the table text; `rig-ra-hint` asserts hint text of the Play button on screen (delay not asserted, section 5).
- [ ] RA-P2-9 Window position, size and mode remembered (DUI-P5-1 shape). Test: `rig-ra-remember`: move and resize, quit, restart; asserts equal geometry and mode, and the last view.
- [ ] RA-P2-10 Pubscreen: opens on the screen named by `PUBSCREEN` (M4-P7-1); default Workbench. Test: covered by `rig-m4-pubscreen` run with `--ui reaction` (the M4-P7-1 test gains the third UI); asserts screen name `Selecta`.

### Phase 3: views and features

- [ ] RA-P3-1 `ra_row_adapt`: `sel_view_row` to listbrowser node description (column texts from the F48 format, generation for tree depth, state flags, pens). Test: `ra-rows` (host): a fixture of 1000 rows gives 1000 nodes; tree rows carry generation equal to depth; the playing row is flagged; texts equal `sel_format_expand` output for the fixture format.
- [ ] RA-P3-2 Library lists in regular, tree, drilldown, compact; styles chosen with `sel_engine_set_ui_view_styles`. Test: `rig-ra-styles`: the style property test (cursor key kept across all offered styles) asserts the offered count = 6 on an RTG screen and 4 on a planar one (`sel_view_style_label` names), and `sel_engine_view_cursor_key` equal before and after each switch. Replaces M4-P9-2's rig test; same numbers.
- [ ] RA-P3-3 Drilldown as `clicktab.gadget` over listbrowsers (one tab per level, breadcrumb text from `sel_uim_queue_title`). Test: `ra-drill-plan` (host): drilling twice gives 3 tabs with titles equal to the breadcrumb; going back removes the last tab.
- [ ] RA-P3-4 Home view: shelves of tiles (RTG) or lists (planar) from the `HOME` source. Test: `rig-ra-home`: the fixture history gives the "Jump back in" shelf with 6 entries in order, matching `dui-home`'s fixture.
- [ ] RA-P3-5 Search: debounced string gadget, scope chooser, grouped results. Test: `rig-ra-search`: typing `format:mod` gives the fixture mod count; `STAT_LIBRARY_SEARCHES` grows by the debounced count (1 per pause), the same numbers as `dui-search`.
- [ ] RA-P3-6 Queue view with group headings (Now playing, Next up, Later), play next, remove, undo and redo, reshuffle; reorder by drag or Move Up / Move Down (RA-P0-3). Test: `rig-ra-queue`: play next then undo restores the queue order (queue hash equal), same assertion as `dui-queue`.
- [ ] RA-P3-7 Playlists: folder tree as listbrowser generations, create, rename, delete with confirm, smart playlists. Test: `rig-ra-playlists`: create in a folder, reload; the folder holds it; `STAT_FOLDER_OPENS` counts the open.
- [ ] RA-P3-8 Song context menu (I305 shape): every page that lists a song opens one menu built from one table. Method on Amiga: the screen menu strip gets a "Song" menu rebuilt for the selected row, plus a popup `chooser.gadget` opened by the Song Menu action. Test: `ra-song-menu` (host): the item list equals the desktop `g_menu_items` order for the same row kind; `rig-ra-song-menu` asserts the Play Next item changes the queue by 1.
- [ ] RA-P3-9 Now Playing panel: artwork area, metadata list, synced lyrics (selected row = `lyric_slot` minus 1), up next, rating and like. Test: `rig-ra-nowplaying`: the LRC fixture advances the selected lyric row at the fixture times; `STAT_META_PANEL_ROWS` equals the metadata list row count.
- [ ] RA-P3-10 Likes and ratings (I306 shape): heart shown for liked songs in lists (a column mark) and the Now Playing like button reflects the state in the same pump as a LIKE toggle; five rating marks via a `chooser.gadget` with stored value and a second pick clearing it. Test: `rig-ra-likes`: toggle LIKE, assert the button state and the row mark change within one pump (`STAT_FRAMES` plus 1); rating 4 then 4 again clears to 0.
- [ ] RA-P3-11 Song info page: module info, STIL, composer, wrong and skip flags, notes. Test: `rig-ra-songinfo`: setting the wrong flag with a note writes the row `selecta-cli wrong` lists.
- [ ] RA-P3-12 Hide what cannot be carried: styles, mini scope and drag entries hidden or replaced exactly per section 5; the registry mask and the plan agree. Test: `ra-capabilities` (host): the capability table (one row per section 5 line) matches the offered styles mask and hidden-widget list; a row without a behaviour fails the test.
- [ ] RA-P3-13 Tracker tabs: pattern view, samples, instruments with click-to-play, phase view, chip registers. Pattern cells as text from `sel_uim_cell_fields` in a space.gadget with a monospace font. Test: `rig-ra-tracker`: a fixture MOD shows the pattern row the engine reports at a fixed clock; clicking a sample plays it (sample-play count 1).
- [ ] RA-P3-14 Empty, loading and error states (DUI-P4-21 shape): empty library, queue, search, playlist, lyrics; slow source shows a loading row; failures from `SEL_EVT_ERROR` show a requester or the status line. Test: `ra-empty-states` (host, row adapter level): each empty source gives one placeholder row with the specified text.
- [ ] RA-P3-15 Text overflow rules (DUI-P4-17 shape): titles ellipsize at the end, paths in the middle, numbers and times never truncate; column widths sized by the plan. Test: `ra-ellipsis` (host): fixture strings give the same truncated output as the desktop text helper (`gui_text.h`) for the listbrowser column width in pixels.

### Phase 4: custom areas

- [ ] RA-P4-1 space.gadget render hook `ra_area_render`: chunky canvas to RastPort with dirty rectangles. Test: `rig-ra-area`: draws a known golden into the area; the screen readback hash equals the host `mem` hash of the same canvas (`sel_gui_hash`) at the area size, as M4-P5-1 does for the custom UI.
- [ ] RA-P4-2 Planar (AGA) path for custom areas: INDEX8 canvas converted to planar for the window's depth, pens by `ObtainBestPen` (RA-P5-4). Test: `rig-ra-area-aga` on a 256-colour screen: readback hash equals the host INDEX8 golden after palette mapping; mapped colour count at most 64 for the area (a budget Johan sets, Q8).
- [ ] RA-P4-3 Grid and cover flow in space.gadget on RTG, using the desktop draw code; key and mouse through `sel_gui_input_map`. Test: `rig-ra-grid-coverflow`: the hash equals the host golden for both; arrow right in cover flow moves the centred album by 1.
- [ ] RA-P4-4 Visualizers in space.gadget: the viz plugin found with `find_plugin`, `sel_surface` INDEX8 or RGBA32 by screen depth, colours from tokens mapped to pen indices or RGB. Test: `rig-ra-viz`: with a scope plugin `STAT_VIZ_FRAMES` grows at least 25 per second of song time on rig 2 (rate checked at the Q15 number for the tier); `VIZ_FRAME` count above 0 (this replaces M4-P9-3's viz half).
- [ ] RA-P4-5 Artwork in the Now Playing area from `sel_ui_art` through `sel_gui_art_cache_get`, generated art when none (`sel_gui_genart_draw`). Test: `rig-ra-artwork`: a song with a fixture cover shows it (area hash differs from the generated tile); a song without a cover shows `sel_gui_genart_draw` output (hash equals the host golden).
- [ ] RA-P4-6 Fullscreen visual mode (F38): the window content goes to one viz area, any key restores. Test: `rig-ra-visual`: the mode `visual` (`SEL_UIM_LAYOUT_VISUAL`, `uimodel.h:44`) gives one region; a key press restores the previous layout hash.

### Phase 5: settings, overlays, theme, fonts

- [ ] RA-P5-1 Settings window: `clicktab.gadget` pages generated from `prefs.def` groups, control kind per pref type by `ra_pref_widget`, every change sent as `PREF_SET` at once. Test: `ra-settings-plan` (host): page count equals `prefs.def` group count and control count equals keys marked `ui` in `prefs.def` (counted from the table); `rig-ra-settings`: changing the first control of each page fires `PREFS_CHANGED` with that key.
- [ ] RA-P5-2 Command palette and command line: `string.gadget` plus result listbrowser driven by `txt_palette_*` and `txt_cmdline_*`, history kept in the UI profile. Test: `rig-ra-overlays`: palette query `shuff` runs shuffle; `:pla` plus Tab completes to the fixture action; history survives restart.
- [ ] RA-P5-3 Help window listing every action: rows from `sel_uim_help_row`. Test: `ra-help-rows` (host): row count equals `sel_uim_action_count()`.
- [ ] RA-P5-4 Token to pen mapping `ra_pen_plan` and presets: Selecta default and the four Amiga presets (Workbench 1.3, Workbench 3.1, Copper, Guru Meditation) affect custom areas and listbrowser cell pens; user screen pens win elsewhere (M4 section 5 theme row). Test: `ra-pens` (host): each preset gives its pen table golden; mapped colour count at most 32 for the UI and at most 256 in total; every token used by a custom area has a mapping.
- [ ] RA-P5-5 Key binding editor and Load/Save prefs actions reachable (DUI-P4-14). Test: `rig-ra-keys-edit`: rebind Next to a key, press it, `queue_playing` advances.
- [ ] RA-P5-6 Dialogs: confirm and text prompt as `requester.class`, playlist and collection pickers as listbrowser windows, guard on destructive actions, first-start flow. Test: `rig-ra-dialogs`: Delete Playlist asks to confirm; Escape leaves the playlist count unchanged; empty prefs directory starts the first-start sequence (dialog count golden).
- [ ] RA-P5-7 Activity panel (downloads and extra files): listbrowser with progress column, Stop and Continue. Test: `rig-ra-activity`: fake slow source with three jobs shows 3 rows; Stop sets the job state to stopped; Continue resumes.
- [ ] RA-P5-8 Fonts: list font and interface font through the asl font requester (M4-P12-3's Amiga part, applied to this plugin), Big Buttons doubles the transport button height. Test: `ra-fonts` (host plan level): `PREF_SET ui.reaction.font topaz.font/8` gives list row height 8 plus the plan spacing; Big Buttons gives transport height at least twice the default; `rig-ra-fonts` asserts the same on the window.
- [ ] RA-P5-9 Info line and output indicator (DUI-P5-2): time, position/length, clock/free memory, name, output name (AHI mode, Paula), download byte count. Test: `rig-ra-infoline`: each of keys 7, 8, 9, 0 gives its golden string from the shared model; output text equals the active output name.
- [ ] RA-P5-10 External lookup (DUI-P5-7): the action opens a URL through openurl.library, else a requester shows the URL (Q22). Test: `rig-ra-lookup`: URL equals the golden built from the song key; a fake openurl.library counts one call.
- [ ] RA-P5-11 Stored programs on F1 to F10 (DUI-P5-8) and columns/format prefs (DUI-P5-9). Test: `rig-ra-fkeys` (F3 plays the saved playlist, `queue_playing` = its first entry) and `ra-rows` format case (a bad format gives the default text and one warning).

### Phase 6: OS integration (calls the M4-P7 helpers)

- [ ] RA-P6-1 AppWindow and AppIcon: files dropped on the window or icon add to the queue; Iconified shows an AppIcon and double-click restores. Test: covered by `rig-m4-appdrop` with `--ui reaction` (M4-P7-2 gains the UI); asserts `queue_count` plus 1 and entry path equal to the dropped file; `ui_appicon=1` in Iconified.
- [ ] RA-P6-2 asl requesters for Add Files, Add Folder, Save Playlist. Test: `rig-m4-asl` with `--ui reaction`; asserts `QUEUE_ADD` count equals files chosen.
- [ ] RA-P6-3 Workbench start: no stdout; messages as requesters (M4-P7-6). Test: `rig-m4-wbstart` with `--ui reaction` and `reaction.library`-class missing (RA-P1-2 reason shown).
- [ ] RA-P6-4 Commodity and hide: global hotkeys, Hide Window key H, commodity Show (M4-P12-12) for this window. Test: `rig-m4-commodity` with `--ui reaction`: hotkey triggers `NEXT`; H gives open window count 0, Show restores.
- [ ] RA-P6-5 Pause drawing when the screen is behind (M4-P7-4 shape, applies to the window being inactive on a covered screen). Test: `rig-ra-screen-back`: with the screen sent behind for 5 s `STAT_FRAMES` grows by at most 1 and viz taps keep counting.

### Phase 7: performance, regressions, landing

- [ ] RA-P7-1 Knob sweep: every `ui.reaction.*` pref and every token the plugin uses moves the output (layout plan hash, pen table or custom-area hash) or carries a written exemption in code; the detector is first run on one live knob (`ui.reaction.mode`) and one dead stub knob. Test: `ra-knob-sweep` (host): LIVE for the live knob, DEAD for the stub, 0 unexplained DEAD.
- [ ] RA-P7-2 Memory and speed on the rig: after open, steady playback with scopes does not grow `AvailMem` by more than 8 KB over 10 minutes; input-to-redraw at most 40 ms for list scroll; fast-RAM use at open recorded. Test: `rig-ra-perf`: asserts the three numbers; plugin size counted into M4-P11-3's image budget (`amiga-m4-size`).
- [ ] RA-P7-3 Switching: `UI_SWITCH reaction` from and to the other UIs during playback keeps the song, cursor key and queue (counted in M4-P11-1's five-UI run). Test: `rig-m4-switch-all` with the reaction step; asserts one song opened, underruns 0, cursor key unchanged, `ui_switches` equals the scripted count.
- [ ] RA-P7-4 Manual check by Johan on the Replay: `selecta --ui reaction` at the Workbench size; steps: open, browse, drilldown, queue, search, Now Playing, scopes, switch mode, compact layout on a 640x256 screen, iconify and restore. PASS = no overlapping gadgets, all five bottom buttons visible and reachable at 640x256, scopes move, no crash on close. Marked LIVE only when the build on the Replay is the one under test (image date stated).
- [ ] RA-P7-5 Ratchet at zero RA IDs and M4 plan row text matches this plan (Phase 9 rows `[>]`). Test: `ratchet` passes with 0 RA IDs.

Count: Phase 0: 7, Phase 1: 6, Phase 2: 10, Phase 3: 15, Phase 4: 6, Phase 5: 11, Phase 6: 5, Phase 7: 5. Total 65 (counted twice: once from the list above, once from the heading numbers RA-P0-7, P1-6, P2-10, P3-15, P4-6, P5-11, P6-5, P7-5).

## 7. Findings: where code contradicts the M4 plan

- F1 Display modes. M4 section 3.4 defines pref values `full|window|lowered|mini|micro|icon`; `include/selecta/uimodel.h:44` defines layouts full, window, mini, visual and `prefs.def:12` only `ui.desktop.mode`. Micro, Iconified and Lowered do not exist in the model, and `visual` is not in the M4 table. The mode set must be added to `src/uimodel/` (M4-P5-3 owner) before RA-P2-7. Which should change: the code (add the modes), keep `visual` as the F38 layout.
- F2 Layout tree. M4 section 5 says "layout tree -> layout.gadget with weights"; `uimodel.h:114-126` and `src/uimodel/layout.c:35-78` give a flat region list of five regions at most (suggestion, sidebar, main, detail, player) with no weights or nesting. The plan derives the groups from region sizes (4.2). The M4 text should say "region list".
- F3 Compact layout is not implemented. `layout.c:29-33` returns the mini-suggestion breakpoint under 700 px with a banner, and the compact layout of DUI-P4-20 and the 640x256 golden of DUI-P1-7 are open. RA-P2-2 depends on them.
- F4 Sentinels. The M4 plan's `ui_open_mui=1` and `ui_open_custom=1` do not exist in code (no `ui_open_` in `plugins/`, `src/`, `tests/`); 4.1 defines one derived sentinel.
- F5 Rig name. M4 plan tests say `rtg-060`; the real RTG targets are rig 2 and the Replay. M4 plan rows are not edited here beyond Phase 9.
- F6 Icon sidebar breakpoint. `uimodel.h:108` names an icon sidebar for 700 to 899 px; the project requires full English words for labels, and lucide icons do not exist on the Amiga. RA-P2-1 therefore maps that breakpoint to labelled entries in a narrow column (decision in Q5).

## 8. Open questions for Johan

Each has a recommended answer; the decisive trade-off is in the last sentence. Not asked, to be put to him by the caller.

1. Minimum OS for the ReAction UI. Recommended: OS 3.2 only, exactly M4 Q9, with the class versions of RA-P0-1 as hard floors. Trade-off: OS 3.1 users with ClassAct are excluded; they keep MUI, GadTools and the text UI.
2. Run the Phase 0 facts on the Replay as well as rig 2? Recommended: yes, one run of `rig-ra-facts` and `rig-ra-probe` by you. Trade-off: costs you 10 minutes; without it, class versions on real hardware stay unknown and rig 2 may differ.
3. Allow classes beyond the seven baseline ones (button, string, integer, checkbox, slider, label, glyph, bitmap, penmap, requester) as part of "OS 3.2 ReAction classes"? Recommended: yes, they ship with 3.2 and the UI cannot work without buttons and sliders. Trade-off: the list in M4 Q9 was shorter; strictly seven classes cannot make a transport bar.
4. Sidebar at 700 to 899 px (icon sidebar breakpoint). Recommended: a narrow column with full-word labels, no icons (F6). Trade-off: the column is wider than the model's icon width, so the main region loses about 80 px; the alternative, icons made from the first letter, breaks the full-words rule.
5. Context menu on right click. Recommended: the right mouse button opens the screen menu strip (Amiga convention), with a "Song" menu rebuilt for the selected row, as M4 3.2 says for toolkit UIs. Trade-off: no popup at the pointer.
6. Gradient/rounded look in custom areas. Recommended: custom areas (grid, cover flow, viz, artwork) use the desktop draw code with rounded corners on RTG; gadgets keep the OS look. Trade-off: a window that mixes two styles; making gadgets match would need custom gadget classes, which is the custom UI's job.
7. Clipboard URL watch (DUI-P5-10) in the ReAction UI. Recommended: not in this plan. Trade-off: one feature fewer than the desktop; clipboard.device has no change notification, so it needs polling that costs CPU on a 68020.
8. Colour budget for a custom area on 256-colour screens. Recommended: 64 colours for the area (viz or artwork) from the shared pen table, leaving the rest to the Workbench palette. Trade-off: fewer colours for artwork; more would steal pens from Workbench (RA-P4-2 uses the number).
9. Sentinel mechanism shared with MUI and custom UI. Recommended: derive `ui_open_<id>=1` in one test helper from `STAT_BACKEND`, `STAT_FRAMES` and the registry (4.1), used by M4-P8-1 too. Trade-off: a small helper change in the MUI plan; a per-plugin new stat tag adds a tag for each UI.
10. Replay manual check timing. Recommended: RA-P7-4 only when RA-P1 to RA-P3 are green on rig 2. Trade-off: later feedback; earlier checks would test an unfinished window.

## 9. Coverage

### 9.1 M4 rows

| M4 row | Moved to | Note |
|---|---|---|
| M4-P9-1 `plugins/ui/reaction/` open test | RA-P1-1, RA-P1-3 | `rig-m4-reaction-open` replaced by `rig-ra-open` |
| M4-P9-2 listbrowser columns, hierarchy, drilldown clicktab | RA-P3-1, RA-P3-2, RA-P3-3 | same offered counts (6 RTG, 4 planar) |
| M4-P9-3 viz in space.gadget, display modes | RA-P4-4, RA-P2-7 | `rig-m4-modes --ui reaction` is the mode half, kept as a shared test with the ReAction step |
| M4-P7-1/2/3/4/6, M4-P11-1/3, M4-P12-12 (shared rig tests) | RA-P2-10, RA-P6-1 to RA-P6-5, RA-P7-2, RA-P7-3 | the ReAction UI is added to those tests; rows stay in M4 |

### 9.2 Desktop plan rows (section 3 views and Phase 4/5)

| Desktop row | RA row or reason |
|---|---|
| DUI-P4-1 frame | RA-P1-3, RA-P2-1 to RA-P2-4 |
| DUI-P4-2 Home | RA-P3-4 |
| DUI-P4-3 Library, browse, stats | RA-P3-1, RA-P3-2, RA-P3-3 (stats page as a listbrowser, same fixture) |
| DUI-P4-4, P4-5 styles, grid, cover flow | RA-P3-2, RA-P4-3 |
| DUI-P4-6 Search | RA-P3-5 |
| DUI-P4-7 Now Playing | RA-P3-9, RA-P4-5 (header tint: not carried, section 5) |
| DUI-P4-8 Queue | RA-P3-6 |
| DUI-P4-9 Playlists | RA-P3-7 |
| DUI-P4-10 Song info | RA-P3-11 |
| DUI-P4-11 Viz, fullscreen | RA-P4-4, RA-P4-6 |
| DUI-P4-12 Tracker tabs | RA-P3-13 |
| DUI-P4-13 Overlays | RA-P5-2, RA-P5-3, RA-P3-14 |
| DUI-P4-14 Settings | RA-P5-1, RA-P5-5 |
| DUI-P4-15 Mini and window mode | RA-P2-7 |
| DUI-P4-16 Song context menu | RA-P3-8 |
| DUI-P4-17 Text overflow | RA-P3-15 |
| DUI-P4-18 Remove and Clear Library | actions in the table via RA-P2-3; confirm in RA-P5-6; no separate test |
| DUI-P4-19 Liked state, rating | RA-P3-10 |
| DUI-P4-20 Compact layout | RA-P2-2 |
| DUI-P4-21 Empty states | RA-P3-14 |
| DUI-P5-1 Remembered state | RA-P2-9 |
| DUI-P5-2 Info line | RA-P5-9 |
| DUI-P5-3 Themes | RA-P5-4 (limited, section 5) |
| DUI-P5-4 Dialogs, first start | RA-P5-6 |
| DUI-P5-5 Activity | RA-P5-7 |
| DUI-P5-6 Fonts | RA-P5-8 |
| DUI-P5-7 Lookup | RA-P5-10 |
| DUI-P5-8, P5-9 F keys, formats | RA-P5-11 |
| DUI-P5-10 Clipboard URL watch | not in this plan: Q7 |
| DUI-P5-11 Drop, file dialogs | RA-P6-1, RA-P6-2 |
| DUI-P5-12 Media keys | not carried: no media keys; commodity hotkeys RA-P6-4 |
| DUI-P3 (tweens, kinetic scroll, hover) | not carried: section 5 |
| DUI-P6 (performance, noalloc, INDEX8 goldens, pixel proxy) | RA-P7-2 for the plugin; the library budgets are DUI items that this plugin consumes |

### 9.3 HippoPlayer and bbmp parity (UI-visible rows)

Rows come from `2026-10-22_parity-hippoplayer.md` sections Display/Screen, Scopes and info, Misc, and `2026-10-22_parity-bbmp.md` Browse and search, Queue and playlists, Misc; the full per-row mapping lives in the M4 plan sections 9.7 and 9.8 and the desktop plan 8.2, and the ReAction plugin gets each through the action table and the views above.

| Source group | RA row |
|---|---|
| Hippo resizable windows, zip (shrink) | RA-P2-7 (Mini, Micro strip), zoom gadget |
| Hippo any font, big buttons | RA-P5-8 |
| Hippo button tooltips | RA-P2-8 |
| Hippo PUBSCREEN, own screen option | RA-P2-10; own screen not offered (Q6 accepted: window UIs use the public screen) |
| Hippo filebox display and zoom | RA-P3-2 (list styles), RA-P2-7 |
| Hippo disable info scroll, info-line modes, AHI indicator | RA-P5-9 |
| Hippo module info window, SID/instrument/author/STIL info | RA-P3-11, RA-P3-13 |
| Hippo keep window positions and open states | RA-P2-9 |
| Hippo About window, Help key | RA-P5-3 (help window; About is an action in the table) |
| Hippo prefs window | RA-P5-1 |
| Hippo scopes (quadra, filled quadra, hippo, bars, spectrum, patternscope, several at once, size, priority, Z) | RA-P4-4, RA-P3-13; scope variants and several viz windows follow DUI-P4-11 (this plugin hosts the same plugins; one viz window per `space.gadget` window, a second window is a Window mode instance) |
| Hippo reqtools file requesters | replaced by asl (RA-P6-2) |
| Hippo iconify, AppIcon | RA-P6-1 |
| Hippo mouse wheel in lists | RA-P2-5 (wheel RAWKEY codes through `sel_gui_input_map`; M4-P5-12 code list) |
| bbmp browse, search, collections | RA-P3-2, RA-P3-5, RA-P3-4 |
| bbmp queue and playlists | RA-P3-6, RA-P3-7 |
| bbmp settings | RA-P5-1 |
| bbmp CLI options, Hippo ARexx and ToolTypes | not UI: M4-P7-5 and the players plan |
| bbmp packages, demo recorder, licence | not UI |

### 9.4 Count check

Section 6 counts: Phase 0: 7, Phase 1: 6, Phase 2: 10, Phase 3: 15, Phase 4: 6, Phase 5: 11, Phase 6: 5, Phase 7: 5 = 65. Desktop rows mapped in 9.2: 21 Phase 4 rows plus 12 Phase 5 rows plus the two grouped rows (P3, P6) = 35 rows, of which 3 are not carried (P5-10, P5-12, P3 effects) and 32 map to an RA ID. Counted from the list in section 6 and from the desktop plan's Phase 4 and 5 headings (21 and 12).
