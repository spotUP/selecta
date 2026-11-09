---
date: 2026-11-08
status: done
row: I1168
---
# Text UI against desktop UI: parity gap table

Host-only read of the code, nothing built or run. P = present, ~ = partial (reachable only through the palette or the `:` command line, or by a cycle key, or a reduced form), M = missing. Template: 2026-11-08-mui-parity-gaps.md; desktop inventory and counts reused from it and re-verified below.

## Where each count comes from

- Desktop actions: `k_actions[]` src/uimodel/actions.c:10, 54 rows, 40 with a default key, 14 without (parsed by a script over the table fields, not grep).
- Pages: `sel_uim_page_id` include/selecta/uimodel/pages.h:15-29 (enum, SEL_UIM_PG_COUNT 14); doors src/uimodel/doors.c:7-13, 7 rows.
- Settings: `g_pref_rows[]` src/uimodel/features.c:88, 19 rows (script count: 8 all machines, 7 host, 4 Amiga) plus ui.desktop.zoom (desktop_ui.c:16419) = 20.
- Page keys: `k_page_keys[]` desktop_ui.c:13258, 6.
- Text UI has no action table, page registry or prefs list. Its structures: the default key table `k_defaults[]` src/uimodel/keybind.c:133 (the only key source, loaded by tui_load_bindings text_ui.c:1165), the dispatcher `tui_key` text_ui.c:438 (`@name` rows go to `tui_local` text_ui.c:293, everything else is sent to the engine as a protocol line), the command line (`:`) and palette (text_ui.c:389-432), the view sources the engine offers (queue, browse, folders, home, query) and the panels tui_viz_panel :817, tui_pattern_panel :895, tui_info :682, tui_stats_pane :954. Each text state below was read from these, not grepped.
- The text UI never reads `sel_uim_action_*`, `g_pages`, `g_pref_rows` or any PREF_* (no `pref` hit in text_ui.c): it is a hand-written parallel feature set, which is the root of the lag.

## Table

### Actions (54)
| Item | Desktop | Text UI | State | Group |
|---|---|---|---|---|
| Play, Pause, Stop, Next, Previous, 2 subsong, 2 seek, 2 volume, 2 pitch, 2 tempo, Solo clear, Filter LED, Undo, Redo, cursor up/down/top/bottom, Activate, Stats show, Stats hide, Quit, ui_text (28; ui_text = already on) | actions.c:10-99 | k_defaults keybind.c:133-205, tui_local text_ui.c:293 | 28 P | - |
| Shuffle on/off, Repeat off/all/one, Next/Previous Album, Queue Clear/Shuffle/Reshuffle, Like (11) | actions.c | no key and no menu; only typed on `:` (e.g. `OPTION random on`); palette sends bare verbs (text_ui.c:420) | 11 ~ | I1184 |
| Styles Regular, Tree, Drilldown | VIEW_STYLE | `y` cycles three styles, tui_next_style text_ui.c:165 | 3 ~ | I1186 |
| Visual modes Scope, Levels, Spectrum | viz_* actions | `v` cycles list/Levels/Scope/Spectrum/Note dots/Phase (k_viz_title text_ui.c:776), no direct key | 3 ~ | I1185 |
| Visual mode SID registers | viz_sidregs | none of the 5 panels | 1 M | I1185 |
| Styles Grid, Cover Flow, Compact | actions.c | tui_next_style stops at Drilldown | 3 M | I1186 |
| Layouts Full, Window, Mini, Visual | PREF_SET ui.desktop.mode | no layouts; panels replace the list pane (tui_bind_ctx :434) | 4 M | I1185 |
| Switch to Desktop Interface (ui_desktop) | actions.c:90 | no binding, no UI_SWITCH in text_ui.c | 1 M | I1184 |

### Keyboard (40 keyed actions, 6 page keys, 3 overlays)
| Item | Desktop | Text UI | State | Group |
|---|---|---|---|---|
| 22 keyed actions bound the same (Space, s, n, p, M-Left/Right, + -, . ,, C-Left/Right, 0, M-l, u, C-r, k j Home End Enter, b) | desk_bind_actions desktop_ui.c:13261 | keybind.c:133-205 | 22 P | - |
| Stats hide C-b | actions.c | only Escape/Backspace while the stats pane is up, text_ui.c:455 | 1 ~ | I1184 |
| Keys C-Enter, M-n, M-p, M-Backspace, M-s, M-r, M-h, C-1..C-4, C-5..C-8, C-t, C-d | actions.c | not in k_defaults | 17 M | I1184, I1185 |
| Page keys @home @library @folders @info @viz @pattern | desktop_ui.c:13258 | H, l, f, i, v, t (keybind.c:164-167, 181-182) | 6 P | - |
| Overlays Command palette, Help, Command line | desktop | C-p, ?, : (text_ui.c:343-349, 735) | 3 P | - |

