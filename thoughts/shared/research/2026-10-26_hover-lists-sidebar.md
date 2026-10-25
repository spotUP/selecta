---
date: 2026-10-26
topic: Hover on list rows and in the sidebar (I345, I346)
status: design, not implemented
ledger: thoughts/spot/todos.md rows I345, I346
plan: thoughts/shared/plans/2026-10-24-desktop-ui.md (add DUI-P3-11, DUI-P3-12 with the fix)
---

# Hover on list rows and in the sidebar

Johan, 2026-10-25, testing the live Mac window after the I341 fix: "lists in general have no hover effect" and, for the
sidebar, "it would be nice if the hover thing followed the mouse ... it gets removed between the items ... a nicer smooth
flow". After checking macOS: "only the selected one has the background, the others can get a hover fg color only", then
"same for the lists change fg color only" and "and selected gets a bg color". One rule for doors and list rows.

## What the code does today (desktop_ui.c, commit 45c034b)

Two unrelated hover mechanisms exist.

1. **Control hover (shared).** `hover_reg` (line ~404) lists a hoverable box while it is drawn; `desk_hover_move` (~5187)
   hit-tests the list, sets `d->hover_box`, restarts the `ANIM_KEY_HOVER` tween (FAST_MS 100, ease out) and calls
   `hover_repaint`, which redraws the region holding the box (sidebar, main, player). `hover_weight` returns the tween
   weight for the current box only; `hover_fill_of` blends the control's fill toward `SURFACE_HOVER`. Buttons, chips,
   tabs and the sidebar doors use it. Only ONE box carries weight: the door the pointer left loses its fill at once.
2. **Songs row hover (ad hoc).** `d->hover_row` (line 175) is set by the Songs page move handler (~5086) and painted by
   `draw_songs` (~1057) as `sel_gui_scrim(..., HOVER_LIGHT, 36u)`. No tween. 36 alpha of a light token over the dark
   panel is close to invisible. Only the Songs page has it: `draw_library` rows (composers, tree, columns, files,
   history, `lib_row_rect` ~1558), the queue and the playlist rows have no hover at all.

Home tiles have a third path (`hover_tile`, `draw_home_content` ~3488) that already uses the ANIM_KEY_HOVER weight.

## Design

### I345: list rows use the control hover path

- Every list row (songs, library rows in all five modes, queue, playlists, any `*_row_rect`) calls `hover_reg(d, row)`
  while drawn. Same rule as the sidebar doors (Johan): the SELECTED row (playing song, chosen composer, focused tree
  node, the row a keyboard cursor sits on) gets the `SURFACE` round-rect background; a hovered unselected row gets NO
  background, its text ink blends `TEXT_MUTED -> TEXT` (or `TEXT -> ACCENT` where the row already uses TEXT; pick one per
  list and name it in the plan item) by the hover weight, fading in and out like the doors (I346 `hover_weight_out`).
  Where a list has no selection concept today (files, history), add none: hover ink only.
- Shared helper: `hover_ink_of(d, box, muted, full)` returns the blended ink from the in and out weights; `draw_sidebar`
  and every row painter call it. One mechanism, no per-view hover state.
- Delete `hover_row`, the scrim at ~1057 and the move handler branch at ~5086; `desk_hover_move` already handles the
  move and `hover_repaint` already redraws main (`rect_inside(hover_box, main)` branch).
- `hbox[64]` is too small for a list: count the worst case from the layout (visible rows at `LIB_ROW_H` 48 px in the
  tallest main region the layout allows, plus doors, transport buttons, chips, tabs) and size the array from that
  count with a comment naming the sum. `hover_prune` already drops boxes inside a region before it is painted again.
- Hit-test cost: `desk_hover_move` is a linear scan; a few hundred boxes per move is fine.

### I346: sidebar hover is foreground only, and fades both ways

- Selected door: keeps the `SURFACE` round-rect background (unchanged).
- Unselected hovered door: NO background. Its icon and label ink blends `TEXT_MUTED -> TEXT` by the hover weight.
  Remove the `hover_fill_of` background branch in `draw_sidebar` (~4027-4029).
- Smooth flow: keep `hover_prev` (the box the pointer left) and `hover_prev_ms` (clock when it left). Its ink weight is
  `65536 - tween value` over the same FAST_MS, so the old door fades back while the new one fades in. `hover_weight`
  grows a sibling `hover_weight_out(d, box)` for that; only `draw_sidebar` uses it in this item.
- Between items (gap rows of the door column): nothing is highlighted, by Johan's macOS reference; the fade-out covers
  the "removed between items" feel.

### Tests (one reachability test per feature, the rest where it lives)

- `tests/dui/hover_rows.cmake` (DUI-P3-11): live loop (`SELECTA_SDL_TEST_LIVE=1`, `SELECTA_SDL_TEST_TIMED`) with a db
  of a few songs, `--view library`, timed move onto row 2, then quit; `--stats` asserts `hovers>=1`,
  `hover_box paints>=1`, and a frame dump shows the row 2 text pixels differ from row 3 text pixels while the row
  background pixels (left margin, no text) are equal (ink moved, no background).
- `tests/dui/hover_sidebar_ink.cmake` (DUI-P3-12): timed move onto door B, 50 ms later onto door C, quit at 60 ms; a
  new `--stats` line `hover_ink=<new weight>,<old weight>` (or a debug dump) asserts the new door's weight is above 0
  and below 65536 and the old door's weight is above 0 and below 65536 at that frame; then a second run at +200 ms
  asserts new 65536, old 0.
- Both tests fail on 45c034b (no hover on library rows; no fade-out).

## Out of scope

Sliding highlight pill between doors (first idea, superseded by Johan's macOS reference). Amiga rtg hover (I287 track).
