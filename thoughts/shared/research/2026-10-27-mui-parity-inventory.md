---
date: 2026-10-27
row: MUI-P0-7
---
# Desktop surface inventory against the MUI plan

Built from the tables that produce the desktop UI. Source of truth for the rows: tests/dui/parity_matrix.tsv (rows keyed desk:*); gate: ctest `mui-parity`.

## Counts per source table (counted from the table, not by grep of labels)

- src/uimodel/actions.c `k_actions`: 52 entries (counted between the table braces; all 52 asserted by the generator against the action-id set)
- src/uimodel/actions.c `k_exempt`: 76 engine verbs with no menu item (not a UI surface, no rows; reasons ARG/PROTO/CTX/COV are in the table)
- plugins/ui/desktop/desktop_ui.c `g_doors` (DOOR_COUNT 7): 7 sidebar doors
- desktop_ui.c `desk_page` / `g_page_name` (PG_COUNT 10): 10 pages, 7 reached by doors, 3 more (songs, viz, tracker) listed as rows
- src/uimodel/features.c `sel_uim_song_menu_label` (SEL_UIM_SONG_MENU_ITEMS 5): 5 song context menu items
- desktop_ui.c `home_chip_name` 4, `search_facet_name` 4, `lib_mode_name` 6, `lib_style_name` 3, `queue_scope_name` 3, `SLOT_` enum 7 transport slots
- desktop_ui.c button sets: QUEUE_BUTTONS 4, PL_BUTTONS 5, SI_BUTTONS 3; dialogs DLG_ 2; overlays OV_ 3; `k_page_keys` 6
- include/selecta/prefs.def `ui.desktop.*`: 3 declared (mode, viz, reduce_motion); `ui.desktop.zoom` is read at desktop_ui.c:8559 but not declared in prefs.def
- Inventory rows added to the matrix: 127 (29 NO ROW). Matrix total 262 rows, NO ROW 67.

## Entries (source table, entry, MUI row ID)

