# I286 Amiga readiness: DUI-P6-3 done, DUI-P6-4 open

## DUI-P6-3 (done, commit e190c14)
- `tests/dui/m68k/CMakeLists.txt`: stand-alone project that adds `src/gui` and `src/uimodel` with an INTERFACE `selecta_core` that only provides headers. It needs no vendored players, so it configures in any checkout.
- `tests/dui/m68k_nofloat.cmake`: configures it with `cmake/amiga-68020.cmake`, builds both libraries under the repo warning flags (-Werror), runs `m68k-amigaos-nm` on both archives. Instrument check: `sel_gui_canvas_init` and `sel_uim_init` must be defined, else it fails. Fails on any undefined soft-float helper (`__mulsf3`, `__adddf3`, `__divdf3`, `__fixdfsi`, `__floatsidf`, the whole sf/df arithmetic, compare, convert and powi families) or libm call (`sqrt`, `floor`, `ceil`, `pow`, `fabs`, `fmod`).
- Result: the libraries were already float-free; nothing to fix. A deliberate float probe compiled with the same toolchain shows `___mulsf3`, `___divdf3`, `___floatsidf` in the same nm format, so the match pattern (`U _?name`) is live.
- Found: both archives reference `___divdi3` and `___udivdi3` (64-bit integer division from libgcc, a software routine on 68020). Not floating point, so not failed, but slow per call on a 68020; find the callers before M4 pixel-work budgets (DUI-P6-5).
- Registered in `SEL_DUI_REAL`, removed from `tests/ratchet/unimplemented.txt`, ticked in the plan. `ctest -R "dui-m68k-nofloat|dui-ratchet"` passes (ratchet count equals plan ids).
- Worktree note: `third_party/nuade` and `third_party/libopenmpt` are empty in agent worktrees, so the full tree does not configure there; the stand-alone project is why this test still works.

## DUI-P6-4 (open, not started in code)
Current state measured from the source:
- `desk_make_canvas` (plugins/ui/desktop/desktop_ui.c ~5113) calls `sel_gui_palette_default` and sets all 256 entries as one flat palette (3 theme entries, 24 greys, 6x6x6 cube, accent steps). No budget regions, no quantiser, no per-region counting. `gui_draw.h` has one 32 KB nearest-colour cube for all 256 entries.
- Only one INDEX8 golden exists (`tests/golden/desktop/testcard-index8-640x400@1.hash`); the plan wants 11 (one per view, 640x512@1 Selecta Dark). `--format index8` already runs through `--view <page>`; `dui-governor` and `dui-dirty` use it.
- No `palette_colours` stat exists (grep of src, include, plugins).
Needed work, in order:
1. gui_draw: budget layout (entries 0-31 UI, 32-63 generated art, 64-127 / 128-191 / 192-255 the three artwork sub-palettes: tile row, panel, cover flow). UI and generated-art draws use a cube over entries 0-63 only; artwork blits use a cube per region sub-palette (3 x 32 KB, rebuilt only when the sub-palette changes).
2. Integer median-cut quantiser on the 64-pixel thumbnail into 64 slots (new, in `src/gui/image/`), fed by `sel_gui_art_cache` thumbnails; the dominant tint takes one UI slot (plan 2.7).
3. Choose the 32 UI colours from the theme tokens (sel_gui token table) and the 32 generated-art colours from `gui_genart.c` output; both are palette design decisions that change every INDEX8 hash, including the existing testcard-index8 golden.
4. Stat `palette_colours_ui`, `_art`, `_tile`, `_panel`, `_flow` = distinct indices used per region in the final canvas; add to the `ui_stats` list in desktop_ui.c next to `canvas_hash`.
5. Test `dui-aga-budget` (script tests/dui/aga_budget.cmake, pattern of golden_home.cmake plus `dui_run`): 11 views at `--canvas 640x512 --format index8`, hash against `tests/golden/desktop/<view>-index8-640x512@1.hash`, region counts within 32 / 32 / 64 x 3; sentinel: quantiser call count rises on a view with artwork; then add to `SEL_DUI_REAL`, remove DUI-P6-4 from `tests/ratchet/unimplemented.txt`, tick the plan.
Estimate: one agent, 40+ tool calls (touches gui_draw, a new quantiser, desktop_ui draw paths, 12 goldens).
