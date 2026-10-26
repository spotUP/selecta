---
date: 2026-10-24
author: spotUP
status: draft, waiting for review (section 7 questions)
milestone: M3d Desktop UI (proposed name, between M3c and M4; D-M4-5)
implements: design doc sections 6, 7, 9a, 9b, 9c, D5, D8; decisions D-M4-5 and D-M4-6 (todos.md:508-509)
related: thoughts/shared/plans/2026-10-24-m4-amiga-ui.md, thoughts/shared/plans/2026-10-22-architecture-design.md, thoughts/shared/research/2026-10-22_features-modern-streaming.md
---

# Desktop UI (M3d): shared widget set in C, SDL backend first

## 1. Goal and finish line

Goal: Selecta gets a desktop GUI on macOS (Linux and Windows build from the same code) with the look of a modern streaming app, Spotify as the reference (design 9a): artwork-first, dark surfaces, sidebar plus content plus Now Playing panel, one accent colour. The GUI is built from ONE widget set, layout engine and design-token layer in portable C99 (D-M4-6). This milestone ships it with two backends: `sdl` (the window) and `mem` (headless software canvas, used for golden renders and later as the reference for the Amiga backends). The RTG and AGA backends are M4 work on top of the same code; this milestone makes sure they can be written without touching the widget set (section 2.4, Phase 6).

Finish line, measurable. M3d is done when all of these hold:

1. Every ID in section 5 (73 items, DUI-P0-1 to DUI-P7-2) is ticked with its test green. Each test drives the shell binary `selecta --ui desktop`, proves the new code ran with a sentinel, and asserts the stated number.
2. The ratchet `tests/ratchet/unimplemented.txt` holds zero DUI IDs.
3. Golden renders: every view in section 3 has a committed hash at 1280x800 scale 1 and 2560x1600 scale 2, dark and light theme (section 4.2). All match.
4. Performance, host (Apple Silicon, release build):
   - idle with nothing playing: 0 redraws per second, under 1 percent CPU;
   - playing with the progress bar visible: at most 4 redraws per second, under 3 percent CPU;
   - input-to-present at most 16 ms at 2560x1600;
   - 10 000-row library scroll at 60 fps.
5. Zero heap allocations after `open` across a 10 000-frame scripted run (DUI-P6-1).
6. 68020 readiness: `src/gui/` and `src/uimodel/` cross-build for `m68k-amigaos -m68020` with no floating-point symbols (DUI-P6-3). The `mem` backend renders the golden set in its 8-bit indexed mode within the AGA colour budget (DUI-P6-4).
7. Johan has run the manual check DUI-P7-1 on a Retina screen and an external non-Retina screen and said yes.

Commands (from RULES.md `## Commands`):

```
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSELECTA_UI_DESKTOP=ON && cmake --build build/host
nice -n 19 ctest --test-dir build/host -L dui -j2 --output-on-failure
nice -n 19 ctest --test-dir build/host -R ratchet --output-on-failure
cmake --build build/host --target lint
```

## 2. Architecture

### 2.1 Where code lives

| Path | Target | Contents | Used by |
|---|---|---|---|
| `src/uimodel/` | `selecta_uimodel` | frame strings, dirty bits, action table, layout tree, governor, lifted text-UI models (M4 plan section 2, unchanged design, moved here) | text UI, desktop UI, all Amiga UIs |
| `src/gui/draw/` | `selecta_gui` | canvas, rasterizer, clipping, dirty rectangles | desktop, custom Amiga UI |
| `src/gui/text/` | `selecta_gui` | glyph atlas format, atlas builder, text layout, ellipsis, UTF-8 | same |
| `src/gui/image/` | `selecta_gui` | decode, scale, artwork cache, generated artwork | same |
| `src/gui/theme/` | `selecta_gui` | `sel_gui_tokens`, token file parser, presets | same |
| `src/gui/widgets/` | `selecta_gui` | widget set (2.3) | same |
| `src/gui/views/` | `selecta_gui` | every view of section 3, built from widgets and the layout tree | same |
| `src/gui/backend.h`, `src/gui/backend_mem.c` | `selecta_gui` | backend interface, headless backend | tests, desktop `--backend mem`, Amiga host tests |
| `plugins/ui/desktop/` | plugin kind 5 `desktop` | descriptor (pattern of `text_ui.c`), `backend_sdl.c`, `input_sdl.c`, `os_<os>.c` (clipboard, open URL, file dialogs) | shell |
| `plugins/ui/custom/` (M4) | plugin `custom` | `amiga/backend_rtg.c`, `amiga/backend_aga.c`, `amiga/input.c` only | Amiga shell |

Same look by construction: views never call a backend; they draw into a `sel_gui_canvas` through `draw/`. The backend only presents dirty rectangles, sets palettes and reports input. Reuse: `src/uimodel/` is the M4 plan section 2 design taken as is (2.1 to 2.3 there); this plan does not redesign it, it builds it first.

### 2.2 Tokens

`sel_gui_tokens` extends the lifted `txt_theme` (M4 plan 2.2, `theme.h` X-macro `TXT_THEME_ROLES`) with one X-macro table `SEL_GUI_TOKENS`. Every token has a name, type, default and range:

- Colour roles: background, surface 1 to 3, surface hover, surface pressed, text, text muted, text disabled, accent, accent text, divider, focus ring, error, warning, success, scrim.
- Sizes on an 8-unit grid (4-unit half step): spacing 1 to 8, radius small/medium/large, row height compact/regular, sidebar width, Now Playing panel width, transport height, artwork sizes (tile, panel, cover flow), font sizes caption/body/title/headline/display.
- Motion: durations (fast, normal, slow) in ms and an easing id; `reduce motion` switch.

The token file uses the existing `txt_theme_parse` line syntax (`role = value`), so one parser serves the text UI, the desktop UI and the Amiga UIs. Presets: Selecta Dark (default), Selecta Light, plus the four Amiga presets of M4 8a Q21, which load and render on desktop too. Colours are stored as 8-bit RGBA. Sizes are stored in logical units; the backend scale factor is applied once at layout time, in integers.

Two token layers (D-DUI-9, Johan 2026-10-24, after the Spotify Encore model). Layer 1 holds base values: a colour ramp (`gui.base.grey_0` .. `grey_10`, `gui.base.accent_0` .. `accent_4`, error, warning and success steps), the spacing steps, the radius steps and the type sizes. Layer 2 holds the role tokens listed above (`gui.surface`, `gui.accent`, `gui.spacing_3`, and so on); each role either references a base token (`gui.surface = @gui.base.grey_1`) or keeps a literal value. Widgets read role tokens only and never read base tokens. A theme, a preset or an Amiga palette normally swaps layer 1 only, and the roles follow it. The reference is resolved once at parse time (no lookup per frame), and a missing or circular reference makes the parse fail with the token name. The same table serves desktop, RTG and AGA, so the Amiga UI matches the desktop look through shared tokens and the shared renderer, not through a copy.

### 2.3 Widget set

Retained tree of fixed-size widget records in an arena allocated at `open`; no allocation per frame. Each widget has: id, kind, rect (Q16 not needed: integer logical pixels), state bits (hover, pressed, focus, selected, disabled), action id (from the action table, which supplies label, tooltip text, icon and binding), and a dirty bit.

Widgets: label, icon, icon button, text button, toggle, slider (seek, volume, with drag and keyboard step), progress, virtual list (only visible rows exist; rows come from `sel_view_window`), table header with column sort and resize, grid of tiles, cover flow strip, tabs, sidebar list, split pane with drag handle, scroll bar (overlay style), text field (search, command line, rename) with caret and selection, menu and context menu, tooltip, toast, modal dialog, scrim, artwork image, viz surface, segmented control, checkbox, radio, dropdown (settings pages).

Icons: one icon atlas generated at build time from SVG sources of a flat open icon set (Lucide, ISC licence) rasterised to alpha masks at each size token, so icons tint with colour tokens and need no runtime vector code. No emoji, no text glyph icons.

### 2.4 Backend interface

```
typedef struct sel_gui_backend_ops {
    int  (*open)(void *b, const sel_gui_open_req *req, sel_gui_caps *caps);
    void (*close)(void *b);
    sel_gui_canvas *(*lock)(void *b);              /* canvas the views draw into */
    void (*unlock_present)(void *b, const sel_gui_rect *dirty, int n);
    void (*set_palette)(void *b, const uint32_t *rgb, int first, int n); /* indexed canvases only */
    void (*set_regions)(void *b, const sel_gui_palette_region *r, int n); /* AGA copper splits, no-op elsewhere */
    void (*set_cursor)(void *b, int cursor_id);
    int  (*poll_input)(void *b, sel_gui_input *ev, int max);
    int  (*wait)(void *b, int timeout_ms);         /* maps to the UI plugin wait hook, 0/1/-1 */
} sel_gui_backend_ops;
```

`sel_gui_caps` reports canvas format (`RGBA32` or `INDEX8`), pixel scale (1, 2, 3), maximum colours, whether alpha blending is cheap, and whether a pointer, touchpad gestures and a keyboard exist. The views read caps once at open and pick effects through the governor (`sel_uim_budget`, M4 2.3): blur, shadows and gradients are off on `INDEX8`.

Backends in this milestone: `mem` (RGBA32 and INDEX8, both headless) and `sdl` (RGBA32 in a streaming texture; `unlock_present` uploads only the dirty rectangles with `SDL_UpdateTexture` per rectangle, then renders and presents). M4 adds `rtg` (RGBA32 or 16-bit through P96/CGX) and `aga` (INDEX8 plus copper regions). Phase 6 tests that INDEX8 through `mem` produces the AGA-bound output, so M4 only writes the hardware side.

### 2.5 Rasterizer (`src/gui/draw/`)

- Integer and Q16.16 fixed point only; no `float` or `double` anywhere under `src/gui/` (enforced by DUI-P6-3 on the m68k build and by a lint grep on host).
- Primitives: fill rect, rounded rect with coverage anti-aliasing on RGBA32 (4x4 coverage table, no float), horizontal and vertical gradient (Q16 step), 1-pixel lines, alpha mask blit with tint (glyphs, icons), image blit with nearest or box downscale, scrim (alpha fill), drop shadow as a pre-baked 9-slice from the token radius (generated at open, not per frame).
- INDEX8 paths: ordered 4x4 dither for gradients, nearest palette entry through a 32x32x32 lookup cube built at palette set (32 KB, once), no anti-aliasing.
- Clip stack of fixed depth 16. Dirty rectangle list of fixed size 64; when full, rectangles merge by bounding box. A redraw walks only widgets intersecting the dirty list.

### 2.6 Text

- One OFL sans family (Inter, regular and semibold) bundled under `third_party/fonts/` with its licence. One mono face is not needed in the UI (section 2a of the global design direction leaves monospace labels out; the pattern view uses the tracker font of the text UI grid model, rendered as a bitmap font).
- One atlas builder `src/gui/text/atlas_build.c` built on `stb_truetype` (vendored single header, public domain). It runs at build time for the Amiga (atlases for scale 1, 1-bit and 4-bit alpha, M4 8a Q13) and at UI open on desktop for the actual scale factor and the user's chosen size step. One code path, one atlas format (`sel_gui_atlas`: glyph metrics table plus 8-bit alpha pages). No rasterising per frame.
- Text layout: UTF-8 decode, kerning pairs from the font, ellipsis at end or middle (paths), line wrap for lyrics and song info. Glyph coverage: Latin-1, Latin Extended-A, plus the characters found in the HVSC and Modland title sets (measured at implement time from the catalogue, DUI-P2-3 records the count). Missing glyphs draw a box glyph and count in `STAT_MISSING_GLYPHS`.
- Shaping (HarfBuzz) is not in this milestone: titles in the catalogues are Latin and Japanese in a few rows; CJK fallback is listed under not in this milestone.

### 2.7 Image and artwork pipeline

