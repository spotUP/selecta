---
date: 2026-10-30
topic: MUI vs Mac rich UI drift, region by region, root cause and a parity guard (ledger I734)
tags: [research, mui, desktop-ui, parity, uimodel]
status: complete (phase 1, code reading only; no rig run, no code edits)
---

# MUI vs Mac drift (I734)

Phase 1 audit from code only. The Mac layout is the truth (memory "MUI matches rich UI"). For each gap, the table names what Mac draws (file:line), what MUI builds or lacks (file:line), and a fix sketch. File shorthands:
- D = plugins/ui/desktop/desktop_ui.c
- M = plugins/ui/mui/mui_ui.c
- ML = plugins/ui/mui/map/mui_layout.c
- MV = plugins/ui/mui/map/mui_views.c
- T = tests/ui/mui_contract_test.c

Shared model: include/selecta/uimodel.h has 5 layout regions (suggestion, sidebar, main, detail, player) plus a topbar with 3 parts (home, field, browse). Breakpoints: three-column at 1100 px or wider, two-column at 900, icon sidebar at 700, mini suggestion below 700. Tiers are DEFAULT and BITMAP. src/uimodel/doors.c holds 7 doors: Home, Search, Browse Folders (page "folders"), Playlists, Queue, Now Playing, Song Information. include/selecta/uimodel/route.h holds the route table and page history. Its comment says the desktop UI and MUI share it, but **MUI does not include route.h**.

## 0. Direct evidence: mui-doors is red on main

`build/host/tests/mui_contract_test doors` (build of 2026-10-30 15:03) fails:
- `mui_contract_test.c:1106: door 2 label`
- `mui_contract_test.c:1109: five main-view lines`

I678i (5e96d7f3, 0a569270) renamed door 2 to "Browse Folders" with page "folders". MUI's `k_line` table in MV:246 (`mui_door_line`) still keys on "library", so on MUI the Browse Folders door returns NULL. `mui_engine_line(NULL)` returns 0 silently (M:355), so the button does nothing. The test hardcodes its own label list (T:1101), which is a second list, and nobody saw it turn red. `weights` and `topbar` pass.

## 1. Differences per region

### Sidebar (9 differences)

| # | Mac (D) | MUI | Fix sketch |
|---|---|---|---|
| S1 | Brand logo plus "Selecta" text at the top of draw_sidebar :9945 | mui_sidebar_group M:694 has door buttons only | Add an Area at the top showing the brand bitmap, with a Text "Selecta" fallback on the bitmap tier |
| S2 | Door rows with icon, label and a current-page highlight :9945 | mui_make_button_left text buttons with no icon and no selected state | Selected state from the current page (MUIA_Selected on a toggle button, or a radio-like group); door icons from the baked icon set as an image button |
| S3 | desk_door_shown :9006 hides Browse Folders when the sidebar is wider than 120 px, because the Your Library header replaces it | M:694 filters only by sel_uim_door_shown; no width rule | Move the rule into uimodel (`sel_uim_door_shown` takes the sidebar width or the library-header flag); both UIs call it |
| S4 | "Your Library" header with Create Playlist (+) and Expand/Collapse buttons, side_lib_draw :9764 (SIDE_HDR_PLUS/WIDE :550, I609) | absent | HGroup: Text "Your Library" (opens the Library page, I673), buttons "Create Playlist" and "Expand" |
| S5 | Chip row All/Playlists/Albums/Artists, single-chip chevrons (I610/I669), "By You" sub-chip (I678d), clear-X | absent | HGroup of toggle buttons; chip state from sel_uim browse/library item types (browse.h:63) |
| S6 | Library item rows in 4 views (compact, list, grid small, grid large; browse.h:38), bitmap-tier kind mark letter | absent (MUI shows library content only as the main AAList) | Sidebar AAList with 1-2 columns (mark, title/subtitle); grid views fall back to list on MUI 3.8 |
| S7 | Expanded table Title, Type, Date Added, Played | absent | When Expand is on, the sidebar AAList gets 4 columns and the slot weight grows (ML) |
| S8 | Empty states "Nothing here yet." / "No playlists yet." | absent | Text under the list when the list is empty |
| S9 | "Create your first playlist" card with button (I627) | absent | Group frame with Text plus button, same condition as Mac |

