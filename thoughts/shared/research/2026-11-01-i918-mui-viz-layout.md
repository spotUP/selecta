---
date: 2026-11-01
topic: Visualizer layout, modal dialog overdraw, label blink and tiny PatternScope labels (I918, I941, I942, I943)
tags: [visualizer, desktop, aga, mui, dialog, sidregs, I918, I941, I942, I943]
status: design
---

# Visualizer layout and overdraw (I918, I941, I942, I943)

Ledger rows: I918 (todos.md:1380), I943 (1403), I941 (1404), I942 (1405). Johan's decision: MUI 3.8 tabs when every
full label fits the width, else a Cycle chooser; only the visualizer on screen runs; the AGA desktop follows the same rule.

## 0. Correction: the "MUI" screenshot is the AGA desktop UI

The screenshot behind I918/I941 (CleanShot 2026-11-01 at 17.06.35, window "vtcon rig 2 ... run 13 bisect 6c2eb328
start 1") comes from script `vtcon/build/rig2/vtc/j5/r13_6c2eb3_1.script`, which runs
`--ui desktop --backend aga --format index8 --view viz ... --db RAM:... --exec "QUEUE_ADD VTCX:j5/Commando.sid" --exec PLAY`.
The text "Signal Path:" exists only in `plugins/ui/desktop/desktop_ui.c:6357`. The `< >` on the tab row are the nav
history arrows. So Q1 and Q3 are desktop (AGA) defects. The MUI defects in section 4 are real too, but they are not
what the screenshot shows. Open question for Johan below.

## 1. Q1: what draws the dark panel ("dialog in front")

It is the sync-on-launch nag dialog (DLG_SYNC, rows I527/I602). The rig runs on a fresh RAM: database, so every source
is "never synced" and the dialog opens: "N sources not synced for a while", checkbox rows (hippoplayer, Amiga...,
Jamendo..., P/R/S/Z...) and Never / Not now / Sync now (the thin bars). Drawn by `dlg_draw` desktop_ui.c:3183-3207 and
`nagdlg_draw` 3066-3098.

Violated invariant (stated at desktop_ui.c:1243-1252): every region repaint ends with `desk_draw_window_overlays`
(song menu, dialog, notices). `viz_tick` (desktop_ui.c ~6474-6503) draws `viz_panes_draw` / `viz_draw_area` and then
`desk_flush` without the overlays. `viz_visible` (~6291-6305) checks `d->overlay` but not `d->dlg_open`. Each viz frame
therefore paints panes over the modal dialog; the dialog title, rows and button labels are overwritten and come back
on the next dialog repaint.

Second defect, same dialog: `nagdlg_row_rect` desktop_ui.c:3014-3018 computes the column width as
`cw = (p.w - 2m) / (n > NAG_COL_ROWS ? 2 : 1)`, while the column index is `k / NAG_COL_ROWS` (NAG_COL_ROWS = 4,
NAG_MAX = 16, enum near 2927). With more than 8 sources the 3rd and 4th columns (P/R/S/Z) are drawn past the panel and
the window edge. `dlg_panel_rect` 2932-2942 sizes the panel for at most 2 columns.

## 2. Q3: why labels blink (I942) and PatternScope labels are tiny and clipped (I941)

Blink: in VIZ_PAGE `viz_tick` calls `viz_panes_draw` (6439-6447), which redraws every pane label (`draw_text` at
`p.y - 20`, no background clear, outside the pane dirty rect) on every frame. That alternates with the full repaint
(`draw_vizpage`) and with dialog repaints, so labels flicker and ghost. The stray "Oscill" fragment is pane 0's label
clipped by that path.

Tiny labels: `draw_vizpage` 6449-6470 always stacks VIZ_MAX = 4 panes:
`ph = (mn.h - page_y(64) - 40 - 4*28) / 4`, about 32 px on 640x256. sidregs (`plugins/viz/sidregs/sidregs.c`) has
ROWS 8, COLS 40, and draws labels with the 4x6 glyph font (`plugins/shared/viz/glyphs.h`, VS_GLYPH_W 4 / VS_GLYPH_H 6)
at `scale = rh / 7` (min 1, line 99). Labels are hidden only when `rh < 6` or `cw < 3`. With `bh = s->h / chips`,
`rh = bh / ROWS` (render 154-180), a 32 px pane gives rh = 4, so 6 px glyphs overflow their cells and get clipped.
Root: pane height caused by four stacked panes. One selected pane gets about 130 px, rh about 16, scale 2: readable.
Also the switch width is fixed at `lpx(360)` split by `viz_n` (`viz_switch_draw` ~6380-6398), so "SID PatternScope"
is ellipsized to "SID Patt...".

## 3. Q2: why MUI hangs after open with viz (I943)

Unresolved. No log of the run (r16-m1.log) exists in the scratchpad or the j5 directory, the S16 AGA binary is gone,
and no rig run was made in this hunt. Last prints: "window minimum" (`plugins/ui/mui/mui_ui.c:3023`) and missing
"UI open returned" (`apps/selecta/main.c:654`), so the hang is inside the MUI open / first event pass. Code reading
found no deadlock in `mui_canvas_push` (mui_canvas_class.c:43-50, plain p96 write) and sidregs guards `chips == 0`.
Candidates, in order:

1. Both VizAreas (strip and page) run steps against one `mui_vizset` before the window is laid out; a step at w or h 0
   is guarded, so less likely.
2. A requester or Guru on the hidden Workbench screen (task suspended, looks like a hang).
3. A busy loop in the wait-turn step (mui_ui.c:1984-1994) when the 40 ms interval yields 0 and the loop never sleeps.

Probe (step 7 below): rig 2 MUI run with the .mod, then with Commando.sid; during the hang run
`rig.py wasabi ps 'Selecta*'` (CPU and state), `rig.py wasabi guru`, `wasabi windows` (quote any requester), and the
debug stream. Same with .mod: unknown until the probe.

## 4. MUI visualizer defects found (plugins/ui/mui)

- mui_ui.c:1549-1555 adds a Selecta_VizArea plus cycle (`mui_make_cycle`, 1116) to the Now Playing strip, and
  `mui_main_pages` 1605-1630 adds a second VizArea on the Visualizer page (Group_PageMode). Two areas run at once.
- Wait-turn step mui_ui.c:1984-1994 steps the strip area, then the page area when `pane == MUI_PANE_VIZ`, with budget
  "rgba32" (40 ms, `mui_budget_interval_ms` in map/mui_viz.c). Both share one `mui_vizset` tick (sequence + gate,
  `mui_vizset_tick` map/mui_viz.c ~140-152): when both are visible the second (page) area never gets a frame.
- `mui_vizarea_class.c` `viza_draw` 63-72: MUIA_FillArea FALSE (line 140) yet it draws nothing before the first step or
  when the size changed, and `mui_vizarea_step` 150-177 returns without drawing when `!rtg`. Stale pixels stay: the
  "empty dark blocks" and garbage on planar screens and after a resize.
- AskMinMax 74-83: MaxHeight +160 caps the page area, so the page viz never fills the page.

## 5. Fix design (build steps)

Each step names files and the test that proves it. Host tests where possible; one rig check at the end (step 7).

1. **Modal dialog pauses the visualizer (desktop).** `viz_visible` returns VIZ_NONE while `d->dlg_open != DLG_NONE`.
   `viz_tick` ends with `desk_draw_window_overlays` before `desk_flush` (restores the invariant for notices and song
   menu too). Files: plugins/ui/desktop/desktop_ui.c. Test (tests/ui, desktop host fixture): open DLG_SYNC with
   `--view viz`, tick 10 frames, assert the dialog panel pixels are unchanged and the viz frame counter did not move;
   close the dialog, tick, assert frames advance. Revert check: drop the `dlg_open` test, the test must fail.
2. **Nag dialog columns fit.** One function `nagdlg_cols(n) = ceil(n / NAG_COL_ROWS)` used by both `nagdlg_row_rect`
   and `dlg_panel_rect`; the panel width grows with the column count, capped to the main rect, and the column width
   is `(p.w - 2m) / cols`. Files: desktop_ui.c. Test: n = 10 and n = 16, every row rect inside the panel rect and the
   panel inside the window.
3. **One visualizer on the viz page, tabs or chooser (desktop, AGA and RGBA).** `draw_vizpage` shows only the selected
   plugin, filling the page. Above it: a tab strip with full labels when every label fits the measured width, else a
   chooser (`< Label >` button, same look as the existing filter control). The decision is one pure function shared by
   both UIs, in uimodel: `sel_uim_viz_tabs_fit(const uint32_t* label_w, uint32_t n, uint32_t gap, uint32_t avail)`
   returns 1 when tabs fit. The desktop measures labels with its font; MUI with `TextLength` on the window font.
   Labels are drawn only on full repaint; the frame path (`viz_tick`) draws only pane pixels inside the pane rect.
   Remove the fixed `lpx(360)` switch width. Files: plugins/ui/desktop/desktop_ui.c, the uimodel header/source that
   holds the viz model. Tests: pure tests for the fit function (fits, one pixel short, n = 1); desktop host test at
   640x256 that the pane height is at least 7 * 8 * chips px (so sidregs rh >= 7) and that the frame path touches no
   pixel outside the pane rect (diff before/after one tick).
4. **sidregs hides labels it cannot draw.** Draw labels only when `rh >= VS_GLYPH_H + 1` and `cw >= VS_GLYPH_W + 1`
   (current threshold 6 / 3 lets 6 px glyphs into 4 px rows). Files: plugins/viz/sidregs/sidregs.c. Test: render into
   a 320x32 surface, assert no label pixels; 320x130, assert labels present.
5. **MUI: one VizArea per visible place, tabs or Cycle.** Keep the Now Playing strip VizArea; the Visualizer page gets
   one VizArea plus a chooser built per `sel_uim_viz_tabs_fit`: Selecta_AATabs (`mui_aatabs_new`,
   plugins/ui/mui/mui_aatabs_class.h, already the project's MUI 3.8 tab row) when all full labels fit, else the existing
   cycle (`mui_make_cycle`). Re-evaluate on window resize (MUIM_Show). Only the area on screen steps: the page area
   when the Visualizer page is shown, else the strip area, never both; give each area its own vizset tick state so a
   step on one cannot starve the other. VizArea: draw a background fill in `viza_draw` when no frame of the current
   size exists (or set MUIA_FillArea TRUE), so no stale pixels; MaxHeight MUI_MAXMAX for the page area. Files:
   plugins/ui/mui/mui_ui.c, mui_vizarea_class.c, map/mui_viz.c. Tests: tests/ui/mui_viz_test.c host tests for the
   vizset per-area tick (two areas, only the shown one advances; switching pages moves the stepping) and for the
   chooser choice at two widths.
6. **AGA desktop chooser reuse.** The desktop chooser in step 3 reuses the filter control drawing (I831 `< label >`) if
   it is on main; otherwise build it once in desktop_ui.c and let I831 reuse it. Search first.
7. **One rig check at the end (rig 2, --ram 256, silent).** AGA desktop `--view viz` with fresh RAM db (sync nag
   opens): dialog stays intact, no blink, PatternScope labels readable, 3rd/4th nag column inside the panel. MUI on the
   same rig with virgill-that_meme_again.mod, then VTCX:j5/Commando.sid: window opens, Visualizer page shows one viz,
   tabs or cycle per width; plus the I943 probe from section 3 if it still hangs.

## 6. Open questions for Johan

1. Was the I918/I941 observation on the AGA desktop UI (the screenshot is the desktop, not MUI)?
2. Modal dialog pauses the visualizer (recommended, step 1) versus visualizer keeps running under a dialog that is
   redrawn after each frame (costs a dialog repaint per frame on 68k).
3. Should the sync nag open on every fresh-db rig run? It covers the screen in every test; a `--no-sync-nag` or a
   seeded "synced" state for test dbs would keep rig shots clean.

## Johan's decisions (2026-11-01)

- The 17:06 "chaos, drawn on top" screenshot is the AGA desktop UI (`--ui desktop --backend aga`). Desktop overlay and viz pane fixes go first; MUI VizArea fixes follow as their own steps.
- A modal dialog open over the visualizer does NOT pause it: the visualizer keeps animating and the dialog must be redrawn over it each frame (viz_tick must call desk_draw_window_overlays, or clip viz output to exclude the dialog rect). This overrides the design's recommended "pause" option.
- The sync nag is suppressed for test databases (a test flag/env), so rig shots show the real layout; the nag keeps its own test case.