- Sources, in order: library `artwork` table (`src/library/db/schema.c`: `artwork(album_id, kind, data BLOB)`), folder images next to files (`cover.*`, `folder.*`), embedded tags where the decoder exposes them, and generated artwork (M4 8a Q12) when none exists.
- Decode with `stb_image` (vendored, PNG and JPEG only). Decode happens on a worker job of the engine thread pool, never in the draw path. Scaled results (box filter in integers) go into an artwork cache: a fixed pool of slots per size token (tile, panel, cover flow, background tint), allocated at open, LRU by slot index.
- Thumbnails are written back to the library as `kind = thumb<size>` so a second run and the Amiga never decode a full JPEG (decoding a 1000x1000 JPEG on a 68020 is seconds).
- Generated artwork: deterministic from format, chip and title hash (gradient plus format glyph), drawn by `draw/`; same output on every backend.
- Dominant colour per artwork (integer histogram over the 64-pixel thumbnail) feeds the Now Playing header tint token, as in Spotify. On INDEX8 the tint takes one palette slot.

### 2.8 Input

`sel_gui_input` events: key (key code, modifiers, text), pointer move, button down and up with click count, wheel with precise deltas (Q16), pinch (scale delta Q16), swipe (direction), drop (file list or URL), resize, scale change, focus in and out, quit.

- Keyboard: every action reachable by key through the lifted bindings (`txt_bindings_*`, M4 2.2), focus ring with Tab order, arrow navigation in lists and grids, type-to-search.
- Mouse: hit testing through `sel_uim_hit` on the layout tree, hover, click, double-click plays, drag and drop to the queue and playlists, column sort, context menu on secondary click.
- Touchpad: two-finger precise scrolling with momentum handled by the OS (SDL delivers the deltas), pinch in grid view changes the tile size token step, two-finger horizontal swipe is Back and Forward in the view history, swipe in cover flow moves the strip.
- Media keys (play, pause, next, previous) through SDL key codes.
- Scripted input for tests: `--input <file>` reads one event per line (`key ctrl+f`, `click 640 120`, `wheel 0 -3`, `pinch 1.25`, `drop /path/a.mod`, `resize 900 600`, `wait 200`) and feeds the same queue the backend fills. Lives in the shell next to `--keys` (apps/selecta/main.c:222) so every UI can use it.

### 2.9 Animation budget

- One timeline per UI with at most 16 running tweens (fixed array). Values in Q16, time in ms from the shell clock, easing tables of 64 entries.
- Animations: hover and press fades (fast), panel slide and view cross-fade (normal), cover flow glide (normal), toast in and out, progress bar (no tween, follows the clock).
- A frame is drawn only when a dirty bit is set or a tween runs. With no tween running the UI blocks in `wait` until the next engine event or the next second boundary of the visible clock.
- The governor (`sel_uim_budget`) turns animations off on INDEX8, on 68020 tiers and when the pref `ui.desktop.reduce_motion` is set (default follows the OS setting where SDL reports it, otherwise off).

### 2.10 HiDPI

Layout in logical pixels; the backend reports the pixel scale (SDL window pixel density). Scale is applied as an integer multiplier for 1x, 2x and 3x; fractional scales (1.25, 1.5 on Windows and Linux) round the layout scale up and let SDL scale the texture down. Atlases and icons are built for the integer scale at open and rebuilt on a scale-change event (moving the window between a Retina and a non-Retina screen).

### 2.11 68020 and AGA constraints honoured from day one

- No `float`/`double`, no VLAs, no `alloca` (design section 12) under `src/gui/` and `src/uimodel/`.
- All per-UI memory from one arena sized at open from the canvas size and the token sizes; the size is printed as `ui_arena_bytes=` and is checked against a budget in DUI-P6-1.
- Dirty-rectangle redraw everywhere; a clock tick redraws only the time label and the progress bar.
- INDEX8 colour budget (AGA 256 colours): 32 fixed UI colours from the theme, 32 for the generated artwork set, 3 x 64 for artwork sub-palettes (tile row, panel, cover flow) placed by copper regions in M4. The `mem` INDEX8 mode quantises artwork to its 64-slot sub-palette (median cut on the thumbnail, integers) and asserts the counts.

## 3. Views

All views are in `src/gui/views/`, read only `sel_state` (engine.h:136) through `sel_uim_frame` (M4 2.3) and send commands only through `sel_engine_send_line()` (engine.h:246) or the action table. Lists come from the engine view model: `sel_view_state` engine.h:87, rows `sel_view_row` engine.h:73, windowed with `sel_view_window`, styles with `sel_view_set_style`.

| View | Reads | Sends | Notes |
|---|---|---|---|
| Frame (sidebar, content, Now Playing panel, transport bar) | `song`, `queue`, `stats`, prefs `ui.desktop.*` | transport commands, `UI_SWITCH`, view navigation | three columns at width 1280 and up; panel collapses under 1100; sidebar collapses to icons under 900; mini mode is a separate layout (DUI-P4-15) |
| Home | view source `HOME` rows (recently played, jump back in, new this week, favourites), artwork ids | open, play, queue next | shelves of tiles, horizontal scroll per shelf |
| Library and browse | sources `BROWSE`, `FOLDERS`, library sources, `sel_view_drill`, `sel_view_expand`, columns (F48) | sort, drill, expand, play, queue, add to playlist | styles regular, tree, drilldown, column browser (9a), plus grid and cover flow (pulled forward, below) |
| Search | source `QUERY`, query language (1a:45), facets from the library | `SEARCH`, scope pref | live results grouped by type: songs, composers, albums, playlists |
| Now Playing | `song`, `panel` label/value pairs, `lyric_text`/`lyric_line`/`lyric_current`, rating, artwork, fade state `sel_engine_fade()` | rate, like, show composer, open song info | large artwork, synced lyrics, up next, header tint from dominant colour |
| Queue | source `QUEUE`, history | play next, reorder (drag), remove, undo/redo (F49), reshuffle and scope (F30) | "Now playing", "Next up", "Later" groups as in Spotify |
| Playlists | `PLAYLIST` and `FOLDER` rows, smart playlists (saved queries) | create, rename, delete with confirm, folder move, drag reorder | folder tree in the sidebar, detail view with header artwork collage |
| Song info | `panel` (module info, STIL, composer, release), `sel_engine_tracker_info()`, wrong and skip flags, notes | edit note, set flags, lookup (DUI-P5-7) | full page, also as a side sheet |
| Visualizer area | `viz` frames (`sel_viz_frame`, include/selecta/viz.h:23), `sel_surface` (added to viz.h), pattern view (`sel_engine_pattern_request()`, `sel_pattern_view`), phase view (F17), chip register panel (1a:53), signal path (F38) | open viz plugin, pick viz, fullscreen | panel inside Now Playing, own tab, fullscreen visual mode F38; tracker tabs for pattern, samples, instruments |
| Settings | prefs (`include/selecta/prefs.def`, `PREF_GET`, `PREF_SET`, `PREFS_CHANGED`), COMMANDS reflection for key bindings | `sel_prefs_set` | pages generated from `prefs.def` groups, theme picker with live preview, key binding editor |
| Overlays | action table, bindings | any action | command palette (F49), help overlay listing every key by area, command line (F45), context menus, toasts, dialogs |

Grid and cover flow: pulled forward from M5 into this milestone (DUI-P4-4 engine styles, DUI-P4-5 views). Reason: they are the artwork-first presentation that defines the Spotify look, and the M4 custom Amiga UI needs them already (M4-P3-1). Cover flow here is 2D: perspective-free stacked tiles with scale and offset, drawn by `draw/`, which also runs on AGA. What stays in M5: the GPU 3D cover flow, GPU visualizers (projectM class, design 1a:54 GPU part) and the bitmap skin loader (D8, 1a:92).

## 4. Testing

### 4.1 Shape

- Every test is a ctest entry with label `dui`. It runs the shell binary: `selecta --ui desktop --backend mem --size WxH --scale S [--theme name] [--input file] [--exec cmds] --frame-dump out.png --stats`.
- `--stats` prints the UI stats as `key=value` lines at exit: the existing `ui_frames=`, `ui_hash=` (UI_STAT_FRAMES 0x5017, UI_STAT_HASH 0x5018) plus new stats added after 0x501C in `include/selecta/tags.def`: `STAT_DIRTY_PIXELS`, `STAT_HEAP_ALLOCS_AFTER_OPEN`, `STAT_WIDGET_DRAWS`, `STAT_VIEW_OPENS`, `STAT_TWEENS`, `STAT_ART_DECODES`, `STAT_ART_CACHE_HITS`, `STAT_MISSING_GLYPHS`, `STAT_PALETTE_COLOURS`, `STAT_GESTURES`, `STAT_DROPS`. Names are final at implement time; each one is a sentinel for one item below.
- The `sdl` backend is tested with `SDL_VIDEODRIVER=offscreen` (SDL3 has it; dummy driver as fallback) so CI needs no display.
- Test media: the fixture songs already used by the text UI tests under `tests/` plus a small fixture library database with artwork rows (DUI-P0-1 creates it).

### 4.2 Golden renders

- `tests/golden/desktop/<view>-<theme>-<w>x<h>@<scale>.hash`: 64-bit hash of the canvas (same hash function as `ui_hash`). The PNG is written to the build directory for review, never committed.
- Set: 11 views of section 3 x 2 themes x 2 sizes (1280x800@1, 2560x1600@2) = 44 hashes, plus INDEX8 hashes of the same 11 views at 640x512@1 Selecta Dark (11), plus mini mode (2) = 57; the compact layout (DUI-P4-20) adds Home, Library, Now Playing and Queue at 640x400 and 680x270 (8) = 65.
- A golden is updated only with `-DSELECTA_GOLDEN_UPDATE=ON` and a commit message that names the view and why; the review step opens the PNGs.
- Clock and animation are frozen in golden runs (`--clock fixed:<ms>`), so hashes are deterministic.

### 4.3 Reachability: one test per feature

Per the global rule, each DUI item has one test through `selecta --ui desktop` with a sentinel. Behaviour that lives in a pure function (fixed-point maths, text layout, token parsing, quantiser) is tested where it lives with small unit tests, not through the shell, and does not need its own reachability test. Knob sweep (REACHABILITY_PROTOCOL section 2): DUI-P6-6 sweeps every token and every `ui.desktop.*` pref over its range and asserts the golden hash changes; exemptions are written in the token table with a reason. The sweep is first run against one token known to work (accent) and one known dead (a token deliberately not read) to check the instrument (protocol section 3).

## 5. Phased checklist

IDs `DUI-P<phase>-<item>`. 73 items. Every test has label `dui`; "Test:" names the ctest entry and what it asserts.

### Phase 0: red tests and dependencies
- [ ] DUI-P0-1 Every test named in this checklist exists and fails red with a missing-feature reason; fixture library with artwork rows committed; all 73 IDs added to `tests/ratchet/unimplemented.txt`. Test: `dui-ratchet`: `ctest -L dui -N` count equals the ID count parsed from this plan by `tests/ratchet`; each `dui-*` test fails before its item lands.
- [ ] DUI-P0-2 SDL dependency: `find_package(SDL3 3.2)` behind `-DSELECTA_UI_DESKTOP=ON`, off by default on the Amiga toolchain; build prints the SDL version; vendored `stb_image.h` and `stb_truetype.h` under `third_party/stb/` with licences. Test: `dui-build-deps`: configure with the option prints `SDL3 3.` and builds `plugins/ui/desktop`; configure without SDL still builds everything else.

