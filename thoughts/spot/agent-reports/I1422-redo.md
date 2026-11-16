# I1422 redo: page sample rows, not wide names

Johan: "no its not that the sample names are wider there are too many rows".

Searched: git log --grep=I1422, desktop_ui.c np_msgpg*, NP_PAGE_MS, desk_timers_wait/tick, tests/dui/np_message_paging.cmake.
Reused: the first fix's pager (clock now_ms / NP_PAGE_MS, np_msgpg fields, wake in desk_timers_wait, redraw in desk_timers_tick, dump line `nowplaying msgpage <page> of <pages>`, the dui test). No new timer.
Removed: the horizontal paging of wide lines (wrong ask); rows now draw with an ellipsis at the panel width.
Change: draw_nowplaying counts message rows; with more than NP_MESSAGE_LINES (6, the box height) it shows one page of 6 rows, advancing every 2 s and wrapping to page 1. Six rows or fewer: no paging (pages = 1).
Test: dui-np-message-paging with fixture tests/m3/message_long/message_rows.it (15 rows = 3 pages): page 1 after 2.3 s, wrapped to 0 after pages*2 s + 0.3 s. Old fixture message_long.it removed.
Twins (RULES 4a, not built): text UI info panel and MUI Now Playing sample list need the same page-of-rows rotation (same interval, wrap, no paging when rows fit).
