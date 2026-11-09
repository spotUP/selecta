---
date: 2026-11-08
status: done
row: I1170
---
# MUI against desktop UI: parity gap table

Host-only read of the code, nothing built or run. P = present, ~ = partial, M = missing. "unverified" = read from code, no rig look.
Inputs: 2026-10-27-mui-parity-inventory.md (item list), 2026-10-31-I799-mui-inverse-drift.md, the I734 ledger row (34 differences, fixed by I791-I799 and I735; the drift doc 2026-10-30-mui-mac-drift.md is not in the tree).

## Where each count comes from

- Actions: `k_actions[]` in src/uimodel/actions.c:10, 54 entries (parsed by a script over the table fields, not grep); `k_exempt[]` 21 engine verbs, not UI (include/selecta/commands.def has 128 SEL_CMD verbs; the exempt table is the gate).
- Pages and doors: `g_pages[]` in src/uimodel/pages.c (SEL_UIM_PG_COUNT 14, include/selecta/uimodel/pages.h:15-29); doors in src/uimodel/doors.c, 7. MUI side: `g_page_slots[SEL_UIM_PG_COUNT]` in plugins/ui/mui/map/mui_views.c:278 (a page without a slot or a written exemption fails to compile, I792).
- Settings: `g_pref_rows[]` src/uimodel/features.c:88, 19 rows (the MUI window iterates the same rows, mui_ui.c:2744-2756).
- Keys: the key field of the same 54 actions (12 single char, 28 multi char, 14 none), `k_page_keys[]` desktop_ui.c:13258 (6).
- Sub-features: the counts in the 2026-10-27 inventory (home chips, lib_mode_name, queue/playlist/song-info button sets, overlays, dialogs), spot-checked in desktop_ui.c.

## Table

### Keyboard and navigation
| Item | Desktop | MUI | State | Row |
|---|---|---|---|---|
| 12 single-char action keys | desktop_ui.c:13260 desk_bind_actions | Menuitem shortcut, mui_menu.c:63, mui_ui.c:407 | 12 P | - |
| 28 multi-char action keys (Space, Enter, Home, End, M-*, C-*, the 8 C-n layout/viz keys) | desk_bind_actions reads every key | mui_keys_build (map/mui_controls.c:147) is called only by tests/ui/mui_p8_test.c:50, never by mui_ui.c; a menu shortcut needs a one-char key (mui_menu.c:63) | 28 M | I1171 |
| Tab, Shift+Tab, arrows by active object | desktop focus | no MUIA_CycleChain in plugins/ui/mui | 1 M | I1165 (open) |
| Page keys @home @library @folders @info @viz @pattern | desktop_ui.c:13258 | @viz, @pattern in k_panes (mui_views.c:311-312); the other 4 only by sidebar doors, key unverified | 2 P, 4 ~ | I1171 |

### Pages (14) and doors (7)
| Item | Desktop | MUI | State | Row |
|---|---|---|---|---|
| 7 doors | doors.c | g_page_slots mui_views.c:279-286, sidebar I793 | 7 P | - |
| Home, Search, Folders, Playlists, Queue, Now Playing, Song Info, Viz, Tracker, Browse (10 pages) | pages.c | mui_views.c:279-290 | 10 P | rig looks I794-I796, I735 |
| Songs page | PG_SONGS | written exemption mui_views.c:285 | 1 ~ | - |
| Settings page | desktop_ui.c:7834 | own window mui_settings_open, mui_views.c:292 | 1 ~ | I1180 |
| Shelf (Show all) page | PG_SHELF | exempt, no handler, mui_views.c:289 | 1 M | I1172 |
| Collection page (album, artist, playlist) | PG_COLLECTION | exempt, mui_views.c:291 | 1 M | I1172 |

### Actions (54, one table feeds both menus)
| Item | Desktop | MUI | State | Row |
|---|---|---|---|---|
| Transport, seek, volume, pitch, tempo, solo, filter LED, queue, undo/redo, like, cursor, stats, ui switch, quit (44) | actions.c:10-99 | mui_menu_build from the same table, mui_ui.c:383 | 44 P | - |
| 4 layout actions (Full, Window, Mini, Visual) | PREF_SET ui.desktop.mode | MUI has its own modes (k_mode_keys, mui_controls.c:170: full, window, lowered, mini, micro, icon); no Visual layout, unverified whether the menu item acts | 4 ~ | I1173 |
| 6 style actions | VIEW_STYLE | menu entry hidden when the style is not offered (mui_menu.c:48), mui_style_mask(0) at mui_ui.c:383 | 6 ~ | I1174 |

