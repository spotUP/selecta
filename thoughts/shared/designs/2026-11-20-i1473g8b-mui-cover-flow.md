---
date: 2026-11-20
row: I1473g8b (MUI Cover Flow on the canvas, RTG and AGA)
status: design, not built
parent: thoughts/shared/designs/2026-11-20-i1473g-mui-cover-grid.md (Johan's answer D6: Grid AND Cover Flow on the canvas in this batch)
twins: Mac plugins/ui/desktop (reference: draw_flow_rows, desktop_ui.c:9633; keys desktop_ui.c:15765); text plugins/ui/text (Cover Flow is a text layout there, no covers: RULES 9 / I1222f exemption covers the images)
plan rows touched: plans/2026-10-25-amiga-mui.md MUI-P5 (canvas); the parent doc section 4 rule "Cover Flow masked out on planar" is reversed by D6
---

# MUI 3.8 Cover Flow (Library and Search, RTG and AGA)

## 1. What the Mac Cover Flow shows (measured from the code)

Source: `plugins/ui/desktop/desktop_ui.c` 9422-9715 (draw), 15765-15790 (keys), 4541 (no Statistics strip), 11048 (Search).

- **Shown when** the engine view style is `SEL_VIEW_STYLE_COVERFLOW` (`lib_grid_on` is true for Grid and Cover Flow). Two pages, as the
  grid: Library (`draw_lib_flow`, `lib_reg = 1`) and Search (`draw_flow_rows(d, mn, &fv, y, 0)`, song rows registered by key).
  The Library Statistics strip is hidden in Cover Flow (`lib_stats_on`), so the flow starts right under the tab row.
- **Geometry** (I391), integer only, all covers are squares:
  - Constants `FLOW_SIDES = 3`, `FLOW_STEP_DIV = 4`, `FLOW_SHRINK_DIV = 5`, reference centre size `FLOW_ART = 240` lpx.
  - Neighbour k (k = 1..3) sits `flow_off(big, k) = k * big / 4` from the centre line and is `flow_size(big, k) = big - k * big / 5`
    (100 / 80 / 60 / 40 % of the centre). Every neighbour shows 0.15 x big of itself beside the nearer cover.
  - Stack width `flow_stack_w(big) = 2 * (flow_off(big, 3) + flow_size(big, 3) / 2)` = 1.9 x big.
  - Centre size `big = min(ref * avail / flow_stack_w(ref), area_h - capt)`, floor 48 lpx: the stack fills the free width or height.
    `capt = HOME_TILE_TEXT_GAP + HOME_TITLE_H + HOME_META_H` (8 + 18 + 16 lpx). Stack + caption are centred vertically in the free area;
    every cover is centred on the same horizontal centre line (`y = y0 + (big - sz) / 2`).
- **Perspective / scale**: none. No skew, no reflection: neighbours are only smaller and darker. Darkening is a scrim in the
  `SEL_GUI_TOKEN_SCRIM` colour with alpha `flow_scrim[] = {0, 90, 150, 150}` by slot.
- **Fractional slots** (I400): each cover sits at `p = index - centre` in 16.16; `flow_slot(big, |p|)` interpolates size, offset and
  scrim between whole slots; past slot 3 the cover shrinks to nothing behind its neighbour, so nothing pops in or out.
  Up to `2 * FLOW_SIDES + 4` covers are candidates; they are insertion-sorted by |p| descending and drawn far to near.
- **Selected item**: the cover nearest the centre (drawn last). It gets the shadow (`sel_gui_shadow_draw`), the hover lighten
  (`HOVER_LIGHT` scrim 48), and it is the only click target (`lib_view_row_reg` in Library, `song_row_reg` in Search). There is no
  accent outline in Cover Flow (the grid has one, the flow does not).
- **Caption**: only for the cursor row, under the centre cover, centred in its width: title (bold 14, TEXT) and artist (13, TEXT_MUTED)
  through `lib_tile_text(..., centre = 1)`.
- **Animation**: a 220 ms ease-out tween (`SEL_GUI_ANIM_NORMAL_MS`, `SEL_GUI_EASE_OUT`, key `ANIM_KEY_FLOW`) of the fractional centre from
  `flow_from` to the cursor (`flow_centre_fx`). A key pressed mid-glide starts the new glide where the covers are.
- **Input**: Left / Right send `VIEW_CURSOR -1 / +1`; Up / Down do nothing in Cover Flow; Enter is the bound `VIEW_ACTIVATE`. The wheel
  goes to `lib_wheel` (the list scroll offset `lib_q16`), which the flow does not read, so the wheel does not move the flow (read from
  code, not seen live: unverified). Side covers are not click targets.
- **Tests**: `tests/dui/grid_coverflow.cmake` (golden `coverflow`, sentinel `flow draws N centre I`, Right moves by 1 and clamps).

## 2. Shared geometry (one source of truth)

There is no uimodel flow geometry today: `flow_off`, `flow_size`, `flow_stack_w`, `flow_slot`, `flow_scrim`, `flow_centre_fx` and the
candidate sort are `static` in desktop_ui.c. Row g8b1 moves them to the uimodel, the same move g1 did for the grid
(`include/selecta/uimodel/grid.h`, `src/uimodel/grid.c`). New `include/selecta/uimodel/flow.h` + `src/uimodel/flow.c`, pure C99, no
allocation, no theme:

```c
enum { SEL_UIM_FLOW_SIDES = 3, SEL_UIM_FLOW_MAX = 2 * SEL_UIM_FLOW_SIDES + 4 };
typedef struct sel_uim_flow_cover {
    uint32_t g;          /* index into the view window (0 .. row_n-1) */
    sel_gui_rect r;      /* square, in the caller's coordinates; the int rect type stays the caller's (int32 x, y, w, h) */
    int32_t alpha;       /* scrim 0..150 */
    int32_t a;           /* |p| in 16.16, for the dump line */
} sel_uim_flow_cover;
/* Centre size for a free area (the Mac formula; `ref` and `floor` in the caller's pixels). */
int32_t sel_uim_flow_big(int32_t avail_w, int32_t area_h, int32_t capt, int32_t ref, int32_t floor);
/* One cover at slot distance a (16.16): size, offset from the centre line, scrim. Whole slots = 100/80/60/40 %, 0/90/150/150. */
void sel_uim_flow_slot(int32_t big, int32_t a_q16, int32_t* sz, int32_t* off, int32_t* alpha);
/* Fractional centre: `from_q16` gliding to `cursor` as glide_q16 runs 0..65536 (65536 = at rest). */
int64_t sel_uim_flow_centre_q16(int64_t from_q16, uint32_t cursor, int32_t glide_q16);
/* The visible covers in draw order (far to near, the front cover last). cx = centre line x, y0 = top of the centre cover.
 * Absolute indices: window_start + g. Returns the count (<= SEL_UIM_FLOW_MAX). */
uint32_t sel_uim_flow_layout(int64_t centre_q16, uint32_t window_start, uint32_t row_n, int32_t big, int32_t cx, int32_t y0,
                             sel_uim_flow_cover* out, uint32_t cap);
/* Click: the window index of the cover under (x, y) among `out[0..n)`, front first; -1 for none. `front_only` = 1 is the Mac
 * rule today (D-F3). */
int32_t sel_uim_flow_hit(const sel_uim_flow_cover* c, uint32_t n, int32_t x, int32_t y, int front_only);
```

- `sel_gui_rect` lives in `selecta/gui/gui_draw.h`; if the uimodel must not include gui headers (check `grid.h`: it only includes
  `stdint.h`), the cover carries `int32_t x, y, sz` instead. The builder follows whatever `grid.h` does.
- Desktop: `draw_flow_rows` keeps the painting (theme tokens, shadow, hover, row registry) and calls `sel_uim_flow_big`,
  `sel_uim_flow_centre_q16` and `sel_uim_flow_layout`. Proof of no drift: the `dui-grid-coverflow` goldens and the `flow_rec` dump
  stay byte-identical.
- MUI pure map (`plugins/ui/mui/map/mui_canvas.{c,h}`), next to the grid functions; the class stays glue:
  - `mui_flow_layout(const mui_grid_style* s, int32_t w, int32_t h, int64_t centre_q16, uint32_t n, sel_uim_flow_cover* out)`: computes
    the free area from the canvas size and `s->caption`, calls `sel_uim_flow_big` (ref = 240 at the canvas scale, see D-F1) and
    `sel_uim_flow_layout` with window_start 0 (the tiles are already the window). Also returns the caption rect of the front cover.
  - `mui_canvas_draw_flow(mui_canvas*, const mui_grid_style*, const mui_tile*, n, int64_t centre_q16, mui_art_fn, void*)`: RTG (section 3).
  - `mui_canvas_draw_flow_aga(const mui_grid_style*, w, h, tiles, n, centre_q16, art, ctx, mui_aga_cache*, const mui_aga_sink*, void*)`:
    AGA (section 4).
  - `mui_flow_signature`: not needed; `mui_grid_signature(tiles, n, cursor)` already covers what Cover Flow shows. During a glide the
    class redraws per tick regardless of the signature.
- Class (`plugins/ui/mui/mui_canvas_class.c`): one new field `int flow` in `canvas_data` plus the glide state (`int64_t from_q16`,
  `uint32_t glide_ms0`, `int gliding`), set by a new `mui_canvas_set_flow(Object*, int on)`. `mui_canvas_set_grid` keeps its signature
  (g6 callers stay); when `flow` is on and the cursor changed, it records `from_q16 = current centre` and the start time. `canvas_draw`
  branches `d->flow` before the grid paths. The glide value uses the shared easing in `selecta/gui/gui_anim.h` (the timeline API the
  desktop uses; the builder reads `sel_gui_tween_*` / `sel_gui_ease_at` there and picks the call that fits a single key).
- Captions: `canvas_captions` gets a flow branch: one caption, title (TEXTPEN, bold soft style) and artist (SHADOWPEN), each centred in
  the front cover's width with `TextFit`, the same pens and font rule as the g4 grid captions.

## 3. RTG draw path

Reuses the g4 path exactly: `mui_canvas_fit` -> pure draw into the RGBA canvas -> `mui_canvas_push` (Picasso96 `WritePixelArray`) ->
`Text()` captions after the push.

- Background fill `s->background`, then for each cover of `mui_flow_layout` (far to near):
  - art words from `art(ctx, art_id)` (square, loader edge, D-F1) blitted into the cover rect: `sel_gui_blit_image_round` (nearest,
    rounded with `mui_cover_radius(sz)`, the grid's radius rule) when growing; when shrinking (side covers) `sel_gui_blit_image(..., box = 1)`
    then `mui_round_corners` against the background. No art: `mui_platform_cover` + `mui_round_corners`, as the grid.
  - scrim: `sel_gui_scrim(c, r, scrim_rgb, alpha)` with alpha from the layout. The scrim colour joins `mui_grid_style` as `scrim`
    (0xAARRGGBB, from the theme token `SEL_GUI_TOKEN_SCRIM` like the grid pens) .
  - front cover: shadow first (`sel_gui_shadow_bake` once per size in the class data, `sel_gui_shadow_draw`), then the cover.
- No hover lighten: MUI 3.8 has no pointer hover on Area objects and the MUI List shows none either (default picked: no hover; it is in
  the Mac's look only while the pointer rests on the cover).
- Cost per frame (estimate, unverified, from ~15 cycles a nearest pixel on a 68020 and ~3 on a 68060): covers write about
  big^2 x (1 + 2 x (0.64 + 0.36 + 0.16)) = 3.3 big^2 pixels with overdraw. At big = 300 (1024 x 768 RTG): ~300 k pixels = ~4.5 M cycles,
  ~320 ms on a 14 MHz 68020, ~20 ms on a 50 MHz 68060, plus the push of the whole canvas (`w x h` words over the RTG bus). So: glide on
  68040/68060 RTG, step (one frame per key) on 68020/68030 RTG (D-F2).
- Optimisation only if the rig measures it slow: push only the stack box (`sel_gui_dirty` union) instead of the whole canvas. Not in the
  first build (shortcut, upgrade when `mui_flow_ms_max` on the rig says so).

## 4. AGA path

Planar screens get Cover Flow too (D6 reverses the parent doc's "Cover Flow RTG only"). Everything goes through the g5 sink: the pure
core emits rects and cube-index squares, the class does `RectFill` and `WriteChunkyPixels` with the shared 27 cube pens (`d->pens`,
held to Cleanup) and three role pens per draw.

- **No perspective hardware needed**: the Mac flow has no perspective either, only scaled squares and a scrim. Scaling is nearest
  (`mui_cover_scale8`, already used by the single AGA cover) from the cached square index image to the slot size. No skew.
- **No overdraw**: because every nearer cover is taller and centred on the same line, the part of a side cover that shows is one full-height
  vertical strip (0.15 x big wide at rest, the gap between its outer edge and the nearer cover's outer edge). The pure core clips each cover
  to that strip and emits only it, so every screen pixel of the stack is written once. Pixels per frame:
  big^2 x (1 + 2 x 0.15 x (0.8 + 0.6 + 0.4)) = 1.54 big^2.
- **Scrim on a palette**: a darkened cover is the same cube indices through a dim table. `mui_cover_dim_lut(int32_t alpha, uint8_t lut[27])`
  (pure) maps each cube index to the cube index of its colour darkened by alpha towards the scrim colour; levels 0 / 90 / 150 give three
  tables, built once. During a glide the interpolated alpha snaps to the nearest of the three. The class pen loop then maps
  `pens[lut[idx]]`: no extra pens, no extra pass, zero extra cost.
- **Sink extension**: `mui_aga_sink` gains `void (*chunky)(void* ctx, sel_gui_rect r, const uint8_t* idx)` with `r.w x r.h` indices
  (already scaled, clipped and dimmed). The class callback maps indices to pens into `d->chunky` (grown to big^2 bytes) and calls one
  `WriteChunkyPixels`. No art: `rect` with `mui_tile_tint` (as the grid). The front cover shadow is skipped on AGA (no alpha on a palette;
  default picked).
- **Index cache**: `mui_aga_cache` (g5) at the flow loader edge (D-F1). Slots 12 = 9 visible + 3 ahead. 144 edge: 12 x 20.7 KB = 249 KB
  fast RAM. A new cover entering the stack costs one remap (`mui_cover_remap`, ~60 cycles a pixel: 144^2 = 1.24 M cycles, ~90 ms on a
  14 MHz 68020, once per album); the g3 loader's one-miss-per-tick rule preloads the next 2 covers on each side while idle.
- **Cost per frame** (estimate, unverified; nearest scale + dim + pen map ~20 cycles a pixel on a 68020, ~4 on a 68060;
  `WriteChunkyPixels` c2p ~40-60 cycles a pixel on a 68020, chip-bus bound on a 68060):
  | Screen | big (est.) | pixels | 68020 14 MHz | 68060 50 MHz |
  |---|---|---|---|---|
  | 640 x 256 | ~110 | ~19 k | ~100 ms | ~6-8 ms |
  | 640 x 512 | ~250 | ~96 k | ~500 ms | ~30-35 ms |
- **Animation**: 68020/68030: no glide, one redraw per key (a 220 ms glide would show at most two frames; a step reads cleaner). 68040/68060:
  glide at the refresh tick, `wait_cap_ms` 40 while gliding (the `mui_cover_tick` pattern), about 5 frames per step. The tier comes from the
  CPU tag of `sel_machine_probe` (`include/selecta/machine.h`), one pure function `mui_flow_glides(cpu)` (D-F2).
- **OS below v40**: tint squares only, as the grid (no `WriteChunkyPixels`).

## 5. Input (one design with g7)

g7's pure functions take the style kind, so Grid and Cover Flow share one key and click map (no second handler, no second event node):

- `mui_grid_key(uint32_t kind, uint16_t rawkey, uint32_t cols, char* line, size_t cap)`: GRID as g7; COVERFLOW Left/Right ->
  `VIEW_CURSOR -1/+1`, Up/Down -> no line (the Mac returns without sending), Enter -> `VIEW_ACTIVATE`.
- Wheel (NewMouse RAWKEY 0x7A/0x7B): D-F4.
- `mui_grid_click(kind, ...)`: COVERFLOW uses `sel_uim_flow_hit` over the layout the class drew last (kept in `canvas_data`, at most
  `SEL_UIM_FLOW_MAX` covers). Front cover: the same single-click semantics g7 copies from the Mac row registry; double-click
  (`DoubleClick`) -> `VIEW_ACTIVATE`; right-click -> the song menu of the front cover. Side covers: D-F3.
- Every cursor change mid-glide starts the next glide from the current fractional centre (the Mac I400 rule), in the class.
- Search page: album tiles route through `sel_uim_tile_route`, as the grid (parent doc section 3).

## 6. Tests (host, `LABELS mui` unless noted; run by name only, rule 6a)

| Test | Asserts |
|---|---|
| `uim-flow-geom` (new, `tests/uimodel`, label uimodel) | whole slots give 100/80/60/40 % sizes, offsets 0/.25/.5/.75 x big, scrim 0/90/150/150; slot 3.5 is half the slot-3 size; layout is far-to-near with the front cover last and at most `SEL_UIM_FLOW_MAX` covers; window edges clip; `sel_uim_flow_centre_q16` at 0 / 32768 / 65536; `flow_stack_w` fit is exact for avail = 1.9 x big; hit front-only and all-covers |
| `dui-grid-coverflow` (existing) | goldens `coverflow` and the `flow draws N centre I` / Right / clamp sentinels unchanged after the desktop calls the uimodel (the 1:1 proof) |
| `mui-flow` (new, `mui_canvas_test flow`) | RTG compose of 7 tiles, cursor 3, at rest: golden `MUI_FLOW_GOLDEN`; mid-glide (centre 3.5): golden `MUI_FLOW_GLIDE_GOLDEN` differs from rest; front cover rect equals `sel_uim_flow_layout`; caption rect under the front cover |
| `mui-flow-aga` (new, `mui_canvas_test flow-aga`) | recording sink: strips count = layout count, every strip index < 27, total pixels <= 1.6 x big^2 (no overdraw), dim tables map level-2 channels down at alpha 90 and 150; golden `MUI_FLOW_AGA_GOLDEN` over the strip bytes |
| `mui-grid-input` (g7, extended) | COVERFLOW: Left/Right -> `VIEW_CURSOR -1`/`+1`, Up/Down -> none, Enter, wheel per D-F4, click front, click side per D-F3, double-click -> `VIEW_ACTIVATE` |
| `mui-grid-switch` (g6, extended) | COVERFLOW -> canvas shown in flow mode and List hidden on RTG and planar; COVERFLOW no longer packed (`mui_style_pack` not called); Compact still packed; `mui_style_mask(0)` now includes Cover Flow |
| `mui-style-*` / `mui_test` / `mui_contract_test` / `mui_p8_test` (existing) | updated where they assert Cover Flow is masked on planar or packed into one text row |

Golden hashes: new, blessed once by the row that adds them, stored in `tests/CMakeLists.txt` beside `MUI_GRID_GOLDEN` /
`MUI_GRID_AGA_GOLDEN` (no value can be given before the code exists).

Rig (in g9, section 9 of the parent doc): add View > Style > Cover Flow on both screens, Right x3, Left x1, Enter plays, click the front
cover; exit-stat sentinels `mui_flow_draws= mui_flow_frames= mui_flow_ms_max= mui_flow_glide=`; one cropped shot per screen
(`~/Desktop/selecta-shots/I1473g8b-<screen>.png`) through a sonnet agent.

## 7. Build rows (one sonnet agent each)

Order after g6 (wiring), g7 (input), g8 (chooser); g8b1 can start now because it touches only the uimodel and the Mac file.

1. **I1473g8b1 shared flow geometry** (Mac slot): `include/selecta/uimodel/flow.h` + `src/uimodel/flow.c` (section 2 API), desktop
   `draw_flow_rows` / `flow_centre_fx` call it, statics deleted. Tests `uim-flow-geom` (new), `dui-grid-coverflow` unchanged. Can run beside g6.
2. **I1473g8b2 MUI RTG flow draw** (after g8b1 and g7, because g7 also edits `mui_canvas_class.c`): `mui_flow_layout`,
   `mui_canvas_draw_flow`, `scrim` in `mui_grid_style`, class `flow` flag + `mui_canvas_set_flow` + glide state, centred caption branch,
   front shadow. Test `mui-flow` (new goldens).
3. **I1473g8b3 MUI AGA flow draw** (after g8b2): strip clip, `mui_cover_dim_lut`, sink `chunky` callback, flow index cache, OS < v40 tint.
   Test `mui-flow-aga` (new golden); `mui-grid-aga` golden unchanged.
4. **I1473g8b4 MUI Cover Flow wiring and input** (after g8 and g8b3): `mui_refresh` COVERFLOW -> canvas in flow mode (leaves the packed
   path); `mui_style_mask` allows Cover Flow on planar and both `mui_style_mask(0)` call sites (mui_ui.c:560 menu, :5465
   `SEL_UI_VIEW_STYLES` tag) pass the real mask (today they pass 0, so the engine never offers Cover Flow on MUI, RTG included);
   g8 chooser shows Cover Flow on planar; flow loader edge and slots per D-F1; `mui_grid_key/click` COVERFLOW cases; glide tier
   `mui_flow_glides` + `wait_cap_ms` 40 while gliding; exit-stat sentinels. Tests `mui-grid-switch`, `mui-grid-input` extended, existing
   mask tests updated.
5. **I1473g8b5 Mac wheel / side click** (Mac slot, only if D-F3 or D-F4 picks the change): the same rule on the Mac so MUI stays 1:1;
   `dui-grid-coverflow` gains the wheel / side-click case.
6. g9 rig check gains the Cover Flow steps above (no new row).

## 8. Johan decisions

- **D-F1 Cover sharpness of the front cover.** (a) One flow art loader at 144 px (the shared Mac thumbnail size, g3 path, write-back
  kept): side covers are always sharp, the front cover is upscaled above 144 px on big RTG screens (Recommended: no JPEG decode on the UI
  task, ~1 MB RTG / 249 KB AGA, reuses g3 as is; trade-off: the front cover is soft at 1024 x 768 where it reaches ~300 px); (b) as (a)
  plus the front cover decoded from the full cover blob at its size, one extra slot (`mui_cover_album_image`, 512 KB cap): sharp like the
  Mac, but each new front cover costs 1-5 s of decode on a 68020 (idle-only, it shows the 144 version first); (c) cap the front cover at
  144 px: always sharp, but the stack is smaller than the Mac's on big screens (breaks 1:1 layout).
- **D-F2 Glide animation by CPU.** (a) Glide (220 ms ease-out, as the Mac) on 68040/68060, one step per key on 68020/68030
  (Recommended: the 020 cannot draw more than 1-2 frames in 220 ms, a step looks cleaner than a stutter); (b) glide everywhere, frames
  dropped on slow CPUs; (c) never glide on MUI.
- **D-F3 Click on a side cover.** (a) Side covers move the flow to that cover (`VIEW_CURSOR <index>`), and the Mac gets the same rule in
  row g8b5 (Recommended: what every Cover Flow does, one small Mac row keeps 1:1); (b) as the Mac today: only the front cover reacts.
- **D-F4 Mouse wheel in Cover Flow.** (a) One notch moves one cover on MUI and the Mac (row g8b5) (Recommended: the Mac wheel does nothing
  useful in Cover Flow today, read from code, unverified live); (b) wheel does nothing, as the Mac today.
- **D-F5 Cover Flow on AGA at all.** D6 already says AGA, but the cost table shows ~0.5 s a frame at 640 x 512 on a 14 MHz 68020.
  (a) Offer it on every AGA screen, step-only on 020 (Recommended: D6 as answered; one redraw per key is usable); (b) offer it on AGA only
  with a 68040+ (style masked on 020 AGA); (c) RTG only (the old mask).
