# MUI track B progress (ledger I638, plan 2026-10-25-amiga-mui.md section P8)

Branch: worktree-agent-a48b66d9ed5a16d37. Host tests only; no rig touched.

| Item | Status | Commit | Test |
|---|---|---|---|
| MUI-P8-1 | [~] host done, native button row unbuilt, rig check pending (track A) | 04cd8417 | mui-p8-transport-extras (revert-checked) |
| MUI-P8-2 | [~] host done, native F-key route into mui_ui.c not wired, rig check pending (track A) | see git log "MUI-P8-2" | mui-p8-voices-programs (revert-checked) |
| MUI-P8-3 | [~] host mapping done (sel_uim_libmode_* in new uimodel/browse.h, desktop rewired to it), native Cycle unwired, rig check pending (track A) | b7d0e573 | mui-p8-library-modes |
| MUI-P8-4 | [~] same, sel_uim_scope_* | b7d0e573 | mui-p8-queue-scope |
| MUI-P8-5 | [~] host mapping done (uimodel/viewcmds.h, desktop rewired), native widgets unwired, rig check pending (track A) | 2747e72b | mui-p8-view-buttons |
| MUI-P8-6 | [~] same (sel_uim_stats_*), native panel unwired | 15c10e86 | mui-p8-statistics |
| MUI-P8-7 | [~] host mapping done (uimodel/panels.h: three panes from engine counters via sel_uim_net, Ctrl+A opens Activity); native panes unwired; no kept/AHI/extra-file engine field | 78c034f8 | mui-p8-panels (stub-checked) |
| MUI-P8-8 | [~] host mapping done (uimodel/sources.h lifted from desktop_ui.c, desktop rewired; search source Cycle new); search.source pref key not in prefs.def; native unwired | 78c034f8 | mui-p8-sources (stub-checked) |
| MUI-P8-9 | [~] host mapping done (mode keys 7 8 9 0, no-scroll, AHI text, downloaded bytes, kept marker); info line text itself waits for DUI-P5-2; native unwired | 78c034f8 | mui-p8-infoline (stub-checked) |
| MUI-P8-10 | [~] host mapping done (transport and door help text from the shared tables, hover updates status line); native ShortHelp unwired | 78c034f8 | mui-p8-tooltips (stub-checked) |
| MUI-P8-11 | [~] host mapping done (PREF_SET lines out, prefs read back); ui.mui.geometry, ui.mui.panes, ui.mui.browse_view not in prefs.def yet | 78c034f8 | mui-p8-remember (stub-checked) |
| MUI-P8-12 | [~] host mapping done, Slider unwired; ui.desktop.zoom exemption grep-tested | 2747e72b | mui-p8-tile-steps |
| MUI-P8-13 | [~] host mapping done (mui_door_activate), native door click unwired | 80ae1675 | mui-p8-doors |
| MUI-P8-14 | [~] host mapping done; engine has no amiga.hotkeys/task_priority/group_file pref keys yet (M4-P12-12) | 2747e72b | mui-p8-amiga-settings |
| MUI-P8-15 | skipped by order | | |

Notes
- MUI-P8-2 created a new shared model `sel_uim_fkey_*` (include/selecta/uimodel/fkeys.h, src/uimodel/fkeys.c): F<n> = PLAYLIST_LOAD F<n> then PLAY, Shift+F<n> = PLAYLIST_SAVE F<n>. DUI-P5-8 was still open; the desktop can reuse it. Program naming is Johan's call.
- MUI-P8-15 (alarm) needs new engine scheduler work (ALARM verb is in the k_exempt list).
- Waiting for rig check: P8-1 and P8-2 native halves.
