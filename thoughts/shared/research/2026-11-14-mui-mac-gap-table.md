---
date: 2026-11-14
topic: MUI against Mac desktop UI, gap table refresh and hook parity guard
status: done
row: I1329
---
# MUI against the Mac desktop UI: gap table (refresh of I1170)

Host-only read of the code at main 2fb997f4, nothing built for the Amiga, no rig. "unverified" = read from code only.
Method: the I1170 table (2026-11-08-mui-parity-gaps.md; the I734 drift doc 2026-10-30-mui-mac-drift.md is not in the
tree, so I1170 is the method source). Two passes:

1. Re-check every I1170 row against the current ledger state and code.
2. New, derived from structure: every `sel_engine_*`, `sel_uim_*`, `sel_route_*` hook declared in include/selecta/**/*.h
   and called by plugins/ui/desktop/** but never by plugins/ui/mui/** (comments stripped). This is what the new ctest
   `mui-mac-hook-parity` computes (tests/ui/mui_mac_hook_parity.cmake). Result: 168 desktop hooks, 82 called by MUI,
   86 not: 55 gaps/differs (53 in section 2, 2 under I1173) and 31 not gaps (desktop pixel layout, dumps, or MUI reaches the same data another
   way). Each of the 86 is a line in tests/ui/mui_mac_hook_parity.exempt with its reason and row.

Desktop commits since 2026-11-08 were also scanned (`git log --since=2026-11-08 -- desktop_ui.c`); the Mac features
they add are covered by the hook rows below (I1206, I1221, I1222f, I1224, I1229, I1231, I1248, I1274, I1277) except
I1235 (Modland format list A-Z + find field), I1286 (source drill levels are history pages), I1290 (Enter on a playlist
then keys act at once) and I1072d (toast names the blocking source): those go through actions or view rows, not a
new hook, so the guard cannot see them; listed as proposed row I1329l (verify, unverified whether MUI already does it).

## 1. I1170 rows, refreshed

| Item | Desktop | MUI | State now | Row |
|---|---|---|---|---|
| 28 multi-char action keys, page keys | desk_bind_actions | 4183adc8 window keys | code done, rig pending | I1171 [~] |
| Tab / arrows by focus | desktop focus ring | 4183adc8 | code done, rig pending | I1165 [~] |
| Shelf, Collection pages | PG_SHELF, PG_COLLECTION | mui_views.c:297 (Show all on Home source), de6578b6 | code done, rig pending | I1172 [~] |
| 4 layout actions Full/Window/Mini/Visual | PREF_SET ui.desktop.mode, desktop_ui.c:17256 sel_uim_set_layout | own modes, no Visual | ~ differs | I1173 [~], I1200 (mode keys dead) |
| Visual layout close button + Escape (I1248, new) | desktop_ui.c:13351 tip_viz_close | none | M | I1173 |
| Styles Grid, Cover Flow, Compact, tile steps | lib_style_name, g_tile_steps | mui_ui.c:204, :2054, :2889 (packed list rows) | ~ | I1174 [~] |
| Library modes Files, History, Sources + Sources page | lib_mode_name | mui_ui.c:619 CTL_LIBMODE (all 6) | code done | I1175 [~], I1197 [x] |
| Queue scope selector | queue_scope_name | 4c609ac9 | code done, rig pending | I1176 [~] |
| Playlist New Folder | pl_button_label | 4669104c | P | I1177 [x] |
| Song Information Mark Wrong, Skip, Save Note | si_button_label | mui_ui.c:133 siwrong/siskip/sinote | P | I1178 [x] |
| Song menu swaps, menu on every list | plmenu_open | mui_ui.c:401-424 | P, rig pending | I1179 [x] |
| Settings page / zoom | desktop_ui.c draw_settings | 006760bb | P, rig pending | I1180 [x] |
| Library Statistics | lib_stats_rows desktop_ui.c:4275 | mui_ui.c:112, :3150 | P, rig pending | I1181 [x] |
| First-start sync offer | nag on sel_uim_offer | mui_ui.c:113 CTL_OFFER_* | P; window never opens on Amiga | I1182 [x], I1308e, I1308c, I1357, I1364 |
| Sidebar Expand/Collapse | side expand | f1cf602d | code done, rig pending | I1166b [~] |

Open live MUI bugs that are not gaps but block 1:1 (cited, not duplicated): I1333 (sidebar Now Playing / Song
Information dead), I1335 / I1362 (Your Library + and > dead), I1336 / I1344 / I1361 (Search dead, black screen,
flicker), I1337d (800x600 Create offscreen), I1351 (stock scroll bar), I1363 (empty Your Library at 640x256),
I1313d (Players Turned Off), I1306b (collection rows show file names, rig pending).

## 2. Hook gaps (derived; one row per feature)

| Feature | Desktop (file:line) | MUI | State | Row |
|---|---|---|---|---|
| Sync card, progress, Cancel, scan progress/toast, sources pending | desktop_ui.c:14001 desk_sync_follow, :15074 sync_cancel, :15629/:15686 scan_progress, :17357 scan_info, :13520 sources_pending | none | M | I1328 (existing) |
| Multi-select in song lists (Cmd toggle, Shift extend, Select All, bulk song menu "N songs from X") | desktop_ui.c:14958-15032 select_sync/pointer/key, :3005-3015 song_menu_selection, :17073 init | none (no MultiSelect in plugins/ui/mui) | M | I1329a (MUI twin of I632) |
| Settings ROM slots: install, reset, source text, user/bundled | desktop_ui.c:10552-10675 draw_settings/settings_click, :10620 settings_rom_install | none | M | I1329b |
| Your Library sort (sort key + label), chip arrow step, chips wrap onto rows | desktop_ui.c:12224 side_lib_apply, :12415 tl_draw, :3227, :11889 side_chip_click, :11758 side_chip_place | map/mui_sidebar.c:31-33 one chip row (lfilter_chips_single), no sort | M | I1329c |
| Search: result groups, zero-hit sections hidden, source scope from opening page, pending spinner, focus remembers page | desktop_ui.c:10190 search_rows_h, :10325-10368 draw_search, :5416 col_source_scope, :10298 reads_pending, :11148 desk_search_focus | search lines + facets mui_ui.c:1496, :2012; no groups/sections | ~ | I1329d (after I1336, I1344, I1361) |
| Now Playing panel lines from shared model (STIL multi-line, note action) | desktop_ui.c:12807-12810 draw_nowplaying, :17438 np_note_action | own lines, mui_ui.c:2440 np_note only | differs | I1329e |
| Queue page group dividers (album/source runs) and their titles | desktop_ui.c:4626-4649 queue_groups_sync | none | M | I1329f |
| Tracker page sample list; tracker position for the viz host | desktop_ui.c:7588 tr_sample_count, :7662 draw_trackerpage, :7119 viz_host_position | pattern rows only (mui_ui.c:136 MUI_TRK_ROWS) | M, unverified | I1329g |
| "Playing from <list>" context line | desktop_ui.c:15567 desk_play_context | none | M | I1329h |
| Back/Forward restores list scroll | desktop_ui.c:11157-11159 desk_rt_leave | route history mui_ui.c:182-187, no scroll keep | M | I1329i |
| Shared command lines and texts: rating line, playlist Delete text/line, Remove from Playlist label/line, Rename line, system-file dialog, song titles in dialogs/menu/composer info | desktop_ui.c:11481 rate, :3447/:3496 pldel, :2897/:3681 plremove, :6875 plrename, :3456 sysfile, :3292/:3722/:6915 song_text | own strings (P4_RATING, P4_PRENAME, P4_PDELETE mui_ui.c:128); no sysfile dialog | differs / M | I1329j |
| Home shelves split and tile route from the shared model | desktop_ui.c:2346 home_shelves ("the one split every UI shares", I1189), :2710 shelf_activate_row | own split (I795 mui_shelf_ctx mui_ui.c:140) | differs | I1329k |

Not gaps (31 hooks, reasons in the exempt file): desktop pixel layout and hit-tests (sel_uim_layout_*, hit,
footer_height, cols_split, budget_allows, motion_allowed, default_format, breakpoint_name, layout_set_side_width),
dumps/self-checks (actions_check, exemption_count, cols_calls, page_at, route_kind_name, lib_deferred, stats_get),
MUI uses a sibling (dlg_title/confirm/cancel via sel_uim_dlg_text + dlg_mui_gadgets after I1368; pref_choice via
pref_choice_index; song_menu_n via song_menu_label; stats_rows via stats_row), and transport (library_items_*,
lib_nowait/lib_site, song_briefs_busy/post: desktop reads the engine directly, MUI reads sel_engine_state view rows and
sel_uim_briefs_post / sel_engine_song_briefs_take). The transport and stats_get reasons are unverified: I did not trace
that MUI shows the same rows those reads fill (sidebar library list, Playlists page).

## Totals

Section 1: 15 I1170 items: 6 present ([x]), 7 code done with rig pending ([~]), 2 still differ (I1173 Visual layout, I1174 styles).
Section 2: 12 features over 53 hooks: missing 8 (I1328, I1329a, b, c, f, g, h, i), differs 4 (I1329d, e, j, k).
Plus I1329l (4 Mac changes the guard cannot see, verify).

## Guard

ctest `mui-mac-hook-parity` (tests/CMakeLists.txt next to mui-ctl-ranges; pure cmake -P, no build). It fails when
the desktop calls a declared hook MUI never calls and the exempt file does not name it, and on a stale exemption (MUI
now calls it, or the desktop no longer does), so landing a gap row means deleting its lines. Revert proof 2026-11-14: a
fake `sel_engine_fake_parity` declared in engine.h and called only in desktop_ui.c fails it ("sel_engine_fake_parity:
the desktop UI calls it, MUI does not and no exemption names it"), passes again after removal.
Limits: it sees hooks, not actions or view-row features (I1329l class), and a call inside a string literal counts.

## Proposed new ledger rows (not added to todos.md)

- I1329a (Amiga track) MUI multi-select in every song list, MUI twin of I632: Shift extend, toggle, Select All, bulk song menu on the selection over the shared sel_uim_selset_* (src/uimodel/select.c); desktop select_pointer/select_key desktop_ui.c:14958-15032. Done when the 13 selset/list_name lines leave tests/ui/mui_mac_hook_parity.exempt and a rig 2 check selects 3 songs and adds them to a playlist.
- I1329b (Amiga track) MUI Settings ROM slots (install, reset, source text) over sel_uim_rom_*; desktop draw_settings/settings_click desktop_ui.c:10552-10675. Done when the 5 rom lines leave the exempt file + rig check.
- I1329c (Amiga track) MUI Your Library sort (sel_uim_lsort_*), chip arrow step (lfilter_step) and chips wrapping onto rows (sel_uim_chips_place, I1221/I1208); desktop side_lib_apply desktop_ui.c:12224, side_chip_place :11758. After I1335/I1362/I1363.
- I1329d (Amiga track, after I1336/I1344/I1361) MUI Search groups and hidden zero-hit sections (sel_uim_search_group, search_section_hidden, I1206), source scope from the opening page (I1224/I1229), pending spinner (I1231), focus remembers the page (sel_uim_page_route).
- I1329e (Amiga track) MUI Now Playing panel draws from sel_uim_np_lines/np_find (I1222f) with multi-line STIL (I1274/I1277) and the note action; replaces the own lines at mui_ui.c:2440.
- I1329f (Amiga track) MUI Queue page group dividers and titles (sel_engine_queue_groups, song_text_generation); desktop queue_groups_sync desktop_ui.c:4626.
- I1329g (Amiga track) MUI Tracker page sample list (sel_engine_sample_get) and tracker position for the viz host (sel_engine_tracker_info); desktop draw_trackerpage desktop_ui.c:7662, viz_host_position :7119. Unverified whether MUI shows samples another way.
- I1329h (Amiga track) MUI "Playing from <list>" context line (sel_uim_context_line); desktop desk_play_context desktop_ui.c:15567.
- I1329i (Amiga track) MUI Back/Forward restores the list scroll position (sel_route_hist_keep/hist_scroll); desktop desk_rt_leave desktop_ui.c:11157.
- I1329j (Amiga track) MUI command lines and texts from the shared model: rating (sel_uim_rate_line, toggle-off I306), playlist Delete/Remove/Rename lines (pldel_*, plremove_*, plrename_line), the missing system file dialog (sel_uim_sysfile_text), song titles from sel_engine_song_text in dialogs, song menu header, composer info.
- I1329k (Amiga track) MUI Home shelves from sel_uim_home_shelves and tile clicks through sel_uim_tile_route (single source, I1189); replaces the own split behind mui_shelf_ctx.
- I1329l (Amiga track, verify first) Mac changes the hook guard cannot see: I1235 Modland format list A-Z + find field, I1286 source drill levels as history pages, I1290 Enter on a playlist then keys act at once, I1072d toast names the blocking source. Read each commit, check MUI, split a row per real gap.
- I1173 add: Visual layout close button + Escape (I1248, sel_uim_tip_viz_close) and the sel_uim_set_layout lines in the exempt file.