### Phase 1: shell and uimodel (moved from M4 Phases 1 and 2)
- [ ] DUI-P1-1 `UI_SWITCH <id>` handled by the shell (from M4-P1-1). Test: `dui-ui-switch`: `--ui desktop --exec "UI_SWITCH text"`; asserts the desktop close hook ran once, the text UI `STAT_FRAMES` > 0, engine song position unchanged.
- [ ] DUI-P1-2 Several UIs at once (from M4-P1-2, host part). Test: `dui-ui-multi`: `--ui desktop --ui text --exec NEXT`; both `STAT_FRAMES` > 0 and both show the new title.
- [ ] DUI-P1-3 Display-mode pref `ui.desktop.mode` (full, window, mini) in `prefs.def` and CLI `MODE=` through the one parser (from M4-P1-3, host part). Test: `dui-mode-pref`: `MODE=mini`; `PREF_GET ui.desktop.mode` prints `mini` and the frame layout id is mini.
- [ ] DUI-P1-4 `src/uimodel/` with `sel_uim_update`, frame strings, dirty bits, progress in Q16 (from M4-P2-1). Test: `dui-uim-frame`: one `NEXT` sets only the song and queue dirty bits (`STAT_WIDGET_DRAWS` delta limited to those widgets).
- [ ] DUI-P1-5 Lift bindings, palette, cmdline, theme, display strings and panel models out of `plugins/ui/text/` (from M4-P2-2). Test: `dui-lift-parity`: the text UI golden hashes (existing `--screen-dump` tests) are unchanged and the desktop UI shows the same clock and stage strings (`ui_hash` of the status widget equals a fixture).
- [ ] DUI-P1-6 Action table: labels in full English words, tooltips, icon ids, menu paths, default bindings (from M4-P2-3). Test: `dui-actions`: every COMMANDS entry has an action row or an exemption; the help overlay lists N rows equal to the table count.
- [ ] DUI-P1-7 Layout tree and breakpoints (from M4-P2-4). Test: `dui-layout`: `resize` events to 1400, 1050, 850 and 500 wide give layout ids three-column, two-column, icon-sidebar, mini-suggestion; no widget rect outside the canvas.
- [ ] DUI-P1-8 Parity matrix of HippoPlayer and bbmp UI features per UI (from M4-P2-5), desktop column filled. Test: `dui-parity-matrix`: the matrix file parses; every row has a DUI ID or an exemption that matches section 8 of this plan.
- [ ] DUI-P1-9 Effects governor `sel_uim_budget` (from M4-P2-6). Test: `dui-governor`: with `--backend mem --format index8` the tween count stays 0 and no blur or shadow primitive runs (`STAT_TWEENS=0`).
- [ ] DUI-P1-10 Token layer `sel_gui_tokens` (X-macro, defaults, ranges) extending `txt_theme`, token files through `txt_theme_parse`. Test: `dui-tokens`: `--theme tests/golden/desktop/accent-red.theme` changes the play button pixels to the token value and nothing else (dirty set of one token).
- [ ] DUI-P1-11 Breakpoints equal the decided numbers. D-DUI-2 and section 3 say three columns from 1100 logical px, icon sidebar under 900; `sel_uim_breakpoint_for` (src/uimodel/layout.c:21-33) uses 1200, 900 and 700 (the 700 edge drops to the mini strip, see DUI-P4-20). The code changes to the plan (the plan is the recorded decision); the widths 1100 / 900 become tokens, not literals. Test: `dui-breakpoints`: `resize` to 1100, 1099, 900, 899 gives three-column, two-column, two-column, icon-sidebar; the same table is read by the layout test DUI-P1-7.

### Phase 2: drawing, text, images, backends
- [ ] DUI-P2-1 Canvas RGBA32 and INDEX8, rasterizer primitives of 2.5 in integers and Q16.16. Test: `dui-draw`: golden of a primitive test card through `--ui desktop --view testcard`; host lint finds no `float`/`double` under `src/gui/`.
- [ ] DUI-P2-2 Dirty-rectangle list and partial redraw. Test: `dui-dirty`: one clock second redraws at most 4 000 pixels at 1280x800@1 (`STAT_DIRTY_PIXELS`).
- [ ] DUI-P2-3 Atlas builder on `stb_truetype`, Inter bundled, text layout with ellipsis and wrap; glyph coverage measured from the catalogue. Test: `dui-text`: the fixture library with long and non-ASCII titles renders with `STAT_MISSING_GLYPHS=0` and the golden matches.
- [ ] DUI-P2-4 Image decode (`stb_image`) on a worker, box scaling, artwork cache, thumbnails written back to the library. Test: `dui-artwork`: two runs over the fixture; first run `STAT_ART_DECODES` = number of albums, second run 0 decodes and `thumb` rows exist.
- [ ] DUI-P2-5 Generated artwork per format and chip for songs without a cover (from M4-P3-5). Test: `dui-genart`: a song with no artwork shows a tile whose hash equals the generated-art fixture for its format; two formats give different hashes.
- [ ] DUI-P2-6 `mem` backend, `--frame-dump`, `--stats`, `--input`, `--clock fixed`, golden harness (from M4-P4-1, portable part). Test: `dui-golden-home`: home view hash matches; `--input` click on a tile changes `STAT_VIEW_OPENS` by 1.
- [ ] DUI-P2-7 `sdl` backend: window, streaming texture, dirty upload, resize relayout, scale change, offscreen driver in tests. Test: `dui-sdl`: `SDL_VIDEODRIVER=offscreen selecta --ui desktop --backend sdl --input resize.txt`; the presented frame hash equals the `mem` hash for the same script.
- [ ] DUI-P2-8 HiDPI: integer scales 1, 2, 3, atlas and icon rebuild on scale change. Test: `dui-hidpi`: a scripted scale change 1 to 2 rebuilds atlases once and the 2x golden matches.

### Phase 3: widgets and input
- [ ] DUI-P3-1 Widget set of 2.3 and the build-time icon atlas. Test: `dui-widgets`: the widget gallery view golden matches in both themes; every widget kind in the enum is drawn once (`STAT_WIDGET_DRAWS` per kind > 0).
- [ ] DUI-P3-2 Mouse: hit test, hover, click, double-click plays, drag and drop to queue and playlist, column sort, context menu (from M4-P4-4). Test: `dui-mouse`: scripted double-click on a row starts that song (`song` path equals the row), drag to queue adds 1 entry, header click sorts descending.
- [ ] DUI-P3-3 Keyboard parity, focus ring, Tab order, type-to-search (from M4-P4-5). Test: `dui-keys`: every action of the action table is invoked once by its binding through `--input`; the count of fired actions equals the table count.
- [ ] DUI-P3-4 Touchpad: precise scroll, pinch tile size, swipe back and forward. Test: `dui-gestures`: `pinch 1.5` in grid view moves the tile size one step; `swipe right` returns to the previous view; `STAT_GESTURES=2`.
- [x] DUI-P3-5 Animation timeline, easing tables, reduce-motion pref, idle blocking. Test: `dui-anim`: with no input and no song the run of 10 s draws 1 frame; with `ui.desktop.reduce_motion=1` a view change draws 1 frame instead of the tween frames.
- [x] DUI-P3-6 Tooltips from the action label (from M4-P12-4). Test: `dui-tooltips`: hover of 600 ms over each transport button shows a tooltip whose text equals the action label.
- [ ] DUI-P3-7 Hover feedback in the live loop (I341). Rule: a pointer event stamps the UI clock at the time the event arrives, not before the wait that preceded it; a tween that ended between two turns still draws its end state once. Test: `dui-hover-live`: through `selecta --ui desktop --backend sdl` with the offscreen driver and a poll that blocks 1000 ms before delivering `move`, the hover fade starts at the event time (`STAT_HOVER_START_MS` equals the event stamp) and the last frame carries the full hover fill (pixel under the control equals the surface-hover token); sentinel `STAT_HOVER_END_FRAMES` is 1.
- [x] DUI-P3-8 Hover repaint covers every region that holds a control. `hover_repaint` (plugins/ui/desktop/desktop_ui.c:4436-4446) repaints only `sidebar` and `main`; the regions of the layout are `sidebar`, `main`, `detail` (Now Playing panel), `player` (transport footer) and the top bar with the navigation arrows. Hover and press fades repaint the region that holds the control, for all five. Test: `dui-hover-regions`: for one control in each region (sidebar entry, tile, transport button, Now Playing like button, navigation arrow) a `move` onto it changes the pixels inside that control after the fade and `STAT_DIRTY_PIXELS` stays inside that region rect.
- [ ] DUI-P3-9 Wheel scroll frame rate on every scrolling surface (I310, I314). One wheel notch is 16 logical px (`SCROLL_NOTCH`, desktop_ui.c:822) and precise touchpad deltas are Q16 fractions of it. A wheel event repaints only the scrolled region (the list, the shelf, or the Home content), never the full canvas, and presents within 16 ms at 1280x800 and 2560x1600. Surfaces: Library lists, Songs, Home vertical, Home shelves horizontal, Queue, Search, Settings, Song info, lyrics, command palette results. Test: `dui-wheel`: 30 `wheel 0 -1` events on each surface; per event `STAT_DIRTY_PIXELS` at most the scrolled region area and `STAT_FULL_REPAINTS` stays 0; frame time per event (`mem` backend, release) under 16 ms.
- [ ] DUI-P3-10 Widget states are specified once: enabled, hover, pressed, focused, disabled. Disabled draws the `text disabled` token, ignores click and key, shows no hover fade, is skipped by Tab. Focus ring is drawn on keyboard focus only (not on mouse click). A dialog or menu traps Tab inside itself, Escape closes it and returns focus to the control that opened it, Enter activates the focused control. Test: `dui-widget-states`: the widget gallery has one widget per state; a click on a disabled button leaves `STAT_ACTIONS` unchanged; Tab inside a dialog cycles within it (N presses return to the first control); Escape returns focus to the opener (focus id equals the saved id).
- [ ] DUI-P3-11 Hover on list rows (I345). One rule for every list row: the selected row (playing queue row, chosen playlist) has the `SURFACE` round-rect background, a hovered unselected row has no background and only its ink fades in and out (`hover_ink_of`, ANIM_KEY_HOVER, FAST_MS): muted rows go `TEXT_MUTED -> TEXT`, text rows `TEXT -> ACCENT`. Rows of Songs, Library (composers, drilldown, tree, columns, files, history), Queue, Playlists and Tracker samples register through `hover_reg`; `hover_row` and its scrim are gone; `hbox[]` holds 128 boxes (sum in the code comment). Test: `dui-hover-rows`: live loop, `--view library`, timed move onto composer row 1; `hover_box` equals the `row 1` rectangle, `hover_ink=` new weight is 65536 at the end, `hover_box paints=0` (no background fill).
- [ ] DUI-P3-12 Sidebar hover is ink only and switches instantly both ways (I346, I356). The selected door keeps its `SURFACE` background; another door has no background, its icon and label blend `TEXT_MUTED -> TEXT`; the door the pointer left drops to muted at once (I356; `hover_prev` only repaints its region). Test: `dui-hover-sidebar-ink`: live loop, timed move onto door 2, 50 ms later onto door 3, quit 60 ms after that: the frame dump shows door 2 pixel-identical to the rest frame and door 3 lit.
- [x] DUI-P3-13 Hover lights instantly (I351; the fade-out was removed by I356). `hover_weight` is 65536 for the hovered box, 0 for any other. Test: `dui-hover-instant`: live loop, move onto door 2, 50 ms later onto door 3, quit at 60 ms: `hover_ink=65536`.
- [x] DUI-P3-14 Library view reloads after a scan (I348). The engine counts library changes in `sel_state.library_serial` (bumped in emit_library_path, which every scan, sync and fetch finish goes through); the window compares it each turn and reloads its page, and the headless loop keeps turning while a scan is active. Test: `dui-library-reload`: empty db, `--view library`, input script `exec LIBRARY_SCAN <dir of 3 songs>`: stats `library_serial>=1`, `library_reloads>=1`, `Songs in library|3`.
- [x] DUI-P3-15 Composers tab groups untagged songs under Unknown composer (I353). The library composer list (`SEL_COMPOSER_LIST`, src/library/db/songdb.c) emits one group, canon `SEL_COMPOSER_UNKNOWN_CANON`, display `SEL_COMPOSER_UNKNOWN_DISPLAY` (src/library/composer.h), for songs with no song_composers row, sorted last; the drilldown lists those songs. Test: `dui-library-unknown-composer`: 3 untagged mods scanned: one composer row `Unknown composer|3`, click drills in to 3.
- [x] DUI-P3-16 The Now Playing heart and the five rating marks light under the pointer (I357). Each rating mark is a registered hover box: an empty mark warms from `SURFACE_RAISED` to `TEXT_MUTED`, a set mark brightens like an accent fill; the heart goes `TEXT_MUTED -> TEXT` through `row_ink`. Test: `dui-hover-nowplaying`: live run on a playing song, timed move onto the heart and onto the fifth mark: the frame dump differs from the rest frame inside that rectangle and `hovers>=1`.