actions.c k_actions | Play | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Pause | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Stop | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Next Track | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Previous Track | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Turn Shuffle On | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Turn Shuffle Off | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Turn Repeat Off | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Repeat Queue | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Repeat Song | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Previous Subsong | MUI-P2-3, MUI-P2-4, MUI-P4-9
actions.c k_actions | Next Subsong | MUI-P2-3, MUI-P2-4, MUI-P4-9
actions.c k_actions | Next Album | NO ROW
actions.c k_actions | Previous Album | NO ROW
actions.c k_actions | Seek Forward | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Seek Backward | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Volume Up | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Volume Down | MUI-P2-3, MUI-P2-4, MUI-P2-5
actions.c k_actions | Pitch Up | NO ROW
actions.c k_actions | Pitch Down | NO ROW
actions.c k_actions | Tempo Up | NO ROW
actions.c k_actions | Tempo Down | NO ROW
actions.c k_actions | Unsolo All Voices | NO ROW
actions.c k_actions | Toggle Filter Light | NO ROW
actions.c k_actions | Clear Queue | MUI-P2-3, MUI-P2-4, MUI-P4-4
actions.c k_actions | Shuffle Queue | MUI-P2-3, MUI-P2-4, MUI-P4-4
actions.c k_actions | Reshuffle Queue | MUI-P2-3, MUI-P2-4, MUI-P4-4
actions.c k_actions | Undo | MUI-P2-3, MUI-P2-4, MUI-P4-4
actions.c k_actions | Redo | MUI-P2-3, MUI-P2-4, MUI-P4-4
actions.c k_actions | Like Song | MUI-P2-3, MUI-P2-4, MUI-P4-7
actions.c k_actions | Cursor Up | MUI-P2-3, MUI-P2-4, MUI-P2-7
actions.c k_actions | Cursor Down | MUI-P2-3, MUI-P2-4, MUI-P2-7
actions.c k_actions | Cursor to Top | MUI-P2-3, MUI-P2-4, MUI-P2-7
actions.c k_actions | Cursor to Bottom | MUI-P2-3, MUI-P2-4, MUI-P2-7
actions.c k_actions | Open Selection | MUI-P2-3, MUI-P2-4, MUI-P2-7
actions.c k_actions | Regular Style | MUI-P2-3, MUI-P2-4, MUI-P3-8
actions.c k_actions | Tree Style | MUI-P2-3, MUI-P2-4, MUI-P3-8
actions.c k_actions | Drilldown Style | MUI-P2-3, MUI-P2-4, MUI-P3-8
actions.c k_actions | Grid Style | MUI-P2-3, MUI-P2-4, MUI-P3-8
actions.c k_actions | Cover Flow Style | MUI-P2-3, MUI-P2-4, MUI-P3-8
actions.c k_actions | Compact Style | MUI-P2-3, MUI-P2-4, MUI-P3-8
actions.c k_actions | Show Statistics | NO ROW
actions.c k_actions | Hide Statistics | NO ROW
actions.c k_actions | Full Layout | MUI-P2-3, MUI-P2-4, MUI-P6-1
actions.c k_actions | Window Layout | MUI-P2-3, MUI-P2-4, MUI-P6-1
actions.c k_actions | Mini Layout | MUI-P2-3, MUI-P2-4, MUI-P6-1
actions.c k_actions | Visual Layout | NO ROW
actions.c k_actions | Oscilloscope Visualizer | MUI-P2-3, MUI-P2-4, MUI-P5-3
actions.c k_actions | Levels Visualizer | MUI-P2-3, MUI-P2-4, MUI-P5-3
actions.c k_actions | Spectrum Visualizer | MUI-P2-3, MUI-P2-4, MUI-P5-3
actions.c k_actions | Switch to Text Interface | MUI-P2-3, MUI-P2-4, MUI-P7-4
actions.c k_actions | Switch to Desktop Interface | MUI-P2-3, MUI-P2-4, MUI-P7-4
desktop_ui.c g_doors | Home | MUI-P4-1
desktop_ui.c g_doors | Search | MUI-P4-3
desktop_ui.c g_doors | Your Library | MUI-P4-2
desktop_ui.c g_doors | Playlists | MUI-P4-5
desktop_ui.c g_doors | Queue | MUI-P4-4
desktop_ui.c g_doors | Now Playing | MUI-P4-8
desktop_ui.c g_doors | Song Information | MUI-P4-9
desktop_ui.c g_page_name (pages that are not doors) | songs | MUI-P3-1
desktop_ui.c g_page_name (pages that are not doors) | viz | MUI-P5-3
desktop_ui.c g_page_name (pages that are not doors) | tracker | MUI-P4-16
features.c sel_uim_song_menu_label | Play | MUI-P4-6
features.c sel_uim_song_menu_label | Play Next | MUI-P4-6
features.c sel_uim_song_menu_label | Add to Queue | MUI-P4-6
features.c sel_uim_song_menu_label | Add to Playlist | MUI-P4-6
features.c sel_uim_song_menu_label | Remove from Library or Queue | MUI-P4-6
desktop_ui.c home_chip_name | Home chip All | MUI-P4-1
desktop_ui.c home_chip_name | Home chip Songs | MUI-P4-1
desktop_ui.c home_chip_name | Home chip Composers | MUI-P4-1
desktop_ui.c home_chip_name | Home chip Playlists | MUI-P4-1
desktop_ui.c search_facet_name | Search facet All | MUI-P4-3
desktop_ui.c search_facet_name | Search facet Songs | MUI-P4-3
desktop_ui.c search_facet_name | Search facet Albums | MUI-P4-3
desktop_ui.c search_facet_name | Search facet Artists | MUI-P4-3
desktop_ui.c lib_mode_name | Library mode Composers | MUI-P3-2
desktop_ui.c lib_mode_name | Library mode Tree | MUI-P3-2
desktop_ui.c lib_mode_name | Library mode Columns | MUI-P3-4
desktop_ui.c lib_mode_name | Library mode Files | NO ROW
desktop_ui.c lib_mode_name | Library mode History | NO ROW
desktop_ui.c lib_mode_name | Library mode Sources | NO ROW
desktop_ui.c lib_style_name | Library style tab Grid | MUI-P3-8, MUI-P5-2
desktop_ui.c lib_style_name | Library style tab Cover Flow | MUI-P3-8, MUI-P5-2
desktop_ui.c lib_style_name | Library style tab List | MUI-P3-8, MUI-P3-1
desktop_ui.c queue_scope_name | Queue scope Library | NO ROW
desktop_ui.c queue_scope_name | Queue scope Album | NO ROW
desktop_ui.c queue_scope_name | Queue scope Playlist | NO ROW
desktop_ui.c queue buttons (QUEUE_BUTTONS) | Queue button Undo | MUI-P4-4
desktop_ui.c queue buttons (QUEUE_BUTTONS) | Queue button Redo | MUI-P4-4
desktop_ui.c queue buttons (QUEUE_BUTTONS) | Queue button Reshuffle | MUI-P4-4
desktop_ui.c queue buttons (QUEUE_BUTTONS) | Queue button Scope selector | NO ROW
desktop_ui.c pl_button_label (PL_BUTTONS) | Playlist button New Folder | NO ROW
desktop_ui.c pl_button_label (PL_BUTTONS) | Playlist button New Playlist | NO ROW
desktop_ui.c pl_button_label (PL_BUTTONS) | Playlist button Rename | MUI-P4-5
desktop_ui.c pl_button_label (PL_BUTTONS) | Playlist button Delete | MUI-P4-5
desktop_ui.c pl_button_label (PL_BUTTONS) | Playlist button Play | MUI-P4-5
desktop_ui.c si_button_label (SI_BUTTONS) | Song information button Mark Wrong | NO ROW
desktop_ui.c si_button_label (SI_BUTTONS) | Song information button Skip Song | NO ROW
desktop_ui.c si_button_label (SI_BUTTONS) | Song information button Save Note | NO ROW
desktop_ui.c DLG_ enum | Confirm dialog Remove from Library | MUI-P4-12
desktop_ui.c DLG_ enum | Confirm dialog Clear Library | MUI-P4-12
desktop_ui.c OV_ enum | Overlay Command palette | MUI-P4-13
desktop_ui.c OV_ enum | Overlay Help overlay | MUI-P4-14
desktop_ui.c OV_ enum | Overlay Command line | MUI-P4-13
desktop_ui.c TOAST_MS and toasts | Toast | MUI-P4-11
features.c sel_uim_state_text | Empty, loading and error text per view (7 views, 3 kinds) | MUI-P4-15
desktop_ui.c SLOT_ enum (TRANSPORT_COUNT) | Transport slot Previous Subsong | MUI-P4-9
desktop_ui.c SLOT_ enum (TRANSPORT_COUNT) | Transport slot Shuffle | MUI-P2-5
desktop_ui.c SLOT_ enum (TRANSPORT_COUNT) | Transport slot Previous | MUI-P2-5
desktop_ui.c SLOT_ enum (TRANSPORT_COUNT) | Transport slot Play | MUI-P2-5
desktop_ui.c SLOT_ enum (TRANSPORT_COUNT) | Transport slot Next | MUI-P2-5
desktop_ui.c SLOT_ enum (TRANSPORT_COUNT) | Transport slot Repeat | MUI-P2-5
desktop_ui.c SLOT_ enum (TRANSPORT_COUNT) | Transport slot Next Subsong | MUI-P4-9
desktop_ui.c CTL_ enum (CTL_LIKE) | Like heart control | MUI-P4-7
desktop_ui.c CTL_ enum (CTL_VIZ_N) | Visualizer picker (6 controls) | MUI-P5-3
desktop_ui.c sidebar | Tooltips on transport and sidebar controls | NO ROW
desktop_ui.c stats_open | Library Statistics panel (Statistics, More, Less) | NO ROW
desktop_ui.c nag_ (I527) | First-start sync offer (Not now, Sync, per-source check) | NO ROW
desktop_ui.c src_ (SRC_TOGGLE) | Sources page: per-source toggle and Sync, Syncing and Synced status | NO ROW
desktop_ui.c line 4698 | Add Folder button | MUI-P4-12
desktop_ui.c line 4699 | Clear Library button | MUI-P4-12
desktop_ui.c line 8559 | Pref ui.desktop.zoom (fit, 75, 100, 125, 150; not declared in prefs.def) | NO ROW
prefs.def ui.desktop.mode | Display mode pref | MUI-P6-1
prefs.def ui.desktop.viz | Visualizer pref | MUI-P5-3
prefs.def ui.desktop.reduce_motion | Reduce motion pref | not carried: Tweens, fades, hover animation, MUI draws its own frames (plan 2.3)
desktop_ui.c g_tile_steps | Tile size steps for grid and home shelves | NO ROW
desktop_ui.c k_page_keys | Page keys @home @library @folders @info @viz @pattern | MUI-P2-7

## Notes
- Every action carries MUI-P2-3/MUI-P2-4 (the native Menustrip is built from the action table), so action rows are red only where no MUI plan row names the feature at all.
- hippo/bbmp NO ROW rows split into desktop engine/source/players exemptions that are not UI (MUI plan has no row and no 2.3 reason) and real UI gaps (stored programs, info-line modes, remembered state, activity panel, lookup, tooltips).