### Settings (20)
| Item | Desktop | MUI | State | Row |
|---|---|---|---|---|
| 19 pref rows | features.c:88 | same rows, mui_ui.c:2744 | 19 P | - |
| ui.desktop.zoom (fit, 75..150; read at desktop_ui.c:16419, not in prefs.def) | 16419 | none | 1 M | I1180 |

### Library, queue, playlists, song information
| Item | Desktop | MUI | State | Row |
|---|---|---|---|---|
| Library modes Composers, Tree, Columns | lib_mode_name | mui_ui.c:1459, :1618 | 3 P | - |
| Library modes Files, History, Sources | lib_mode_name | none | 3 M | I1175 |
| Library style List | lib_style_name | main list | 1 P | - |
| Library styles Grid, Cover Flow | lib_style_name | no hit in mui_ui.c or map/ | 2 ~ | I1174 |
| Queue Undo, Redo, Reshuffle | QUEUE_BUTTONS | menu (actions) | 3 P | - |
| Queue scope selector Library, Album, Playlist | queue_scope_name | none | 1 M | I1176 |
| Playlist Play, Rename, Delete, New Playlist | desktop_ui.c:4957 | tool buttons mui_ui.c:1157, :2470-2476 | 4 P | I799 A8-A11 moves them |
| Playlist New Folder | pl_button_label | none | 1 M | I1177 |
| Song Information Mark Wrong, Skip Song, Save Note | desktop_ui.c:6746 | none | 3 M | I1178 |
| Song menu: 5 items | sel_uim_song_menu_label | mui_ui.c:324, :1680 | 5 P | - |
| Song menu swaps (Remove from Queue / Playlist), menu on every song list | desktop_ui.c:3005 plmenu_open | context menu on the main list only, mui_ui.c:1680 | 1 ~ | I1179 |
| Library Statistics panel | desktop_ui.c:447 stats_open | menu action; panel code only map/mui_p8.c:177, unverified | 1 ~ | I1181 |
| First-start sync offer | desktop_ui.c:613 nag_ | none | 1 M | I1182 |
| Sources page: per-source toggle, Sync status | desktop_ui.c:8934 | none (tile only, map/mui_tiles.c:108) | 1 M | I1175 |
| Tile size steps | desktop_ui.c:482 g_tile_steps | none | 1 M | I1174 |

### Overlays, dialogs, feedback
| Item | State |
|---|---|
| Command palette, Help overlay, Command line (3) | 3 P (map/mui_overlay.c, I798) |
| Confirm dialogs Remove from Library, Clear Library (2) | 2 P (MUI_Request, mui_ui.c:1386, :2930) |
| Tooltips (1), Toasts (1) | 2 P (map/mui_overlay.c, mui_ui.c:116) |

## Totals

180 items: present 117, partial 20, missing 43. (Pages 14, doors 7, actions 54, action keys 40, page keys 6, focus 1, settings 20, library modes 6, styles 3, queue buttons 4, playlist buttons 5, song info 3, song menu 6, overlays 3, dialogs 2, tooltips/toasts/stats/nag/sources/tiles 6.) 28 of the 43 missing share one root: the live window binds no multi-char key (I1171).

## Proposed order

1. Keyboard and navigation: I1165 (Tab, arrows; open), I1171 (28 multi-char keys, page keys), I1173 (layout keys).
2. Pages: I1172 (Shelf, Collection), I1175 (Files, History, Sources, Sources page), I1176 (queue scope), I1177, I1178, I1179, I1181, I1182, I1174 (styles, tiles).
3. Settings: I1180 (zoom; Settings page versus window; Johan 2026-10-31: both UIs show the same settings).

Rig looks still open on earlier rows (I794-I796, I798, I735, I733, I739, I740, I730, I731) and I1166 (sidebar chips) are not repeated here.