### Phase 4: frame and views
- [x] DUI-P4-1 Plugin `plugins/ui/desktop/` and the frame: sidebar, content, Now Playing panel, transport bar with shuffle and repeat (from M4-P4-1 plugin part and M4-P4-2). Test: `dui-frame`: `selecta --ui desktop` opens through the plugin registry (kind 5); transport play, pause, next, shuffle change engine state.
- [x] DUI-P4-2 Home view. Test: `dui-home`: fixture history gives the "Jump back in" shelf with the last 6 songs in order.
- [ ] DUI-P4-3 Library and browse: regular, tree, drilldown, column browser, sources, Files, History, Stats (F24) (from M4-P4-3). Test: `dui-library`: drilling into a fixture composer shows its song count from the library; stats page totals equal `selecta-cli stats`.
- [x] DUI-P4-4 Engine view styles grid, cover flow, compact, artwork id in `sel_view_row`; registry offers them where the UI allows (from M4-P3-1, M4-P3-2). Test: `dui-styles`: `VIEW_STYLE grid` on the desktop UI accepted, on the text UI refused with the existing error; rows carry artwork ids.
- [x] DUI-P4-5 Grid and cover flow views (2D). Test: `dui-grid-coverflow`: goldens for both; arrow right in cover flow moves the centred album by 1.
- [ ] DUI-P4-6 (partial 5e6eede, remainder I256) Search with live results grouped by type and facets. Test: `dui-search`: typing `format:mod` gives the fixture count of mod songs; `STAT_LIBRARY_SEARCHES` grows by the debounced count (1 per pause).
- [ ] DUI-P4-7 (partial 4ec6bc3, remainder I257) Now Playing panel: artwork, metadata panel, synced lyrics, up next, rating and like, header tint. Test: `dui-nowplaying`: the LRC fixture advances the highlighted lyric line at the fixture times; `STAT_META_PANEL_ROWS` equals the panel pair count.
- [ ] DUI-P4-8 (partial b10ffaa, remainder I258) Queue: groups, play next, drag reorder, remove, undo and redo, reshuffle and scope (F30, F49). Test: `dui-queue`: play next then undo restores the queue order exactly (queue hash before equals after).
- [ ] DUI-P4-9 (partial 35c64f0, remainder I259) Playlists: folder tree, smart playlists, create, rename, delete with confirm, drag reorder. Test: `dui-playlists`: create in a folder then reload the library; the folder holds the playlist; `STAT_FOLDER_OPENS` counts the open.
- [ ] DUI-P4-10 (partial 71aaf73, remainder I261) Song info page and side sheet: module info, STIL, composer, wrong and skip flags, notes. Test: `dui-songinfo`: setting the wrong flag with a note writes the row that `selecta-cli wrong` lists.
- [ ] DUI-P4-11 (partial f5b3050, remainder I269) `sel_surface` in `viz.h`, viz plugins render to it, viz area in Now Playing, viz tab, fullscreen visual mode and signal path (F38), several viz windows (from M4-P3-3, M4-P12-6). Test: `dui-viz`: with a scope plugin `VIZ_FRAMES` grows at least 25 per second of song time; fullscreen toggle changes the layout id and back.
- [ ] DUI-P4-12 Tracker tabs: pattern view, samples and instruments with click-to-play, phase view (F17), chip register panel (from M4-P3-4). Test: `dui-tracker`: a fixture MOD shows the pattern row that the engine reports at a fixed clock; clicking a sample plays it (sample-play command count 1).
- [ ] DUI-P4-13 (partial 2644c1e, remainder I264) Overlays: command palette, help overlay, command line with completion (F45), context menus, toasts (from M4-P12-2, M4-P12-11). Test: `dui-overlays`: palette query `shuff` runs shuffle; `:` command line completes `pla` to the action list of the fixture.
- [ ] DUI-P4-14 Settings pages generated from `prefs.def`, theme picker with live preview, key binding editor (from M4-P12-1). Test: `dui-settings`: changing each page's first control fires `PREFS_CHANGED` with that key; a rebind is saved and used by the next run.
- [ ] DUI-P4-15 Mini mode and window mode layouts. Test: `dui-mini`: `MODE=mini` golden matches; the window is at most 420x120 logical.
- [ ] DUI-P4-16 Song context menu on every page (I305). Every place that shows a song (Library lists, Songs, Home tiles and shelves, grid tiles, cover flow centre, Search results, Queue rows, Playlist rows, Song info header, Now Playing panel title) opens the same menu on secondary click, built from one table (`g_menu_items`, desktop_ui.c:936, currently Play, Play Next, Add to Queue, Remove) and the keyboard key for the menu; Home tiles currently offer Play Next only. Menu items depend on the page: Remove is Remove from Queue on the Queue page and Remove from Library elsewhere (DUI-P4-18); Add to Playlist submenu is listed (DUI-P5-4 picker). Test: `dui-song-menu`: for each page in the list a secondary click on a fixture song opens a menu whose item labels equal the table; picking Add to Queue adds 1 entry (`menu_picks` and queue length). Landed (I305):  their song rows while drawing (`song_row_reg`); right-click, ctrl-click, the trailing "..." button (lucide ellipsis, on hover) and Shift+F10 open the one menu (`song_menu_open`): Play, Play Next, Add to Queue, Add to Playlist (chooser of the engine's playlists, `PLAYLIST_ADD_SONG`), Remove on queue rows. Replaces `shelf_menu_open`/`shelf_menu_click` and the Songs-page menu. Wired: Songs, Queue, Home shelves, Search songs, Composers drilldown. Landed (I371, 1b97a9c, 6b2b032): Library Tree/Columns/Files/History rows register through `lib_view_row_reg` and the new engine query `sel_engine_view_row_path`; tests `dui-song-menu-live` (real SDL loop, timed right button, label song-menu) and `dui-song-menu-lists` (Tree leaf, Files row). Open (I372): Library grid and cover flow tiles, Home grid tiles, Song info header, Now Playing title; Search and Home shelves are wired but not asserted. Stays open until every listed place is wired.
- [ ] DUI-P4-17 Text overflow and truncation rules (I307). One rule per text kind: single-line titles ellipsize at the end, paths ellipsize in the middle, lyrics, song info values and dialog text wrap and the row grows, numbers and times never truncate (the column is sized for the widest value). No glyph is drawn outside its widget rect. A list shows `floor(room / row height)` rows: the engine view window is sized to the room under the list top, so a 600 px canvas does not hold 12 rows of 48 px that overflow it. Test: `dui-overflow`: fixture library with 120-character titles at 640x400, 900x600 and 1280x800 (scale 1 and 2); `STAT_TEXT_OUTSIDE_RECT` = 0 for every view, and the row count of each list equals `floor(room / row height)`; golden per size.
- [ ] DUI-P4-18 Remove from Library and Clear Library (I313). Song menu: Remove from Library, with the confirm dialog of DUI-P5-4 naming the title. Library page and Settings: Clear Library, confirm dialog naming the song count and the playlist count and stating that audio files on disk are not touched. Both go through engine commands, not direct database writes from the UI. Test: `dui-library-remove`: remove one fixture song, confirmed: `selecta-cli stats` song count drops by 1 and the file still exists; cancelled: count unchanged; Clear Library confirmed (Q16, Johan 2026-10-25: removes everything, playlists included): song count 0, playlist count 0, ratings and notes gone, settings kept, queue is emptied; thumbnails of removed songs are deleted (`thumb` row count 0).
- [ ] DUI-P4-19 Liked state and rating (I306). The Now Playing heart shows the liked state of the playing song and refreshes in the same frame as a LIKE toggle; the five rating marks show the stored rating and a second click on the stored rating clears it; both sync to DEViLBOX through `pending_push`. Rows and tiles show a heart on hover, filled when liked (Spotify feel; Q13). Test: `dui-like-rate`: `click` on the heart flips `np_liked` and the heart pixels in one frame, the library row has `liked=1`, one `pending_push` row exists; `click` on rating mark 3 sets 3, a second click sets 0; liked-songs source count follows.
- [ ] DUI-P4-20 Compact full-screen layout (I343). Today a canvas under 700 px wide takes the 40 px mini strip (`sel_uim_breakpoint_for` returns mini-suggestion, layout.c:32; navigation arrows hidden under 400 px, desktop_ui.c:3592), so a 640x400 RTG screen or a 680x270 Replay mode has no pages. A compact layout is added: single column, the active page full width, the sidebar entries as one icon row, the transport as one footer row; the mini strip is only the user choice `MODE=mini`. Minimum supported canvas and the row and icon sizes are open (Q14, Q15). Test: `dui-compact`: `resize` to 640x400 and 680x270: layout id compact, every page of section 3 reachable by click and key, no widget rect outside the canvas, goldens for Home, Library, Now Playing, Queue in both sizes.
- [ ] DUI-P4-21 Empty states, loading states and error toasts. Each list view has an empty state (icon, one line, one action): empty library, empty queue, no search results, empty playlist, no lyrics, no artwork (generated art, DUI-P2-5), no liked songs. A slow source shows a loading row, never a blank page. Failures toast through the one toast widget: song failed to load, library write failed, remote fetch failed, bad format string (DUI-P5-9), drop of an unsupported file. Toast time on screen and stacking are open (Q12). Test: `dui-empty-errors`: an empty fixture library shows the empty state of each view (golden per view, `STAT_EMPTY_STATES` = 7); a `LOAD` of a corrupt fixture shows one error toast naming the file and the engine state stays stopped.

### Phase 5: desktop features
- [ ] DUI-P5-1 Remembered state: window position and size per mode, last view, browse style, search scope, panel widths (from M4-P12-5). Test: `dui-state`: run 1 changes them, run 2 opens with them (layout hash equal).
- [ ] DUI-P5-2 Info line and output indicator modes (from M4-P12-7). Test: `dui-infoline`: each mode key cycles the text to the fixture strings.
- [ ] DUI-P5-3 Theme presets: Selecta Dark, Selecta Light, the four Amiga presets; follow system appearance (from M4-P12-8). Test: `dui-themes`: each preset gives its golden; `ui.desktop.appearance=system` with a scripted OS theme event switches dark to light.
- [ ] DUI-P5-4 Dialogs and first start: confirm, text prompt, playlist picker, collection picker, Sounds Wrong note, guard on destructive actions (from M4-P12-9). Test: `dui-dialogs`: delete playlist without confirm keeps it; with confirm removes it.
- [ ] DUI-P5-5 Activity panel with Downloads and Extra Files: progress, failures, Stop and Continue (from M4-P12-10). Test: `dui-activity`: a fixture remote fetch shows progress and Stop cancels it (job state cancelled).
- [ ] DUI-P5-6 Fonts and sizes: size steps for list and interface text, optional user TTF through the atlas builder (from M4-P12-3, desktop part). Test: `dui-fonts`: size step +1 changes the row height token result and the golden.
- [ ] DUI-P5-7 External lookup: web search and Demozoo search open a URL (`SDL_OpenURL`) (from M4-P12-14). Test: `dui-lookup`: with the URL opener stubbed by `--url-log`, the logged URL equals the fixture.
- [ ] DUI-P5-8 Stored programs on F1 to F10 (from M4-P12-15). Test: `dui-fkeys`: F3 bound to a saved playlist loads it and plays song 1.
- [ ] DUI-P5-9 Columns, title formats and status-line format (F48) from the view model (from M4-P12-16). Test: `dui-columns`: a custom format shows the expanded string from `sel_format_expand()`; a bad format falls back and toasts the error.
- [ ] DUI-P5-10 Clipboard URL watch (F43) in `plugins/ui/desktop/os_<os>.c`. Test: `dui-clipboard`: a scripted clipboard event with a Modland URL shows the offer toast; accept queues that song.
- [ ] DUI-P5-11 Drop files and folders on the window; Add Files, Add Folder, Save Playlist through SDL3 file dialogs. Test: `dui-drop`: `drop` of two fixture files adds 2 queue entries (`STAT_DROPS=1`); dialogs are stubbed by `--dialog-answer` in tests.
- [ ] DUI-P5-12 Media keys play, pause, next, previous. Test: `dui-mediakeys`: scripted media key events change engine state as the transport buttons do.

### Phase 6: performance and Amiga readiness
- [ ] DUI-P6-1 No heap allocation after open; arena size printed. Test: `dui-noalloc`: 10 000-frame scripted run with views, scrolls and song changes; `STAT_HEAP_ALLOCS_AFTER_OPEN=0` (malloc counter in the test build); `ui_arena_bytes` at 640x512 under 1.5 MB.
- [ ] DUI-P6-2 Host frame budget and idle behaviour of section 1 item 4. Test: `dui-perf` (release build only, label `dui-perf`): 10 000-row scroll at 60 fps on the `mem` backend measured in frame time; idle 10 s draws 0 frames after the first.
- [x] DUI-P6-3 m68k cross-build of `src/gui/` and `src/uimodel/` for `-m68020`, no floating-point symbols. Test: `dui-m68k-nofloat`: the Amiga toolchain builds the library; `nm` shows none of the soft-float helpers (`__mulsf3`, `__adddf3`, and the rest of the list in the test).
- [x] DUI-P6-4 INDEX8 colour budget: 32 UI, 32 generated-art, 3 x 64 artwork sub-palettes, quantiser and dither. Test: `dui-aga-budget`: the 11 INDEX8 goldens match and `STAT_PALETTE_COLOURS` per region stays within the budget.
- [ ] DUI-P6-5 Pixel-work proxy for 68020 targets (M4 4.5: 40 ms input-to-redraw): count pixels written per common action. Test: `dui-pixel-budget`: at 640x512, a row move writes at most 2 rows of pixels, a song change at most 25 percent of the canvas.
- [ ] DUI-P6-6 Knob sweep over every token and every `ui.desktop.*` pref, instrument checked first on one live and one dead knob. Test: `dui-knob-sweep`: each knob changes the golden hash or has a written exemption in the token table; the check knobs report LIVE and DEAD.
- [ ] DUI-P6-7 Missing numbers measured and recorded in the token table: cold start to first frame, window resize relayout time, artwork cache byte cap and eviction rule, thumbnail sizes for `thumb<size>` rows (Q7 left the sizes open), peak resident memory with the 10 000-song fixture, scroll momentum time, toast and tooltip delays. Each is measured on the host release build and written as a budget. Test: `dui-budgets`: the run prints each number as `budget_<name>=` and fails when one exceeds the committed value; no budget line is empty.

### Phase 7: landing
- [ ] DUI-P7-1 Manual check by Johan: LOCAL, `./build/host/apps/selecta/selecta --ui desktop` on the Retina screen and on an external screen; steps: open, browse, grid, cover flow, play, lyrics, queue drag, settings theme switch, mini mode, move the window between screens. PASS = no blurry text after the move, no stutter in cover flow, every label in full English words. Test: Johan's yes recorded in the ledger.
- [ ] DUI-P7-2 Ratchet at zero DUI IDs; design doc text of section 6 applied after review; M4 plan revision items (section 9) opened as ledger IDs. Test: `ratchet` passes with 0 DUI IDs.

Count check: Phase 0: 2, Phase 1: 11, Phase 2: 8, Phase 3: 10, Phase 4: 21, Phase 5: 12, Phase 6: 7, Phase 7: 2. Total 73 (61 original plus 12 added by the gap review, I342).

## 6. Proposed design doc text (not applied)

Section 14, insert after the M3c line:

> - M3d Desktop UI: shared GUI library `src/gui/` (widget set, layout tree, design tokens, fixed-point rasterizer, glyph atlases, artwork pipeline) over `src/uimodel/`, with a headless `mem` backend and an SDL3 backend; plugin `plugins/ui/desktop/`. All views of sections 9a to 9c including grid and 2D cover flow, Spotify-style layout, dark and light themes, HiDPI, touchpad gestures. The M4 custom Amiga UI adds only RTG and AGA backends to the same library (D-M4-5, D-M4-6).

Section 14, M5 line: replace "SDL desktop UI with grid and cover flow" with "GPU tier of the desktop UI (3D cover flow, `sel_gpu_ctx`), bitmap skin loader".

Section 14, M4 line: add "custom Amiga UI = RTG and AGA backends of `src/gui/` (M3d)".

D5, replace:

> - D5 Desktop GUI = SDL3 + own widget set in C, shared with the custom Amiga UI: one widget set, layout and token layer with backends `mem`, `sdl`, `rtg`, `aga`. Why: one codebase on three desktop OSes and the Amiga, the same look by construction, golden renders headless; native look is not a goal. GPU visualizers and skins come later on the same widget set.

D8, append: "Tokens are one X-macro table in `src/gui/theme/`, parsed with the text UI theme syntax; presets Selecta Dark and Selecta Light plus the Amiga presets."

Section 7 planned UIs: change "SDL2 desktop (D5)" to "desktop (SDL3, D5, M3d)".

## 7. Open questions

Each has a recommended answer; the decisive trade-off is in the last sentence.

- Q1 SDL2 or SDL3? Recommended: SDL3 (3.4 installed; Homebrew `sdl2` is already `sdl2-compat` over SDL3). SDL3 brings file dialogs, pixel density per window, pinch events and the offscreen video driver the tests need. Trade-off: D5 says SDL2, so D5 text changes (section 6), and SDL2-only Linux distributions need the compat layer.
- Q2 Layout grid? Recommended: 8-unit grid with a 4-unit half step, three columns (sidebar 240 to 320, content flexible, Now Playing 280 to 420), breakpoints at 1100 and 900 logical px, content max width none. Trade-off: on AGA 640x512 the unit has to drop to 4 px through tokens, so every size must be a token, never a literal.
- Q3 Colour scheme? Recommended: neutral near-black surfaces in three steps, one Selecta accent colour (not Spotify green, to avoid copying a brand), artwork-derived tint on the Now Playing header. Trade-off: the tint costs one palette slot per region on AGA and makes goldens depend on the artwork fixture.
- Q4 Typography? Recommended: Inter (OFL) regular and semibold, five size tokens (11, 13, 16, 24, 32 logical px), atlases built by one `stb_truetype` builder at build time for the Amiga and at open on desktop. Trade-off: no complex shaping (no HarfBuzz), so CJK and right-to-left titles show box glyphs until a later milestone.
- Q5 Dark, light or both? Recommended: both, Selecta Dark default, `ui.desktop.appearance = dark | light | system`. Trade-off: doubles the golden set (44 instead of 22 RGBA hashes) and every colour token needs two values.
- Q6 Grid and cover flow: pull forward from M5? Recommended: yes, 2D only; 3D GPU variant stays M5. Trade-off: the engine style work (M4-P3-1) lands before M4 and widens this milestone by two items.
- Q7 Artwork thumbnails in the library database? Recommended: yes, `kind = thumb<size>` rows. Trade-off: the database grows by about 30 KB per album, in return the Amiga never decodes a full JPEG.
- Q8 Icon set? Recommended: Lucide (ISC), rasterised at build time into an alpha atlas. Trade-off: a build-time SVG rasteriser step (one small tool under `tools/`), no runtime vector code.
- Q9 (I342) Hover and press fade times? Recommended: 120 ms hover, 80 ms press, 250 ms view cross-fade, tooltip after 600 ms (the DUI-P3-6 test value). Trade-off: shorter feels snappier on Spotify but shows as flicker at 60 fps on the Amiga tiers, where the governor turns tweens off anyway. Answer (Johan 2026-10-25): 120 ms hover, 80 ms press, 250 ms view cross-fade, tooltip after 600 ms (recommended default taken, not asked).
- Q10 (I342) Maximum wheel and keyboard scroll speed and momentum? Recommended: 16 px per notch, OS momentum from the touchpad, no added smoothing. Trade-off: no smoothing keeps one frame per event (I310), but a mouse wheel feels steppy against Spotify. Answer (Johan 2026-10-25): 16 px per notch, OS momentum, no added smoothing (recommended default taken, not asked).
- Q11 (I342) Tooltips on disabled controls? Recommended: yes, the tooltip names why (for example "Nothing in the queue"). Trade-off: one more string per disabled state to write. Answer (Johan 2026-10-25): yes, tooltip names why (recommended default taken, not asked).
- Q12 (I342) Toast duration and stacking? Recommended: 3 s, errors 6 s, at most one visible, newer replaces older. Trade-off: a replaced error toast can be missed; a stack costs vertical space and an arena slot per toast. Answer (Johan 2026-10-25): 3 s, errors 6 s, one visible, newer replaces older.
- Q13 (I342) Heart on every song row and tile on hover? Recommended: yes, as Spotify does. Trade-off: one more hit target per row (accidental likes on touchpads) and a DEViLBOX sync per click. Answer (Johan 2026-10-25): yes, heart on every song row and tile on hover.
- Q14 (I342) Minimum supported canvas for the compact layout? Recommended: 640x400 (the RTG screen of I340), with 680x270 working through scrolling. Trade-off: smaller sizes need a second row-height token step; larger minimum leaves the Replay mode on the mini strip. Answer (Johan 2026-10-25): many AGA users run 640x256. 640x256 is the design target, fully laid out with no scrolling, goldens at 640x256; 640x200 and 680x270 scroll; 640x400 gets the same layout with more rows. Below 640 px width: text UI only (Amiga).
- Q15 (I342) Compact navigation pattern? Recommended: bottom icon row (Home, Library, Search, Queue, Now Playing) above the transport, as Spotify mobile. Trade-off: the bottom row costs about 40 px of height, which is scarce at 270 px; a side icon column costs width instead. Answer (Johan 2026-10-25): bottom icon row.
- Q16 (I342) Clear Library scope? Recommended: removes songs, ratings, notes and thumbnails, keeps playlists as empty shells and settings, never touches files on disk. Trade-off: playlists with no songs look broken; deleting them too is irreversible and alpha data has no backup. Answer (Johan 2026-10-25): Clear Library removes everything, playlists included (songs, ratings, notes, thumbnails, playlists); settings stay; files on disk are never touched. Irreversible, so the confirm dialog names the playlist count.
- Q17 (I342) Empty state copy and illustration? Recommended: one icon, one sentence, one action button per view (for example "Add Files" in an empty library). Trade-off: copy is product voice and is yours to word; illustrations need assets the icon atlas does not hold. Answer (Johan 2026-10-25): one icon, one sentence, one action button per view (recommended structure taken); Johan words the copy at review.

### 7.1 Answers (Johan, 2026-10-24)

All eight recommended answers were chosen. They are decisions now; do not reopen them.

- D-DUI-1 (Q1): SDL3. The D5 text changes as section 6 proposes.
- D-DUI-2 (Q2): 8-unit grid with a 4-unit half step, three columns, breakpoints 1100 and 900 logical px. Every size is a token.
- D-DUI-3 (Q3): neutral near-black surfaces in three steps, one Selecta accent colour, artwork tint on the Now Playing header.
- D-DUI-4 (Q4): Inter regular and semibold, five size tokens (11, 13, 16, 24, 32), one `stb_truetype` atlas builder. No HarfBuzz in this milestone.
- D-DUI-5 (Q5): dark and light themes, Selecta Dark the default, `ui.desktop.appearance = dark | light | system`.
- D-DUI-6 (Q6): grid and 2D cover flow move forward from M5; the 3D GPU variant stays in M5.
- D-DUI-7 (Q7): artwork thumbnails stored in the library database as `kind = thumb<size>` rows.
- D-DUI-8 (Q8): Lucide icons, rasterised at build time into an alpha atlas.
- D-DUI-9 (Johan 2026-10-24): two token layers, base values plus role tokens that reference them (section 2.2), after the Spotify Encore model.
- D-DUI-10 (Johan 2026-10-24): one shared C renderer (`src/gui/`) for desktop and Amiga; no React or Chromium shell and no separate web UI to port. Rejected because porting a web UI to C means rewriting it, two codebases drift apart, and a Chromium shell adds about 150-200 MB per platform. The Spotify look comes from tokens, layout and the I230 design gaps.

## 8. Coverage

One row per UI-relevant source item. Mapping is a DUI ID or `not in this milestone: <reason>`.

Sources:
- `design-1a`: architecture design section 1a feature table rows (line numbers in `2026-10-22-architecture-design.md`) that have a UI part.
- `ledger-F`: feature rows in `thoughts/spot/todos.md` that are UI work.
- `stream`: `2026-10-22_features-modern-streaming.md` lines 18 to 33 (Spotify features) and 170 to 178 (library presentation).
- `hippo`, `bbmp`, `era`, `cat`: the parity and catalogue rows the M4 plan selected in its sections 9.7 to 9.9, with their M4 mapping kept in the third column and remapped through the M4 ID to a DUI ID in the fourth (M4 IDs marked `(Amiga)` stay in M4).

### 8.1 Design 1a, ledger and streaming rows

| Source | Item | Mapping |
|---|---|---|
| design-1a:43 | Channel mute and solo; pitch and tempo | DUI-P4-12 |
| design-1a:44 | Subsongs, random, repeat modes, song length | DUI-P4-1 |
| design-1a:45 | Typed search filters, one query language | DUI-P4-6 |
| design-1a:46 | Smart playlists = saved queries | DUI-P4-9 |
| design-1a:47 | Home view: recently played, jump back in, new this week | DUI-P4-2 |
| design-1a:48 | Ratings, liked list, history, scrobbling | DUI-P4-7 |
| design-1a:50 | Favourites, kept songs, cache budget, prefetch | DUI-P4-14 |
| design-1a:51 | Pattern scroller, sample and instrument list with click-to-play | DUI-P4-12 |
| design-1a:52 | Per-voice scopes, spectrum, VU | DUI-P4-11 |
| design-1a:53 | Chip register panels | DUI-P4-12 |
| design-1a:54 | GPU visualizers + generated artwork per format | DUI-P2-5 (generated artwork); GPU part not in this milestone: M5 |
| design-1a:55 | Switchable view styles incl. cover flow, grid, compact | DUI-P4-4 |
| design-1a:58 | Collaborative lists, votes and comments inline | DUI-P4-10 |
| design-1a:59 | Share links `selecta://` | DUI-P4-13 |
| design-1a:63 | HVSC / STIL / composer / release metadata panel | DUI-P4-10 |
| design-1a:67 | Sleep timer and alarm | DUI-P4-14 |
| design-1a:69 | Radio across collections | DUI-P4-8 |
| design-1a:70 | Playlist folders tree | DUI-P4-9 |
| design-1a:75 | OS file integration: drag-drop, open-with | DUI-P5-11; Amiga AppWindow stays M4-P7-2 |
| design-1a:76 | Cue points and bookmarks per song | DUI-P4-10 |
| design-1a:79 | Composer pages across sources | DUI-P4-3 |
| design-1a:80 | Similar and also-by | DUI-P4-7 |
| design-1a:81 | Duplicate detection, best-source choice | DUI-P4-10 |
| design-1a:82 | Local notes and tag edits | DUI-P4-10 |
| design-1a:83 | Module info panel | DUI-P4-10 |
| design-1a:84 | Seek by pattern / order position | DUI-P4-12 |
| design-1a:85 | Sample export to WAV / 8SVX | DUI-P4-12 |
| design-1a:86 | Global hotkeys and user key bindings | DUI-P4-14 (bindings); global hotkeys not in this milestone: needs OS hotkey API per desktop OS, no ledger ID yet |
| design-1a:92 | Design tokens per UI, bitmap skin loader (D8) | DUI-P1-10; skin loader not in this milestone: M5 |
| design-1a:95 | Phase view (F17) | DUI-P4-12 |
| design-1a:98 | Amiga OS integration | not in this milestone: Amiga, M4-P7-1 to M4-P7-5 |
| design-1a:99 | Wrong flag with note, skip flag | DUI-P4-10 |
| design-1a:102 | Synced lyrics (F19) | DUI-P4-7 |
| design-1a:106 | Stats screen (F24) | DUI-P4-3 |
| design-1a:109 | Reshuffle, play scope, next and previous album (F30) | DUI-P4-8 |
| design-1a:116 | Signal path display and fullscreen visual mode (F38) | DUI-P4-11 |
| design-1a:120 | Clipboard URL watch (F43) | DUI-P5-10 |
| design-1a:121 | Batch tag editing, fingerprint tagging | not in this milestone: M5 per design 1a; single-song tag edit is DUI-P4-10 |
| design-1a:122 | Vi-style command mode (F45) | DUI-P4-13 |
| design-1a:124 | Configurable columns and formats (F48) | DUI-P5-9 |
| design-1a:125 | Command palette, help overlay, undo/redo (F49) | DUI-P4-13 |
| ledger-F:F3 | Visual picks | DUI-P4-11 |
| ledger-F:F10 | Metadata picks | DUI-P4-10 |
| ledger-F:F11 | Tracker extras picks | DUI-P4-12 |
| ledger-F:F17 | Phase view | DUI-P4-12 |
| ledger-F:F19 | Synced lyrics | DUI-P4-7 |
| ledger-F:F24 | Stats screen | DUI-P4-3 |
| ledger-F:F27 | Amiga OS integration | not in this milestone: Amiga, M4-P7-1 to M4-P7-5 |
| ledger-F:F28 | Wrong flag and skip flag | DUI-P4-10 |
| ledger-F:F30 | Reshuffle, play scope | DUI-P4-8 |
| ledger-F:F38 | Signal path display | DUI-P4-11 |
| ledger-F:F43 | Clipboard URL watch | DUI-P5-10 |
| ledger-F:F44 | Batch tag editing, fingerprint tagging | not in this milestone: M5 per design 1a:121 |
| ledger-F:F45 | Vi-style command mode | DUI-P4-13 |
| ledger-F:F48 | Columns, title formats, status-line format | DUI-P5-9 |
| ledger-F:F49 | Command palette, help overlay, undo/redo | DUI-P4-13 |
| stream:18 | Home / Jump back in | DUI-P4-2 |
| stream:19 | Liked songs | DUI-P4-7 |
| stream:20 | Queue, play next | DUI-P4-8 |
| stream:21 | Playlist folders | DUI-P4-9 |
| stream:22 | Smart Shuffle | not in this milestone: engine recommendation feature, no ledger ID; shuffle button is DUI-P4-1 |
| stream:23 | Reshuffle | DUI-P4-8 |
| stream:24 | Radio | DUI-P4-8 |
| stream:25 | Connect (device hand-off) | not in this milestone: M5 remote protocol |
| stream:26 | Crossfade | DUI-P4-14 |
| stream:27 | Loudness normalisation | DUI-P4-14 |
| stream:28 | Collaborative playlists | DUI-P4-10 |
| stream:29 | Blend | not in this milestone: no ledger ID, needs accounts |
| stream:30 | Canvas (looping video behind the song) | not in this milestone: no video decoder; header tint in DUI-P4-7 is the closest |
| stream:31 | Lyrics | DUI-P4-7 |
| stream:32 | Share links | DUI-P4-13 |
| stream:33 | Offline | DUI-P4-14 (cache budget and kept songs pages) |
| stream:170 | Column browser | DUI-P4-3 |
| stream:171 | Facets | DUI-P4-6 |
| stream:172 | Cover flow | DUI-P4-5 |
| stream:173 | Grid | DUI-P4-5 |
| stream:174 | Tree | DUI-P4-3 |
| stream:175 | Drilldown | DUI-P4-3 |
| stream:176 | Smart playlists | DUI-P4-9 |
| stream:177 | Query languages | DUI-P4-6 |
| stream:178 | Nested rules | DUI-P4-9 |

### 8.2 Parity and catalogue rows (from the M4 plan 9.7 to 9.9)

| Source | Item | M4 mapping | Mapping |
|---|---|---|---|
| hippo:25 | Play mode button next to position slider | M4-P12-12 | DUI-P4-1 (shuffle and repeat buttons in the transport bar); Amiga gadget stays M4-P12-12 |
| hippo:37 | Alarm (timed start) | not in M4: engine scheduler, not UI (F13a NEW, unhomed) | not in this milestone: engine scheduler, not UI (F13a NEW, unhomed) |
| hippo:45 | Doublebuffering of sample data | not in M4: output layer, M2 | not in this milestone: output layer, M2 |
| hippo:70 | F1-F10 stored programs | M4-P12-15 | DUI-P5-8 |
| hippo:77 | Module comment file | not in M4: engine metadata (see 9.5); shown by M4-P4-3 | not in this milestone: engine metadata (see 9.5); shown by DUI-P4-3 |
| hippo:78 | Hide window hotkey H | M4-P12-12 | not in this milestone: Amiga-only, stays M4-P12-12 (Amiga) |
| hippo:82 | Archives LhA, LZX, Zip, gzip | not in M4: engine loader, M2 | not in this milestone: engine loader, M2 |
| hippo:83 | XPK, PowerPacker, XFDmaster | not in M4: engine loader, M2 | not in this milestone: engine loader, M2 |
| hippo:92 | Search source remembered | M4-P12-5 | DUI-P5-1 |
| hippo:96 | Infobox downloaded byte count | M4-P12-7 | DUI-P5-2 |
| hippo:99 | UHCBIN network transport | not in M4: source layer, not UI | not in this milestone: source layer, not UI |
| hippo:105 | Resizable main and info windows | M4-P5-3, M4-P10-1 (NEWSIZE relayout) | DUI-P2-7 (window resize relayout); Amiga NEWSIZE stays M4-P10-1 |
| hippo:106 | Window shrink / expand (zip) | M4-P5-3 (zoom gadget, Mini player) | DUI-P4-15 (mini mode); Amiga zoom gadget stays M4-P5-3 |
| hippo:107 | Any font for list and infobox/buttons | M4-P12-3 | DUI-P5-6 |
| hippo:108 | Big buttons | M4-P12-3 | DUI-P5-6 |
| hippo:109 | Button tooltips | M4-P12-4 | DUI-P3-6 |
| hippo:110 | Public screen selection | M4-P7-1 | not in this milestone: Amiga-only, stays M4-P7-1 (Amiga) |
| hippo:111 | Own screen option | M4-P6-1, M4-P5-3 | not in this milestone: Amiga-only, stays M4-P6-1 (Amiga), M4-P5-3 (Amiga) |
| hippo:112 | Filebox display and zoom | M4-P5-3, M4-P2-4 | M4-P5-3 (Amiga), DUI-P1-7 |
| hippo:113 | Disable info scroll | M4-P12-7 | DUI-P5-2 |
| hippo:114 | Info-line modes 7/8/9/0 | M4-P12-7 | DUI-P5-2 |
| hippo:115 | Module info window with wheel scroll | M4-P4-3, M4-P4-4 | DUI-P4-3, DUI-P3-2 |
| hippo:116 | Info window: SID chip, instruments, channels | M4-P4-3 | DUI-P4-3 |
| hippo:117 | Info window: author, publisher, album, year | M4-P4-3 | DUI-P4-3 |
| hippo:118 | Info window: STIL trivia | M4-P4-3 | DUI-P4-3 |
| hippo:119 | AHI indicator | M4-P12-7 | DUI-P5-2 |
| hippo:120 | Keep window positions and open states | M4-P12-5 | DUI-P5-1 |
| hippo:121 | About window | M4-P12-2 | DUI-P4-13 |
| hippo:122 | Prefs window | M4-P12-1 | DUI-P4-14 |
| hippo:127 | Quadra scope | M4-P12-6 | DUI-P4-11 |
| hippo:128 | Filled quadra scope | M4-P12-6 | DUI-P4-11 |
| hippo:129 | Hippo scope | M4-P12-6 | DUI-P4-11 |
| hippo:130 | Bars scope | M4-P12-6 | DUI-P4-11 |
| hippo:131 | Spectrum analyzer | M4-P3-3 | DUI-P4-11 |
| hippo:132 | Patternscope 4-18 channels, XL rows | M4-P3-3 (pattern plugin) | DUI-P4-11 (pattern plugin) |
| hippo:133 | PSID register scope | M4-P3-4 | DUI-P4-12 |
| hippo:134 | Several scopes at once | M4-P12-6 | DUI-P4-11 |
| hippo:135 | Scope size, priority, bars settings | M4-P12-6 | DUI-P4-11 |
| hippo:136 | Z toggles scopes | M4-P12-6 | DUI-P4-11 |
| hippo:137 | External scope plugins | M4-P3-3 (viz plugin ABI) | DUI-P4-11 (viz plugin ABI) |
| hippo:138 | Module info window (I) | M4-P4-3, M4-P4-5 | DUI-P4-3, DUI-P3-3 |
| hippo:139 | Per-sample listing | M4-P4-3 (tracker tabs) | DUI-P4-3 (tracker tabs) |
| hippo:140 | Synth sample / size counts | M4-P4-3 (tracker tabs) | DUI-P4-3 (tracker tabs) |
| hippo:145 | Prefs page Play | M4-P12-1 | DUI-P4-14 |
| hippo:146 | Prefs page Show | M4-P12-1 | DUI-P4-14 |
| hippo:157 | MHI decoder hardware | not in M4 UI plan: players plan (F26) | not in this milestone UI plan: players plan (F26) |
| hippo:158 | AmiGUS mixer | not in M4 UI plan: players plan (F26) | not in this milestone UI plan: players plan (F26) |
| hippo:159 | Trinity audio | not in M4 UI plan: players plan (F26) | not in this milestone UI plan: players plan (F26) |
| hippo:160 | Load / save prefs | M4-P12-1 | DUI-P4-14 |
| hippo:162 | Hotkeys enable | M4-P12-12 | not in this milestone: Amiga-only, stays M4-P12-12 (Amiga) |
| hippo:163 | Double-click play toggle | M4-P12-12 | DUI-P3-2 (double-click plays) |
| hippo:164 | Task priority setting | M4-P12-12 | not in this milestone: Amiga-only, stays M4-P12-12 (Amiga) |
| hippo:165 | HippoPlayer.group location | not in M4: Selecta has no group file; replayer set comes from the plugin registry | not in this milestone: Selecta has no group file; replayer set comes from the plugin registry |
| hippo:211 | ToolType parsing | M4-P7-5 | not in this milestone: Amiga-only, stays M4-P7-5 (Amiga) |
| hippo:212 | UHCBIN environment variable | not in M4: source layer | not in this milestone: source layer |
| hippo:213 | PIPE: streaming names | not in M4: source layer | not in this milestone: source layer |
| hippo:219 | R opens prefs | M4-P12-1, M4-P4-5 | DUI-P4-14, DUI-P3-3 |
| hippo:228 | ESC exit | M4-P4-5 | DUI-P3-3 |
| hippo:233 | HELP about | M4-P12-2 | DUI-P4-13 |
| hippo:235 | ` window shrink / expand | M4-P5-3, M4-P4-5 | M4-P5-3 (Amiga), DUI-P3-3 |
| hippo:236 | 7/8/9/0 info-line modes | M4-P12-7 | DUI-P5-2 |
| hippo:237 | I module info | M4-P4-5 | DUI-P3-3 |
| hippo:238 | F1-F10 stored programs | M4-P12-15 | DUI-P5-8 |
| hippo:240 | H hide window; [ join program | M4-P12-12; join program = M4-P4-5 (playlist append action) | M4-P12-12 (Amiga); join program = DUI-P3-3 (playlist append action) |
| hippo:242 | Z scope toggle | M4-P12-6 | DUI-P4-11 |
| hippo:269 | STIL.txt trivia | M4-P4-3 (engine done M3) | DUI-P4-3 (engine done M3) |
| hippo:270 | Hardware outputs | not in M4 UI plan: players plan (F26) | not in this milestone UI plan: players plan (F26) |
| hippo:275 | reqtools file requesters | M4-P7-3 (asl) | DUI-P5-11 (desktop file dialogs); asl stays M4-P7-3 |
| hippo:276 | Keyfile / registration | not in M4: out of scope, Selecta has no registration | not in this milestone: out of scope, Selecta has no registration |
| hippo:278 | Prefs file location | M4-P12-1 (prefs store done M1) | DUI-P4-14 (prefs store done M1) |
| hippo:279 | PS3M config file | not in M4 UI plan: players plan | not in this milestone UI plan: players plan |
| hippo:280 | Iconify to AppIcon | M4-P5-3 | not in this milestone: Amiga-only, stays M4-P5-3 (Amiga) |
| hippo:281 | Mouse wheel in lists and info window | M4-P4-4 | DUI-P3-2 |
| hippo:282 | Kickstart 1.2, 68000 baseline | not in M4: runtime tier, later (cat:255) | not in this milestone: runtime tier, later (cat:255) |
| bbmp:41 | Loudness on Info with applied gain; off switch | M4-P4-3, M4-P12-1 | DUI-P4-3, DUI-P4-14 |
| bbmp:46 | Last visualiser remembered | M4-P12-5 | DUI-P5-1 |
| bbmp:51 | Per-format status line (verified / faults / does not play) | M4-P4-3 (Song info view) | DUI-P4-3 (Song info view) |
| bbmp:60 | Queue tab; Enter jumps to song | M4-P4-3, M4-P4-5 | DUI-P4-3, DUI-P3-3 |
| bbmp:69 | Playlists tab; queue a whole playlist | M4-P4-3, M4-P4-5 | DUI-P4-3, DUI-P3-3 |
| bbmp:72 | Import via Files (.m3u) | M4-P4-3 (Files view; import engine done M3) | DUI-P4-3 (Files view; import engine done M3) |
| bbmp:85 | Three-column Browse | M4-P3-1, M4-P10-2 (drilldown style) | DUI-P4-4, M4-P10-2 (Amiga) (drilldown style) |
| bbmp:86 | First list: All plus each collection | M4-P3-1 (engine done M3) | DUI-P4-4 (engine done M3) |
| bbmp:87 | Per-collection Formats, Platforms, Chips, Artists, New | M4-P3-1 (engine done M3) | DUI-P4-4 (engine done M3) |
| bbmp:88 | Formats > artists > songs | M4-P3-1 (engine done M3) | DUI-P4-4 (engine done M3) |
| bbmp:89 | Platforms drill | M4-P3-1 (engine done M3) | DUI-P4-4 (engine done M3) |
| bbmp:90 | Sound chips drill | M4-P3-1 (engine done M3) | DUI-P4-4 (engine done M3) |
| bbmp:91 | Artists A-Z | M4-P3-1 (engine done M3) | DUI-P4-4 (engine done M3) |
| bbmp:93 | HVSC layout view | M4-P3-1 (engine done M3) | DUI-P4-4 (engine done M3) |
| bbmp:94 | Sizes at every Browse level | M4-P3-1 | DUI-P4-4 |
| bbmp:95 | Narrow width drops the parent column; reopens where left | M4-P2-4 (breakpoints), M4-P12-5 | DUI-P1-7 (breakpoints), DUI-P5-1 |
| bbmp:96 | Go to playing song | M4-P4-5 | DUI-P3-3 |
| bbmp:99 | Search tab / box | M4-P4-3 | DUI-P4-3 |
| bbmp:102 | Search scope next collection / all | M4-P4-5 | DUI-P3-3 |
| bbmp:103 | Search box Enter / Down / Esc | M4-P4-5 | DUI-P3-3 |
| bbmp:104 | Artist page | M4-P4-3 (composer pages) | DUI-P4-3 (composer pages) |
| bbmp:107 | Web and Demozoo search | M4-P12-14 | DUI-P5-7 |
| bbmp:108 | Song info page | M4-P4-3 | DUI-P4-3 |
| bbmp:109 | Info tab: format notes, failure reason, loudness | M4-P4-3 | DUI-P4-3 |
| bbmp:110 | Page Up / Page Down | M4-P4-5 | DUI-P3-3 |
| bbmp:111 | Collection filter setting | M4-P12-1, M4-P12-5 | DUI-P4-14, DUI-P5-1 |
| bbmp:112 | Remembered browse view | M4-P12-5 | DUI-P5-1 |
| bbmp:138 | Not prepared upstream (archives, MP3/Ogg/FLAC) | not in M4: sources, not UI | not in this milestone: sources, not UI |
| bbmp:149 | Files tab | M4-P4-3 | DUI-P4-3 |
| bbmp:169 | History tab | M4-P4-3 | DUI-P4-3 |
| bbmp:171 | Stats tab | M4-P4-3 | DUI-P4-3 |
| bbmp:175 | Kept marker in lists | M4-P12-7 | DUI-P5-2 |
| bbmp:183 | Saved on this computer list | M4-P4-3 | DUI-P4-3 |
| bbmp:184 | Activity panel | M4-P12-10 | DUI-P5-5 |
| bbmp:196 | Tabs 1-0 and Help tab | M4-P4-3, M4-P4-5 | DUI-P4-3, DUI-P3-3 |
| bbmp:197 | All keys remappable | M4-P12-1 | DUI-P4-14 |
| bbmp:198 | Help tab listing all keys | M4-P12-2 | DUI-P4-13 |
| bbmp:199 | Quit | M4-P4-5 | DUI-P3-3 |
| bbmp:200 | Collection switch, view switch, go to playing | M4-P4-5 | DUI-P3-3 |
| bbmp:201 | Right / left enter and leave | M4-P4-5 | DUI-P3-3 |
| bbmp:202 | Update catalogues | M4-P4-5 | DUI-P3-3 |
| bbmp:203 | Sounds wrong key with note dialog | M4-P12-9 (report storage unverified, engine side) | DUI-P5-4 (report storage unverified, engine side) |
| bbmp:204 | Activity key | M4-P12-10 | DUI-P5-5 |
| bbmp:205 | Theme preview while scrolling | M4-P12-8 | DUI-P5-3 |
| bbmp:206 | 26 themes incl. 4 Amiga | M4-P12-8 (Amiga four plus Selecta default; the rest Q21) | DUI-P5-3 (Amiga four plus Selecta default; the rest Q21) |
| bbmp:207 | Viz and meters take theme colours | M4-P12-8 | DUI-P5-3 |
| bbmp:208 | Dialogs: collections, confirm, prompt, playlist picker | M4-P12-9 | DUI-P5-4 |
| bbmp:209 | First-start flow | M4-P12-9 | DUI-P5-4 |
| bbmp:210 | Guard on destructive actions | M4-P12-9 | DUI-P5-4 |
| bbmp:211 | Visualiser area, meters, now-playing bar | M4-P4-2, M4-P3-3 | DUI-P4-1, DUI-P4-11 |
| bbmp:218 | Command plays files / folders / archives | M4-P7-5, M4-P1-3 | M4-P7-5 (Amiga), DUI-P1-3 |
| bbmp:234 | `wrong` CLI | not in M4: CLI, not UI | not in this milestone: CLI, not UI |
| bbmp:242 | Subsongs all / first only | M4-P12-1 | DUI-P4-14 |
| bbmp:246 | Browse and search scope setting | M4-P12-1 | DUI-P4-14 |
| bbmp:248 | Theme setting | M4-P12-1, M4-P12-8 | DUI-P4-14, DUI-P5-3 |
| bbmp:257 | Downloads and Extra files panels | M4-P12-10 | DUI-P5-5 |
| bbmp:258 | Saved on this computer panel | M4-P4-3 | DUI-P4-3 |
| bbmp:259 | Key remapping | M4-P12-1 | DUI-P4-14 |
| bbmp:261 | Settings saved the moment they change | M4-P12-1 | DUI-P4-14 |
| bbmp:262 | Environment variables | not in M4: host configuration, M1 | not in this milestone: host configuration, M1 |
| bbmp:287 | Testers guide | not in M4: documentation, not UI | not in this milestone: documentation, not UI |
| era:18 | Info window | M4-P4-3 | DUI-P4-3 |
| era:19 | Sample list | M4-P4-3 | DUI-P4-3 |
| era:20 | Scopes | M4-P3-3, M4-P12-6 | DUI-P4-11, DUI-P4-11 |
| era:21 | Pattern view | M4-P3-3 | DUI-P4-11 |
| era:28 | Stealth / iconify commodity | M4-P5-3, M4-P12-12 | not in this milestone: Amiga-only, stays M4-P5-3 (Amiga), M4-P12-12 (Amiga) |
| era:29 | Screen modes, pubscreen, fonts, big buttons, tooltips | M4-P7-1, M4-P12-3, M4-P12-4 | M4-P7-1 (Amiga), DUI-P5-6, DUI-P3-6 |
| era:34 | Prefs | M4-P12-1 | DUI-P4-14 |
| era:37 | Hotkeys | M4-P4-5, M4-P12-12 | DUI-P3-3, M4-P12-12 (Amiga) |
| era:45 | Eagleplayer analyzers FFT, Levelgraph, Levelmeter | M4-P12-6 | DUI-P4-11 |
| era:46 | Quadrascope, Stereoscope, SpaceScope | M4-P12-6 (SpaceScope variant unverified, added to the variant list) | DUI-P4-11 (SpaceScope variant unverified, added to the variant list) |
| era:48 | Module info engine | M4-P4-3 | DUI-P4-3 |
| era:57 | Time engine, PublicScreenSelector | M4-P12-7, M4-P7-1 | DUI-P5-2, M4-P7-1 (Amiga) |
| era:58 | Pluggable GUIs | M4-P1-1, M4-P11-1 | DUI-P1-1, M4-P11-1 (Amiga) |
| era:71 | UI niceties | M4-P12-3, M4-P12-4, M4-P12-5 | DUI-P5-6, DUI-P3-6, DUI-P5-1 |
| era:113 | Two-screen UI (Chipmachine) | not in M4: rejected by Johan (Q20) | not in this milestone: rejected by Johan (Q20) |
| era:121 | Skins | not in M4: M5 | not in this milestone: M5 |
| era:132 | Column browser | M4-P3-1 | DUI-P4-4 |
| era:133 | Cover flow | M4-P3-1 | DUI-P4-4 |
| era:134 | Grid | M4-P3-1 | DUI-P4-4 |
| era:135 | Facets | M4-P3-1 (grouping keys; engine done M3) | DUI-P4-4 (grouping keys; engine done M3) |
| cat:97 | Pattern view | M4-P3-3 | DUI-P4-11 |
| cat:98 | Sample list | M4-P4-3 | DUI-P4-3 |
| cat:99 | Scopes | M4-P3-3, M4-P12-6 | DUI-P4-11, DUI-P4-11 |
| cat:100 | Spectrum | M4-P3-3 | DUI-P4-11 |
| cat:101 | VU | M4-P3-3 | DUI-P4-11 |
| cat:102 | Note dots | M4-P3-3 | DUI-P4-11 |
| cat:103 | Phase | M4-P3-3 | DUI-P4-11 |
| cat:104 | Chip registers | M4-P3-4 | DUI-P4-12 |
| cat:105 | GPU viz | not in M4: M5 | not in this milestone: M5 |
| cat:106 | Generated art | M4-P3-5 | DUI-P2-5 |
| cat:107 | Cover art (M4) | M4-P4-2 | DUI-P4-1 |
| cat:108 | Artwork fetch | not in M4: source layer, M3; drawn by M4-P4-2 | not in this milestone: source layer, M3; drawn by DUI-P4-1 |
| cat:109 | Multiple scope windows (M4) | M4-P12-6 | DUI-P4-11 |
| cat:110 | Signal path | M4-P3-3 | DUI-P4-11 |
| cat:111 | Fullscreen visual | M4-P4-3 | DUI-P4-3 |
| cat:112 | Lyrics | M4-P4-3 | DUI-P4-3 |
| cat:119 | STIL panel | M4-P4-3 | DUI-P4-3 |
| cat:175 | OS file integration (M4) | M4-P7-2 | DUI-P5-11 (drop files on the window); AppWindow stays M4-P7-2 |
| cat:176 | File requester (M4) | M4-P7-3 | DUI-P5-11 (desktop file dialogs); asl stays M4-P7-3 |
| cat:190 | Module info / infobox | M4-P4-3, M4-P12-7 | DUI-P4-3, DUI-P5-2 |
| cat:191 | Module comment file / message (M1) | M4-P4-3 (message shown; engine done M1) | DUI-P4-3 (message shown; engine done M1) |
| cat:192 | Per-format info pane (M5) | not in M4: M5 | not in this milestone: M5 |
| cat:194 | External lookup Demozoo (M4, unhomed) | M4-P12-14 | DUI-P5-7 |
| cat:203 | Module info | M4-P4-3 | DUI-P4-3 |
| cat:206 | Hotkeys | M4-P4-5, M4-P12-12 | DUI-P3-3, M4-P12-12 (Amiga) |
| cat:207 | vi mode | M4-P12-11 | DUI-P4-13 |
| cat:231 | Switchable view styles | M4-P3-1 | DUI-P4-4 |
| cat:232 | Column browser | M4-P3-1 | DUI-P4-4 |
| cat:233 | Multiple screens (views) | M4-P4-3 | DUI-P4-3 |
| cat:234 | Configurable columns, title and status formats | M4-P12-16 | DUI-P5-9 |
| cat:235 | Skins and design tokens (M5) | tokens M4-P12-8; skins not in M4: M5 | tokens DUI-P5-3; skins not in this milestone: M5 |
| cat:236 | Mini mode (M4) | M4-P5-3 | DUI-P4-15 (mini mode); Amiga mini stays M4-P5-3 |
| cat:237 | Live filter | M4-P4-3 (Search view; engine done M3) | DUI-P4-3 (Search view; engine done M3) |
| cat:238 | Multiple UIs switchable | M4-P1-1, M4-P11-1 | DUI-P1-1, M4-P11-1 (Amiga) |
| cat:239 | Two-screen layout | not in M4: rejected by Johan (Q20) | not in this milestone: rejected by Johan (Q20) |
| cat:240 | Home screen | M4-P4-3 | DUI-P4-3 |
| cat:241 | Tag editor screen | M4-P4-3 (Tag editor view; engine done M3) | DUI-P4-3 (Tag editor view; engine done M3) |
| cat:242 | Stats, Activity, clock, outputs screens | M4-P4-3, M4-P12-10, M4-P12-7 | DUI-P4-3, DUI-P5-5, DUI-P5-2 |
| cat:243 | Public screen / own screen, screen modes (M4) | M4-P7-1, M4-P6-1 | not in this milestone: Amiga-only, stays M4-P7-1 (Amiga), M4-P6-1 (Amiga) |
| cat:244 | AHI indicator | M4-P12-7 | DUI-P5-2 |
| cat:245 | Help overlay, palette | M4-P12-2, M4-P12-11 | DUI-P4-13, DUI-P4-13 |
| cat:252 | Prefs store | M4-P12-1 (store done M1) | DUI-P4-14 (store done M1) |
| cat:255 | 68000 build | not in M4: runtime tier, later | not in this milestone: runtime tier, later |
| cat:259 | Stealth / iconify commodity (M4) | M4-P5-3, M4-P12-12 | not in this milestone: Amiga-only, stays M4-P5-3 (Amiga), M4-P12-12 (Amiga) |

### 8.3 Row counts (counted twice)

| Source | Count 1: `grep -c '^\| <src>:'` | Count 2: awk over column 2 (`awk -F'\|' '$2 ~ "^ <src>:"'`) |
|---|---|---|
| design-1a | 41 | 41 |
| ledger-F | 15 | 15 |
| stream | 25 | 25 |
| hippo | 74 | 74 |
| bbmp | 61 | 61 |
| era | 20 | 20 |
| cat | 44 | 44 |
| Total | 280 | 280 |

240 rows carry a DUI ID; 40 rows are `not in this milestone` with a reason (Amiga-only rows stay in M4, engine or loader rows stay with their milestone, GPU and skin rows stay in M5). Checklist items: 61 (counted from `- [ ] DUI-P` lines, matching the per-phase count in section 5).

## 9. M4 items that move into this milestone, and M4 sections to revise later

Moved (M4 ID to DUI ID):

| M4 ID | Item | DUI ID |
|---|---|---|
| M4-P1-1 | UI_SWITCH handled by the shell | DUI-P1-1 |
| M4-P1-2 | several UIs at once (host part) | DUI-P1-2 |
| M4-P1-3 | mode pref `ui.<plugin>.mode` (host part) | DUI-P1-3 |
| M4-P2-1 | uimodel | DUI-P1-4 |
| M4-P2-2 | lift from the text UI | DUI-P1-5 |
| M4-P2-3 | action table | DUI-P1-6 |
| M4-P2-4 | layout tree | DUI-P1-7 |
| M4-P2-5 | parity matrix | DUI-P1-8 |
| M4-P2-6 | effects governor | DUI-P1-9 |
| M4-P3-1 | engine grid, cover flow, compact styles | DUI-P4-4 |
| M4-P3-2 | style offer | DUI-P4-4 |
| M4-P3-3 | `sel_surface` | DUI-P4-11 |
| M4-P3-4 | chip register panel | DUI-P4-12 |
| M4-P3-5 | generated artwork | DUI-P2-5 |
| M4-P4-1 | plugin, rasterizer, mem backend, frame dump | DUI-P2-1, DUI-P2-6, DUI-P4-1 |
| M4-P4-2 | full layout | DUI-P4-1 |
| M4-P4-3 | all views | DUI-P4-2 to DUI-P4-15 |
| M4-P4-4 | mouse | DUI-P3-2 |
| M4-P4-5 | keyboard | DUI-P3-3 |
| M4-P12-1 | settings | DUI-P4-14 |
| M4-P12-2 | about and help | DUI-P4-13 |
| M4-P12-3 | fonts (desktop part; the Amiga part stays an open M4 item) | DUI-P5-6 |
| M4-P12-4 | tooltips | DUI-P3-6 |
| M4-P12-5 | remembered state | DUI-P5-1 |
| M4-P12-6 | viz windows | DUI-P4-11 |
| M4-P12-7 | info line | DUI-P5-2 |
| M4-P12-8 | themes | DUI-P5-3 |
| M4-P12-9 | dialogs | DUI-P5-4 |
| M4-P12-10 | activity | DUI-P5-5 |
| M4-P12-11 | command line and palette | DUI-P4-13 |
| M4-P12-14 | lookup | DUI-P5-7 |
| M4-P12-15 | F-keys | DUI-P5-8 |
| M4-P12-16 | columns and formats | DUI-P5-9 |

Stay in M4: P5 (RTG backend), P6 (AGA backend), P7-1 to P7-5 (pubscreen, AppWindow and AppIcon, asl, screen switch, ToolTypes), P10, P11, P12-12 (commodity). M4 keeps the Amiga parts of P1-2, P1-3, P4-1 (Amiga input, Intuition window) and P12-3 (build-time 1-bit and 4-bit atlases for scale 1, using the DUI-P2-3 builder).

M4 sections to revise after this plan is accepted (list only, not edited here):
- M4 section 3 (phases): remove the moved IDs, make P4 and P12 depend on M3d.
- M4 section 4 (architecture of the custom UI): point at `src/gui/`; keep only backends and Amiga input.
- M4 section 6 (tests): goldens come from the M3d harness; INDEX8 goldens are shared.
- M4 section 7.2 and section 9 coverage: remap moved rows to DUI IDs.
- M4 section 8a answers Q13 (font atlases: one builder, DUI-P2-3) and Q21 (theme presets live in `src/gui/theme/`).
- Design doc section 14 milestone list (section 6 of this plan).