### Top bar (3 differences)

| # | Mac (D) | MUI | Fix sketch |
|---|---|---|---|
| T1 | Back/Forward round buttons draw_nav_arrows :8336 driven by route history (I678h) | none; mui_topbar_group M:838 has Home, field, Browse | Two buttons ahead of Home, enabled from route.h history; requires MUI to include route.h (see R-root) |
| T2 | Hide/Show Now Playing toggle in the top bar, I619 (draw_topbar :8403, np_toggle_rect) | none | Button at the right end; toggles the detail slot (ML) |
| T3 | Top bar on every tier and width (draw_topbar :8403) | RTG only (M:838); native screens have the search field in the tool row (mui_tool_group M:859, bitmap tier) | Accept for native low-res if the layout model says so; otherwise build the same group on AGA. Decide in the uimodel topbar field, not in M |

### Main area, per route (11 differences)

MUI has one generic AAList for every page (mui_main_group M:953) plus tool rows, crumb text and a hidden canvas. Mac has 13 pages (enum D:81) with their own draw functions and engine lines (desk_page_load D:8618).

| # | Mac page (D) | MUI | Fix sketch |
|---|---|---|---|
| P1 | Home: draw_home :8480, quick tiles draw_quick :3271, shelves draw_shelves :3310 | Home door sends VIEW_SOURCE home (MV:246) into the flat AAList | Home page group: quick-tile row (6 buttons with art) plus shelves as horizontal lists; at minimum, shelf header Text plus a short AAList per shelf with "Show all" |
| P2 | Shelf "Show all" page PG_SHELF | absent | Reuse the AAList with a crumb title; route target SHOW_ALL |
| P3 | Collection page (album/artist/composer) col_draw_shelves :4773 | absent; TRACK_ARTIST/TRACK_ALBUM/META_ARTIST/ALBUM_CARD targets have no handler | Header group (art, title, meta) above the AAList; driven by route.h targets |
| P4 | Browse tiles draw_browse :7984 (FORMATS_SHOW, COMPOSERS_SHOW, SOURCES) | Browse button sends the same lines (CTL_TOPBAR_BROWSE M:377) but the result lands in the flat list | Tile grid as a Virtgroup of image buttons, or a 2-column list on native |
| P5 | Search page draw_search :8071 with facets | Search door sends VIEW_SOURCE search, flat list | Facet chip row (Songs, Artists, Albums...) above the AAList |
| P6 | Browse Folders PG_FOLDERS (tree mode, VIEW_SHOW browse, COMPOSERS_SHOW, STATS_SHOW) | **door does nothing** (k_line "library" vs page "folders", MV:246) | Fix the k_line key now; tree indent column in the AAList |
| P7 | Playlists draw_playlists :5317, playlist page draw_playlist_page :5176 (header, art, owner, counts) | flat list plus tool buttons New/Open/Rename/Delete Playlist | Playlist header group; move the 4 tool buttons into the header or context menu as Mac does |
| P8 | Songs page PG_SONGS (Library rows) | flat list | Same AAList; columns from the shared view command table (viewcmds) |
| P9 | Now Playing page draw_nowplaypage :10274 | door returns NULL (MV:246); content only as a detail tab (M:1080-1115) | Main-area page that reuses the detail Now Playing group |
| P10 | Song Information page draw_songinfo :6295 | door returns NULL; detail tab only | Main-area page reusing the detail song-info group |
| P11 | Viz page draw_vizpage :6038 and Tracker page draw_trackerpage :6131 (@viz, @pattern, k_page_keys :10836) | only as detail tabs (Selecta_VizArea, tracker rows); no main page, and no detail below 1100 px (I735) | Main pages hosting Selecta_VizArea and the tracker list; reachable from the keys and the command line |

Also in the main area: the 12 tool buttons in mui_tool_group M:859 (Like, Remove From Queue, Clear Queue, Move Up/Down, Help, Settings, New/Open/Rename/Delete Playlist, Clear Library) have no Mac counterpart as a toolbar. Mac puts these in context menus, page headers and the player bar. This is an inverse drift: MUI shows controls Mac does not.

