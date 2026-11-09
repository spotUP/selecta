---
date: 2026-11-09
row: I1230
topic: Viz plugins drawing in the app design (font, theme, layout)
status: decided, implemented for sidregs
---

# I1230: viz plugins in the app design

Johan (Mac, 2026-11-09): "the sid patterncope works but it looks like shit not at all like the selecta app design with
its pixel font etc, make it use the app design modern dark fresh".

## Problem

A viz plugin draws into a host-owned `sel_surface` with five host pixel values (`sel_viz_colors`). It has no access to
the app's text renderer, so the SID PatternScope drew its labels with the private 4 x 6 glyphs of
`plugins/shared/viz/glyphs.h` on a 40 x 8 cell grid with square blocks, and abbreviated words to fit ("Flt", "Pw",
"Pul", "Noi", "Reso"). The look had nothing in common with the desktop UI (rounded cards, theme radius, Inter-like app
font, accent token).

## Searched before deciding

- `plugins/shared/viz/glyphs.h`, `surface.h`: the plugin-side drawing kit (pixel glyphs, rect, line, clear). Only
  `sidregs` includes `glyphs.h`; scope, levels and spectrum draw no text.
- `include/selecta/gui/gui_draw.h`, `gui_text.h`: `sel_gui_round_rect`, `sel_gui_fill_rect`, `sel_gui_text_draw`,
  `sel_gui_text_width` on a `sel_gui_canvas` (RGBA32 or INDEX8): the app's own rasterizer and text renderer.
- `plugins/ui/desktop/desktop_ui.c`: `draw_text` / `desk_font` (the app atlases per size and weight), `desk_mix`,
  `lpx`, the theme tokens, and `viz_draw_area`, which renders the plugin into `viz_px` and blits it.
- `include/selecta/uimodel/tokens.h`: SURFACE, SURFACE_RAISED, SURFACE_PRESSED, TEXT, TEXT_MUTED, ACCENT,
  ACCENT_TEXT, RADIUS.
- `plugins/ui/mui/map/mui_viz.c` (Amiga host) and `tests/viz/sidregs_test.c` (the plugin's unit test).

## Decision: a host pen on the surface

`sel_surface` gains an optional last field `const sel_viz_pen* pen`. The pen is the host's drawing service: a context,
the label line height, the theme radius, five extra theme roles (card, track, text, muted, on-accent) and four calls
(text width, text, rounded rectangle, colour mix). All calls draw into the same surface the plugin got.

The desktop builds the pen per draw in `viz_draw_area`: a persistent `sel_gui_canvas` view over `viz_px` (initialised
once, later only re-pointed: size, pixels, clip), text with `desk_font(d, weight, 12)` and `sel_gui_text_draw`, shapes
with `sel_gui_round_rect`, roles from the theme tokens, radius from the RADIUS token via `lpx`. The existing blit then
carries the result, so no second compositing pass and no ordering problems. The text stats of the pen go to their own
counter (`viz_pen glyphs/missing/shapes` dump line), which is the reachability sentinel.

A host without a pen (Amiga MUI, the unit tests) leaves the field NULL and the plugin keeps its own pixel-glyph
drawing, so the Amiga output is unchanged and its cost is unchanged.

Rejected: the plugin returns a label list that the UI draws afterwards. That splits one picture across two owners,
duplicates the layout maths in every host and breaks z-order (labels on chips). Rejected: shipping the app font into
the plugin. That duplicates the text renderer and the font, and the plugin would still not know the theme.

## Scope

- `sidregs` (SID PatternScope) uses the pen: per chip three voice cards (heading "Voice n", a Filter chip, Frequency
  and Pulse width bars, Gate / Sync / Ring / Test / Triangle / Sawtooth / Pulse / Noise chips with afterglow, a
  vertical envelope bar) and a filter card (Cutoff, Resonance, Low pass / Band pass / High pass). Full words, accent
  fill for lit parts, theme track for unlit ones, theme radius on cards and half of it on bars and chips.
- scope, levels and spectrum do not draw text and already use host colours; they do not share the font problem and
  are unchanged. They can adopt the pen's rounded shapes later without an API change.
