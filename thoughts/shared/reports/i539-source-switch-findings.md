# I539 findings (read-only, static; ROOT CAUSE NOT CONFIRMED)
Worktree: .../agent-aef30ca469ddcc5ed, file plugins/ui/desktop/desktop_ui.c

## Q1 draw vs hit geometry: SAME source, no divergence
- Rects: src_toggle_rect 4869, src_sync_rect 4873, lib_row_rect 2539 (comment: "drawing, hit-testing and the dump all go through it").
- Draw: draw_lib_sources 5057 (tg = src_toggle_rect(lib_row_rect) at ~5069). Hit: src_click 4902 (same two helpers).
- Both use mn = panel_rect(d,"main") (838), lpx scale, same lib_body_y/lib_off_px.
- Gates are identical for draw and click: draw_library only when page==PG_LIBRARY && st->composers!=NULL (5450); library_click returns 0 under the same condition (5223). Tabs are tested before src_click (5170-5177) and lib_mode==5 routes to src_click (5176).
- git diff main...HEAD on desktop_ui.c touches NO click/src/lib_row/in_rect code; layout.c adds only sel_uim_layout_compute_tier (rail 160 wide at 640, layout.c:~74). Hit region test (in_main, 7422) uses sel_uim_hit on px_to_logical coords of the same d->layout used by region_rect (843). So P3 did not change the switch hit path.

## Q2 coordinate mapping: consistent
- amiga/input.c:134-135 passes raw MouseX/MouseY (window-relative canvas rows, 640x256). desk_route_input 7875-7882 multiplies y,y2 by canvas ydiv (2 on AGA, set 8393) -> square-pixel space, same as layout/draw (cvh 580). No difference between switches and tabs/rail; if tabs/rail click, mapping is fine. (Not live-verified: I could not click on rig 3 without a toggle coordinate; the Amiga build has no state dump, the dump at 4083 is posix-only.)
- Note raw.button is 1 for left (input.c:149) while handlers test ev->button==0; the conversion happens in the gui input layer, same for all clicks (not checked further).

## Q3 handler: not a no-op on Amiga
- src_toggle_send 4882 sends "PREF_SET library.sources_off ..." + "SOURCES" via desk_send 6648 (engine-generic). Engine side platform-neutral: engine.c:1042/5398, search.c:405. Reply handled in desk_on_event 8757-8790 (repaints only if the row changed).
- No #ifdef/Amiga guard on this path.

## Conclusion so far
Static reading finds no switch-specific difference; the draw/hit/handler chain is shared with the tabs. The bug is either (a) a pre-existing issue not from P3 (the sources click path is unchanged on the branch), or (b) the click is claimed earlier in desk_pointer_view (7421-7457: dlg_pointer, overlay, song_menu_pointer, footer_bar_pointer, desk_button_at, nav arrows, sidebar_door_at, viz_switch_at) by a rect that now overlaps the toggle at 640 wide with the 160 rail, or (c) the PREF_SET/SOURCES round trip does not change state on Amiga (d->src_wait / SEL_EVT_LIBRARY_RESULT SRC tags).
## Next decisive step (needs the other agent's build, or a rig repro)
1. Add/enable counters: d->sends (6652) before/after one click on a toggle; d->clicks (7455). sends unchanged => click claimed earlier (b); sends +2 but row unchanged => (c).
2. Host side: run the posix desktop with --ydiv 2 at 640x512 on the Sources tab, dump "sourcetoggle" rects (4083-4090), compare them to rects of footer_bar/song_menu/nav/sidebar_door/desk_button_at at the same point.
## Regression test idea
Host mem backend, --ydiv 2, 640x512 (already supported: ydiv_opt 8393), --view library, set lib_mode 5, feed --input click at the centre of each dumped sourcetoggle rect (y divided by 2 as the AGA backend delivers rows), assert src row `enabled` flips in the next dump and d->sends rose by 2 (sentinel). One test through desk_route_input.