### Player bar (4 differences)

| # | Mac (D) | MUI (M:733) | Fix sketch |
|---|---|---|---|
| B1 | Player-left: 56 px cover art plus title/artist, draw_player_left :10001, at 900 px or wider | title/artist/format as Text in row 3; no art | Left column with an art Area (Selecta cover class) plus 2 Texts |
| B2 | 3-column bar: left (art, title), centre (transport above the seek row), right (volume and extras), draw_transport :10326 | 3 stacked rows: transport, clock/seek/volume, title/status | Single HGroup with 3 VGroups and Mac weights (from ML, not hardcoded) |
| B3 | Transport icons Shuffle, Previous, Play/Pause, Next, Repeat, plus minus/plus (draw_transport :10326) | text buttons; extras row album/speed | Image buttons from the baked icon set; order matches Mac; shuffle and repeat state shown as selected |
| B4 | Like (heart) next to the title on Mac (I-row Like) | Like is a tool button in the main area (M:859) | Move Like next to the title in the player-left column |

### Right panel / detail (3 differences)

| # | Mac (D) | MUI | Fix sketch |
|---|---|---|---|
| R1 | Detail panel draw_detail :1484 / draw_nowplaying :10064 at three-column; Now Playing, Song Info, Viz and Tracker also reachable as pages at any width | detail slot only at 1100 px or wider (ML); hidden on every native screen and RTG under 1100. No other path to Viz/Tracker (I735) | Route the content to main pages below 1100 (P9 to P11); the layout model already says detail is hidden, so MUI must offer the pages |
| R2 | Detail is one Now Playing column with sections (art, title, credits, queue preview) | AATabs with pages Now Playing, Song Info, Tracker, Viz (M:1080-1115) | Keep tabs as a MUI 3.8 compromise, but order and label them like the Mac sections; add the queue preview |
| R3 | Hide/Show toggle I619 | none | See T2 |

### Dialogs and overlays (4 differences)

| # | Mac (D) | MUI | Fix sketch |
|---|---|---|---|
| O1 | Command palette overlay (@palette, desk_draw_overlay :10945, palette.h) | none; MUI has a command-line string in the tool row | Popup window with a String plus AAList of filtered commands from palette.h (MUI includes cmdline/palette today: no) |
| O2 | Help overlay @help (D:10852) | mui_help_open M:1551 window | Same content source; check that both read the same key table (keybind.h, fkeys.h, both included) |
| O3 | Tooltips desk_draw_tip :10402 | MUIA_ShortHelp only where set | Set MUIA_ShortHelp from the same tip strings for every control |
| O4 | Song context menu (Mac right-click) | mui_song_context_menu M:257 | Compare entries in phase 2; not read in detail here |

Settings (mui_settings_open M:1664) and the Clear Library MUI_Request were not compared in detail.

### Totals

| Region | Differences |
|---|---|
| Sidebar | 9 |
| Top bar | 3 |
| Main area (per route) | 11 |
| Player bar | 4 |
| Right panel | 3 |
| Dialogs and overlays | 4 |
| **Total** | **34** |

## 2. The 5 worst

1. **P6: Browse Folders door does nothing on MUI**, and mui-doors is red on main (MV:246 k_line "library" vs page "folders"; NULL swallowed at M:355). This is a functional bug, not only a visual one.
2. **P11 and R1: Viz and Tracker are unreachable below 1100 px** (I735). Detail is hidden and there are no main pages.
3. **S4 to S9: the sidebar has none of Your Library** (header, chips, items, create card). It is the biggest visual gap and the Mac's main navigation since I673.
4. **P1 to P4: Home, Browse and Collection are flat lists.** MUI does not use route.h, so none of the route targets (shelves, Show all, album/artist links, browse tiles) work.
5. **B1 and B2: the player bar has no art and is not laid out like Mac** (3 stacked rows instead of left/centre/right).

## 3. Root cause

Mac rows land without a MUI counterpart because nothing forces one:

