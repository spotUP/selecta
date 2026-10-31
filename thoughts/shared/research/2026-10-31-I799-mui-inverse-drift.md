# I799: MUI inverse drift review (MUI shows controls Mac does not)

Date 2026-10-31. Design source: 2026-10-30-mui-mac-drift.md row 10. Review only, no code changed.
Rule: Mac layout is the truth; MUI matches the rich desktop UI as close as MUI 3.8 allows.
Mac = plugins/ui/desktop/desktop_ui.c (D) and src/uimodel (U). MUI = plugins/ui/mui/mui_ui.c (M), map/mui_layout.c (L).

## A. The 12 tool buttons (M:277 g_tool_which, M:1070 mui_tool_group, labels L:12)

| # | Area | MUI today | Mac today | Proposed MUI change | Size |
|---|------|-----------|-----------|---------------------|------|
| A1 | Like | Already moved beside the title in the player bar (M:923, I796 B4); skipped in tool rows (M:1097) | Now Playing add/like control, tooltip via sel_uim_tip_like (D:11044) | None. Drop the dead entry from the tool row table (MUI_TOOL_BTNS count and labels) | S |
| A2 | Remove From Queue | Tool button r2 (M:1101), acts on cursor row | Per-row Remove button on Queue page (D:4234) and song menu item "Remove from Queue" when on queue (U features.c:25, D:3233) | Remove the tool button; keep the song context menu item (already present, M:310). Add a per-row remove only if AAList allows | S |
| A3 | Clear Queue | Tool button (M:2446) | No button; QUEUE_CLEAR exists only as an action (U actions.c:45) reachable from palette/keys. Queue page header has Undo, Redo, Reshuffle, Scope only (D:4107) | Remove the tool button; Clear Queue stays in the window menu strip and command palette. Open question for Mac: no header button either | S |
| A4 | Move Up | Tool button (M:2453) | Drag reorder only (QUEUE_MOVE, D:4209) | Remove button; replace by drag in the queue list if AAList supports it, else keep keyboard action in menu strip only | M |
| A5 | Move Down | Tool button (M:2453) | Same as A4 | Same as A4 | S |
| A6 | Help | Tool button, opens List window "Selecta Help" (M:2156) | No button. Keyboard shortcut overlay OV_HELP via action @help (D:11362, D:12083); same model sel_uim_help_row | Remove the button; keep window-menu entry and key. Keep the window (MUI has no overlay layer) | S |
| A7 | Settings | Tool button, opens Settings window (M:2328) | No Settings page or button anywhere in desktop_ui.c (grep Settings: no hits) | Remove the tool button; reach the window from the menu strip only. See section B | S |
| A8 | New Playlist | Accent tool button (M:1101) | Playlists page header buttons New Folder, New Playlist, Play (D:4325, D:5592) | Move to the Playlists page header (MUI browse/playlists page), not the global tool rows; keep accent role | M |
| A9 | Open Playlist | Tool button on cursor row (M:2470) | No button: click a playlist row opens its page (D:5836 flow) | Remove; open on row activate (double click / Return) | S |
| A10 | Rename Playlist | Tool button (M:2471) | Playlist page "More" button menu (D:4679) and row menu (D:2646, D:3183); inline name field (D:4633) | Move to a playlist context menu on the Playlists list and a More item; remove the tool button | M |
| A11 | Delete Playlist | Tool button (M:2476) | Same More menu as A10, with confirm dialog (D:3034, D:3183) | As A10 | S |
| A12 | Clear Library | Tool button, MUI_Request confirm (M:2498) | Library page header button left of Add Folder (D:3767, D:7674), confirm dialog DLG_CLEAR | Move to the Library page header beside Add Folder; keep MUI_Request | M |

Net: the third tool row (r2/r3, M:1085..1105) can go away once A2-A12 are moved; only search, command line, rating and subsong Cycles remain (rating and subsong are not in the 12; not reviewed here).

## B. Settings dialog

| # | Area | MUI today | Mac today | Proposed MUI change | Size |
|---|------|-----------|-----------|---------------------|------|
| B1 | Settings existence | 9 pref rows from sel_uim_pref_row_at (U features.c:61..70) in a 2-column grid window (M:2328) | No settings page consuming these rows. Mac only writes sort/view prefs from its own controls (D:10082) and other prefs per feature (D:7926, D:8276) | Decision for Johan: either build a Mac Settings page from the same U rows (Mac is truth, so MUI stays) or accept MUI-only. Not an MUI change until Mac exists | L (Mac side) |
| B2 | Rows with no Mac control: Loudness, Crossfeed, Power LED Filter, Stereo Width, Prefetch, Metered, Fill Screen, Title Format, Status Format | All in MUI window | Not reachable on Mac UI by a control | Same as B1 | M |

## C. Context menus

| # | Area | MUI today | Mac today | Proposed MUI change | Size |
|---|------|-----------|-----------|---------------------|------|
| C1 | Song menu items | 5 fixed items Play, Play Next, Add to Queue, Add to Playlist, Remove from Library (M:310, U features.c:25) with keys 1-5 | Same 5 plus Remove from Queue swap and Remove from Playlist when pl_id set (D:2583, D:2588) | Pass on_queue to labels (M:314 calls with 0); add Remove from Playlist item on playlist rows | M |
| C2 | Playlist row menu | None (buttons instead) | Rename, Delete (folder: Delete) via plmenu_open (D:2647, sel_uim_plmenu_action) | Add Menustrip context menu on the Playlists list with the same items | M |
| C3 | Playlist More menu | None | More button beside Play (D:4679) | Add a More button on the playlist page, or reuse C2 | S |
| C4 | Context menu scope | Only Listview of main list (M:1348) | Menu on song rows in Library, Search, Queue, Playlist, Liked pages (song_row_reg) | Attach the same context strip to every song list object (queue, search, playlist) | M |

## D. Help and shortcuts

| # | Area | MUI today | Mac today | Proposed MUI change | Size |
|---|------|-----------|-----------|---------------------|------|
| D1 | Help content | List window, one row per action (mui_help_row, map/mui_views.c:105) | Overlay titled "Keyboard shortcuts" from the same action table (D:11447, D:13359) | Same data; keep the window. Only the title differs: rename "Selecta Help" to "Keyboard Shortcuts" (M:2170) | S |
| D2 | Help entry point | Tool button (see A6) | Key or palette action @help | Menu strip item and key only | S |
| D3 | Command palette | MUI command line String in tool row 1 (M:1086); palette done by I798 | Overlay OV_PALETTE (D:11362) | None (I798 merged) | - |

## Counts

A tool buttons: 12 rows (A1 already moved; 11 to move or remove). B settings: 2. C context menus: 4. D help: 3 (1 no change, 2 small). Total differences needing action: 11 + 2 + 4 + 2 = 19.

## Open decisions for Johan

1. Mac has no Settings page: build one on the shared pref rows, or keep Settings MUI-only? (B1)
2. Mac has no Clear Queue or Move Up/Down button: keep them as keyboard/menu actions in MUI only? (A3, A4)

## Decisions (Johan, 2026-10-31)

- Settings: the Mac desktop gets a Settings page built on the shared pref rows, so both UIs show the same settings.
- Queue: menus on both UIs. Mac adds Clear Queue to the queue header and Move Up/Down to the queue song menu; MUI moves
  the same actions from its tool buttons into the same places.