### Pages (14) and doors (7)
| Item | Desktop | Text UI | State | Group |
|---|---|---|---|---|
| Home, Search, Folders (browse), Playlists, Queue, Viz, Tracker, Browse | pages.c | VIEW_SOURCE home (`H`), `/` prompt, Tab browse, `f` folders source, Tab queue, `v`, `t` | 8 P | - |
| Now Playing | PG_NOWPLAYING | header line + panels, no page (tui_header :601) | 1 ~ | I1191 |
| Songs | PG_SONGS | Library screen (query view, tui_library_open :284) | 1 ~ | I1187 |
| Song Info | PG_SONGINFO | `i` tag dump, tui_info :682, no actions | 1 ~ | I1191 |
| Shelf (Show all), Collection | PG_SHELF, PG_COLLECTION | none | 2 M | I1189 |
| Settings | desktop_ui.c:7834 | none | 1 M | I1188 |
| Doors: Home, Search, Browse Folders, Playlists, Queue | doors.c:7-11 | keys H, /, Tab, f, Tab | 5 P | - |
| Doors: Now Playing, Song Information | doors.c:12-13 | no page behind them | 2 ~ | I1191 |

### Settings (20)
| Item | Desktop | Text UI | State | Group |
|---|---|---|---|---|
| 19 pref rows (cycle, check, slider, string, ROM) | features.c:88 | no settings screen; only `:PREF_SET key value` typed (commands.def:129), no list, no current value | 19 M | I1188 |
| ui.desktop.zoom | desktop_ui.c:16419 | none (terminal cell size; exemption candidate) | 1 M | I1188 |

### Library, queue, playlists, song information, feedback
| Item | Desktop | Text UI | State | Group |
|---|---|---|---|---|
| Library mode Tree | lib_mode_name | Library screen opens tree style grouped by format, text_ui.c:268 | 1 P | - |
| Library modes Files, History | lib_mode_name | Browse view (Tab), Home source | 2 ~ | I1187 |
| Library modes Composers, Columns, Sources | lib_mode_name | none (`g` groups folder/format only, :174) | 3 M | I1187 |
| Library style List | lib_style_name | regular style | 1 P | - |
| Library styles Grid, Cover Flow | lib_style_name | none | 2 M | I1186 |
| Queue Undo, Redo | QUEUE_BUTTONS | u, C-r | 2 P | - |
| Queue Reshuffle | QUEUE_BUTTONS | `:` only | 1 ~ | I1184 |
| Queue scope Library, Album, Playlist | queue_scope_name | none | 1 M | I1187 |
| Playlist Play | desktop_ui.c:4957 | Enter on the folders view (hint text_ui.c:1033) | 1 P | - |
| Playlist Rename, Delete, New Playlist, New Folder | pl_button_label desktop_ui.c:4957 | none | 4 M | I1190 |
| Song Information Mark Wrong, Skip Song, Save Note | desktop_ui.c:6746 | none | 3 M | I1191 |
| Song menu 5 items plus Remove swaps | sel_uim_song_menu_label, plmenu_open :3005 | only `a` add and `d` remove at the cursor (tui_add_cursor_file :138, tui_remove_cursor_entry :151), `m` mark; no menu | 6 ~ | I1190 |
| Statistics panel | desktop_ui.c:447 | `b`, tui_stats_pane :954 | 1 P | - |
| First-start sync offer | desktop_ui.c:613 | none | 1 M | I1192 |
| Sources page | desktop_ui.c:8934 | none | 1 M | I1192 |
| Tile size steps | desktop_ui.c:482 | none (grid-only concept; exemption candidate) | 1 M | I1192 |
| Tooltips | desktop | none (key hint line only, tui_draw :1033-1051) | 1 M | I1192 |
| Toasts | desktop | status line `cmd.status`, text_ui.c:1025 | 1 ~ | I1192 |
| Confirm dialogs Remove from Library, Clear Library | desktop | none | 2 M | I1192 |
| Focus and navigation (arrows, j k, Tab) | desktop focus | Up/Down/j/k, Tab view switch | 1 P | - |

## Totals

180 items (same item list as the MUI table): present 79, partial 33, missing 68. Present 79 = 28 actions + 22 keys + 6 page keys + 8 pages + 5 doors + 3 overlays + 1 focus + 6 sub-features (Tree, List, Undo, Redo, Playlist Play, Statistics). The two largest missing roots: no Settings screen (20) and k_defaults lacking 17 keys.

## Proposed order (matches the new ledger rows)

1. Player basics: I1184 (keys and toggles for the 11 command-only actions, Stats hide, UI switch).
2. Visual and layout: I1185, I1186; library: I1187.
3. Pages: I1188 Settings, I1189 Shelf and Collection, I1190 playlist and song menu, I1191 Now Playing and Song Information, I1192 dialogs, toasts, nag, Sources.