1. **Mac state lives in desktop_ui.c, not uimodel.** The page list (enum PG_* D:81), the door-hiding rule (desk_door_shown D:9006), the library sidebar (side_lib_draw D:9764) and route-history use are all local to D. MUI cannot follow what it cannot see.
2. **MUI keys off strings that go stale.** mui_door_line matches page names in a local k_line table (MV:246). When I678i renamed "library" to "folders", nothing failed at compile time.
3. **Missing handlers fail silently.** mui_engine_line(NULL) returns 0 (M:355).
4. **The contract test has its own list.** test_doors (T:1101) hardcodes labels, so a rename breaks the test instead of checking MUI against the model. It is now red, and nobody saw it, possibly because the label `mui` is not in the Mac track's gate.
5. **No test enumerates pages, routes or controls.** test_weights and test_topbar check regions and topbar parts only, so a new Mac page (Viz, Tracker, Shelf, Collection, Browse) can land with no MUI slot.

## 4. Parity guard design

Goal: fail when a region, door, control or route of the shared model has no MUI slot. Extend the existing mui-weights and mui-doors tests; add no second list.

**Model change (prerequisite).** Move the page list into uimodel: `sel_uim_page` table (new, in include/selecta/uimodel/route.h next to the route pages, or a pages.h it includes). Each entry has an id, a name (the k_page_keys name), its route page, the door that opens it (or none), and its region. desktop_ui replaces enum PG_* with it. Move the desk_door_shown width rule into `sel_uim_door_shown`.

**MUI side.** Key mui_door_line on the shared page id, not a string. Add `mui_page_slot(page_id)` in MV that returns the MUI group/view for that page, or an exemption code with a reason string. The map has one entry per page id, and the compiler can check it (a designated-initialiser array sized by `SEL_UIM_PAGE_COUNT`).

**Tests (extend, no new list):**
- test_doors (T:1101): loop `i < sel_uim_door_count()`. Derive the expected label from `sel_uim_door_at(i)`, not a literal. Require `mui_door_line(i) != NULL` or an exemption from the page slot.
- test_weights (T:627): loop over every tier and the 4 breakpoint widths. For each region and topbar part that `sel_uim_layout_compute_tier` shows, require that `mui_layout_map` shows a slot. For each region it hides, require that the page slot of every page in that region has a main-area fallback. This check catches I735.
- New case in the same binary: loop over every route target in route.h and require a MUI handler, or an exemption.
- Run mui-weights and mui-doors in the Mac track's pre-push gate, so a Mac row that adds a page or door goes red until MUI has a slot or a written exemption.

Expected state: red now (folders door, Viz/Tracker below 1100, route targets); green after the fix rows below.

## 5. Proposed fix rows (for the ledger; not added here)

1. Fix the Browse Folders door: k_line key "folders", derive test labels from the model (P6; turns mui-doors green).
2. Parity guard: sel_uim_page table, mui_page_slot, the extended tests, gate wiring (section 4).
3. Viz and Tracker main pages, plus Now Playing and Song Info as pages (P9 to P11, R1; I735).
4. Sidebar Your Library: header, chips, item list, empty states, create card; door hiding through uimodel (S3 to S9).
5. MUI includes route.h: Back/Forward, Show all, collection pages, track links (T1, P2, P3).
6. Home shelves and quick tiles (P1); Browse tiles (P4); Search facets (P5).
7. Player bar: player-left art and title, 3-column Mac order, icon transport, Like beside the title (B1 to B4).
8. Sidebar brand and door icons with selected state (S1, S2).
9. Overlays: command palette, tooltips from shared strings (O1, O3); Now Playing toggle (T2, R3).
10. Review the 12 MUI tool buttons against Mac's context menus and page headers (inverse drift).

## 6. Regions that need a rig look

These need screenshots, because the code alone cannot show the gap:
- Sidebar (AGA and RTG)
- Player bar (AGA and RTG)
- Main area: Home and Browse
- Detail panel on RTG at 1100 px or wider, plus confirmation that it is absent below 1100
- Top bar on RTG

Not read in this pass: settings dialog, context menu entries, the MUI help content.
