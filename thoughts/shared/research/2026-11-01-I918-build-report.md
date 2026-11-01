# I918 build report (branch i918-viz-layout)

Design: thoughts/shared/research/2026-11-01-i918-mui-viz-layout.md. Steps done: 6 of 7 coded and host-tested; step 7 (rig) only partly run.

## Commits
- 563d9aa6 steps 1-4 in one WIP commit (per-step commits not possible; gap): libfilter `sel_uim_viz_tabs_fit`, desktop nag columns, desktop viz chooser, one-pane viz page, sidregs label threshold.
- 81f74f57 step 1 follow-up: clip around `desk_draw_window_overlays` in viz_tick.
- 32b58871 step 5: MUI Visualizer page chooser (AATabs or cycle), tall viz area, backdrop paint.
- Step 6 (second area) is covered by the step 3 chooser reuse; no separate work.

## Tests
- dui-viz PASS, dui-sync-banner PASS, viz-sidregs PASS, mui-libfilter PASS, mui-overlays PASS, mui-viz-chooser PASS.
- Revert proofs: desktop_ui.c at a742f102 makes dui-viz and dui-sync-banner FAIL; sidregs threshold `g->rh < 6` makes viz-sidregs FAIL; removing `mui_vizarea_set_tall` makes mui-viz-chooser FAIL.
- Not proven: mui-libfilter revert. No dedicated test for the dialog-over-viz overlay redraw (gap). mui-viz-chooser is a source-wiring check only.

## Rig 2 verdicts (build/amiga binary, 4,930,908 bytes)
- I943 with virgill.mod, MUI: no hang, plays, no Guru. With Commando.sid, MUI: INCONCLUSIVE. A previous Selecta was still running (Break did not end it), two instances competed; UI open returned after ~187 s rig time, no Guru. Rerun with one instance needed.
- I918 MUI Visualizer page: View > Visualizer Page opens, tab chooser buttons appear at 570 px (chooser works). The viz area draws NO content: pixel statistics over 6 shots show only the background color, zero change. Log says screen plan mode depth 8 and "ttengine or Inter missing"; the viz area gates on `d->rtg`, Picasso96 and `sel_uim_budget_for("rgba32")`, so it is likely the depth-8 non-RTG screen gate (pre-existing, not shown to be caused by my change). UNVERIFIED: not checked on a depth-24 screen; not compared with main.
- I918/I941/I942/I944 AGA desktop: NOT RUN. `rig.py aga` ended with "Workbench stayed PAL (W43)" and the rig stopped. Johan's visual items (dialog in front, PatternScope labels, blinking texts, nag columns) remain unverified; no image was viewed.

## Cleanup
Rig 2 stopped. My copies of Selecta_i918b, virgill.mod, i918* logs and the screenshots are deleted. build/sdl (86 MB) and build/amiga stay with the worktree.
