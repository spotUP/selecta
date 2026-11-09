---
date: 2026-11-09
author: Claude (Opus design agent, I1222a)
status: proposed
ledger: I1222a (design), I1222b..I1222i (build)
topic: Text UI frame mirrors the desktop frame
---

# Text UI frame: mirror the desktop frame (I1222a)

Johan 2026-11-09: "the text ui still doesnt use the same layout as the desktop ui's". He picked "Mirror desktop frame". The desktop UI is the reference. This document maps every desktop frame region to a text region, names the layout data that moves into shared uimodel code, and splits the build into sonnet-sized ledger rows.

## 1. Current state

### Desktop frame (reference)

The shared layout (`src/uimodel/layout.c`, `sel_uim_layout_compute_panel`) builds a tree of named regions. Breakpoints are in pixels.

| Region | Desktop source | Content |
|---|---|---|
| `sidebar` | `draw_sidebar` (desktop_ui.c ~12230) | Brand logo plus "Selecta" (sidebar wider than 120 px); door rows (40 px each, `desk_door_shown` over `sel_uim_door_shown_in`); "Your Library" header band (36 px) with Create Playlist `+` and Expand/Collapse chevron (`side_lib_header_rect`, `side_hdr_rect`); filter chip band (`side_chips_rect`, labels `sel_uim_lfilter_label(1..6)`; bitmap tier shows one cycle label); library list (`side_lib_draw`) with the I1183 keyboard cursor. Runs from the window top. |
| `topbar`, `topbar-home`, `topbar-field`, `topbar-browse` | `draw_topbar` (~10507) | Home button, "What do you want to play?" field, Browse button, Now Playing panel toggle (flips `detail_hidden`). Spans main plus detail (I972b). 56 px. |
| `main` | page draw functions | Current page: Home, Search, Browse Folders, Playlists, Queue (`draw_queue`), Song Information (`draw_songinfo` ~7393), and so on. |
| `detail` | `draw_nowplaying` (~12370) | Now Playing panel: context header (`st->ctx_name`, else panel "Release"), cover art, title, artist, catalogue pairs (`NP_PAIRS_MAX` 6), lyrics, Up Next (`NP_UPNEXT_MAX` 3), like and rating. 320 px. Three-column breakpoint only. |
| `player` | `draw_player_left` (~12311), `draw_transport` (~12646), seek row, volume | Cover plus title and artist (hidden under 900 px); subsong "n/m" with minus and plus, shuffle, previous, play/pause, next, repeat; clock, seek bar, length; volume icon, bar, AirPlay device button; status text at the right (skipped in the one-row footer). 72 px, 40 px one-row form at window height 600 or less. |
| `suggestion` | mini breakpoint | Mini player only, below 700 px. |

Breakpoints: three-column 1100 px and up, two-column 900 and up, icon sidebar 700 and up, mini below 700. Focus stops are the regions in layout order (`region_stop`, ~900, DUI-P3-3).

### Text UI today (`plugins/ui/text/text_ui.c`)

| Rows | Content |
|---|---|
| 0-1 | `tui_header`: play word, title, author; format, subsong, clock, volume, shuffle, repeat. Built inline, not from `sel_uim_frame`. |
| 2 | `tui_pane` title |
| 3 .. rows-2 | Full-width list (x 0 or 1), or `tui_styled_rows` (grid, cover flow, compact) at hardcoded y=3. Optional `tui_info` right panel (`TUI_INFO_COLS` 30, all META tags) when `u->info`, layout FULL and cols > 50. |
| rows-1 | Key hints, or the command line (`:`, `/`, confirm prompt). |

No sidebar, no doors, no library list, no top bar, no player bar at the bottom. `tui_main_rows` = rows - 4 (`TUI_CHROME_ROWS`). The text UI does not call `sel_uim_layout_compute*` or `sel_uim_update`.

Gap: every desktop region except `main` is missing or in another place. The fix is not a text-only layout. The text UI must consume the same region tree as the desktop and MUI, computed in cells.

## 2. Region map

Text regions use the same names as the desktop regions, so focus order, hit testing (`sel_uim_hit`) and the layout check (`sel_uim_layout_check`) are shared. One text-only region is added: `hints` (1 row at the bottom, key hints and the command line). It has no desktop equivalent; the desktop uses tooltips and menus.

| Desktop region | Text region | Text content |
|---|---|---|
| `sidebar` | `sidebar` | Row 0: brand "Selecta" (TXT_ROLE accent). Door rows, 1 row each, same `sel_uim_door_shown_in` rule (with top bar and labelled sidebar: Playlists, Queue, Now Playing, Song Information). "Your Library" header row with `[+]` and expand control (`>>` / `<<`). Chip rows from the shared chip flow (labels 1..6: Playlists, Albums, Artists, Composers, Sources, Local Files), wrapped like I1221. Tools row (search, sort, view) when the stack has room. Library list rows to the bottom, with the cursor. |
| sidebar, icon rail | `sidebar` (rail) | 3 columns: one letter per door (H, S, F, P, Q, N, I) with selection role, no header, no chips, no list. Same rule as the desktop icon rail. |
| `topbar` | `topbar` | 1 row: `[Home]` `[What do you want to play?]` `[Browse]` `[Panel]`. Spans main plus detail, like I972b. The field opens the `/` search. `[Panel]` flips `detail_hidden`. |
| `main` | `main` | Current page. Existing text pages (list, grid, cover flow, compact, Song Information, Queue) draw into the `main` rect: origin `main.x`, `main.y`, size `main.w` x `main.h`. Pane title is the first row of `main`. |
| `detail` | `detail` | Now Playing panel from shared `sel_uim_np_lines`: context header, title, artist, up to 6 catalogue pairs, current lyric line, Up Next (3), like and rating. Cover art: text exemption (no image in cells; RULES exemption written into I1222f). Vertical separator in its first column. |
| `player` | `player` | From `sel_uim_frame`. Two-row form: row 1 = title - artist (left), transport glyphs `|<  >  >|` plus shuffle, repeat, subsong `n/m` (centre), volume bar (right). Row 2 = clock, seek bar, length, status. One-row form: title - artist, play glyph, clock/length, volume; no status (same rule as the desktop one-row footer). |
| `suggestion` | `suggestion` | Mini layout: player bar only (`SEL_UIM_LAYOUT_MINI` becomes the player rows plus hints). |
| (none) | `hints` | 1 row: key hints, or the command line and confirm prompt. Text only. |

Separators: a vertical line `│` belongs to the last column of the region on its left (sidebar, main when detail is shown). Horizontal edges use background roles, not line rows, so no row is lost. The existing `tui_info` META panel is retired; its content lives on the Song Information page (main), which is where the desktop shows it.

## 3. Sizes

Proposed text metrics (cells):

| Metric | Text value | Desktop value (px) |
|---|---|---|
| Three-column breakpoint | 120 cols | 1100 |
| Two-column breakpoint | 80 cols | 900 |
| Icon rail breakpoint | 60 cols | 700 |
| Mini below | 60 cols | 700 |
| Sidebar, three-column | 26 | 220 (`SIDEBAR_WIDE`) |
| Sidebar, two-column | 20 | 200 (`SIDEBAR_NARROW`) |
| Sidebar, icon rail | 3 | 56 (`SIDEBAR_ICONS`) |
| Sidebar, expanded | about 40% of cols (cap so main keeps its minimum) | 360 / 320 / 220 |
| Detail | 34 | 320 (`DETAIL_W`) |
| Main minimum | 40 x 8 | 240 x 120 |
| Top bar | 1 row, shown when rows >= player + hints + 1 + 8 | 56 |
| Player bar | 2 rows; 1 row when rows <= 25 | 72; 40 at height <= 600 |
| Hints | 1 row | none |

Region sizes per terminal size (columns are cells, heights are rows):

| Terminal | Breakpoint | Sidebar | Main | Detail | Top bar | Player | Hints | Main height |
|---|---|---|---|---|---|---|---|---|
| 160 x 50 | three-column | 26 | 100 | 34 | 1 | 2 | 1 | 46 |
| 120 x 40 | three-column | 26 | 60 | 34 | 1 | 2 | 1 | 36 |
| 120 x 40, detail hidden | three-column | 26 | 94 | 0 | 1 | 2 | 1 | 36 |
| 120 x 40, expanded | three-column | 48 | 40 (min) | 32 (shrinks, or hides when main would go under 40) | 1 | 2 | 1 | 36 |
| 80 x 25 (m2 tests) | two-column | 20 | 60 | none | 1 | 1 | 1 | 22 |
| 80 x 24 | two-column | 20 | 60 | none | 1 | 1 | 1 | 21 |
| Amiga 80 x 32 (unverified) | two-column | 20 | 60 | none | 1 | 2 | 1 | 28 |
| 60 x 24 | icon rail | 3 | 57 | none | 1 | 1 | 1 | 21 |
| 50 x 20 | mini | none | none | none | none | 1 to 2 | 1 | none |

Sidebar height is the full terminal height minus player and hints (it runs from row 0, as on the desktop). Top bar and main sit to the right of the sidebar.

At 80 columns the layout is two-column, like the desktop at 900 to 1099 px: there is no detail column. The Now Playing door opens the Now Playing page in `main`, which draws the same `sel_uim_np_lines`.

Amiga text screen: the m2 tests use 80x25 and that size is covered. The UP-Term window size on a PAL screen (640x256 with an 8 px font gives 80 columns and about 30 to 32 rows inside a window) is NOT verified; I could not find the default in the repo. I1222b must read the real size on the rig (`amiga_text` / window size) and add that size to the reachability test.

## 4. Shared uimodel code and API

Rule: the text UI must not compute geometry, door visibility, chip wrapping or Now Playing panel content itself. All of it moves to uimodel, where desktop and MUI already read it or will read it.

### 4.1 Per-tier metrics and a text tier (`src/uimodel/layout.c`, `include/selecta/uimodel.h`)

Today the constants are `#define`s used directly in `sel_uim_layout_compute_panel`. Replace them with a per-tier table:

```c
typedef struct sel_uim_metrics {
    int32_t bp_three, bp_two, bp_icons;           /* breakpoint thresholds */
    int32_t side_wide, side_narrow, side_icons;   /* sidebar per breakpoint */
    int32_t side_exp_wide, side_exp_narrow, side_exp_icons; /* expanded */
    int32_t side_exp_pct;                         /* 0 = fixed widths; text = 40 */
    int32_t detail_w, main_min_w, main_min_h;
    int32_t topbar_h, topbar_button, topbar_field_max, topbar_gap, topbar_gutter;
    int32_t bar_h, bar_one_row_h, one_row_max_h;
    int32_t banner_h;                             /* suggestion banner */
    int32_t hints_h;                              /* 0 on desktop and MUI, 1 on text */
} sel_uim_metrics;

enum { SEL_UIM_TIER_DEFAULT = 0, SEL_UIM_TIER_BITMAP = 1, SEL_UIM_TIER_TEXT = 2 };

const sel_uim_metrics* sel_uim_metrics_for(uint32_t tier);

/* cols x rows in cells; same region names; adds "hints" */
int sel_uim_layout_compute_text(sel_uim_layout* l, int32_t cols, int32_t rows,
                                int expand, int detail_hidden);
```

`compute_panel`, `topbar_compute`, `expanded_side` and `sel_uim_layout_set_side_width` read the metrics for the tier. Default and bitmap tables hold today's values exactly, so desktop and MUI output stays byte-identical (proven by the existing dui goldens and `tests/ui/mui_contract_test.c`, which call the compute functions). `sel_uim_layout_check` gains the `hints` region in its tiling check.

### 4.2 Chip flow (`src/uimodel/libfilter.c`)

I1221 (branch `worktree-agent-a1cfdfc0032f10be6`, 67b62c9f, not merged) wraps the chips inside `desktop_ui.c` (`side_chip_place`, `side_chip_rows`, `side_chip_pitch`). Row I1221 asks for the rule in uimodel; it is not there. Extract it:

```c
/* Flow-wrap n chips of widths w into a band of width band with gap between
   chips. Writes each chip's x and row; returns the row count (>= 1 when n > 0). */
int32_t sel_uim_chips_flow(const int32_t* w, uint32_t n, int32_t gap, int32_t band,
                           int32_t* x, int32_t* row);
```

Units are the caller's (px on desktop, cells on text, MUI uses its own font widths). The desktop calls it in place of `side_chip_place` after I1221 merges. The By You sub-row (I678d) stays a caller concern.

### 4.3 Sidebar stack (`src/uimodel/sidestack.c`, new file next to doors.c)

Order and visibility of the sidebar sections, shared by desktop, MUI and text:

```c
typedef enum { SEL_UIM_SIDE_BRAND, SEL_UIM_SIDE_DOOR, SEL_UIM_SIDE_LIBHDR,
               SEL_UIM_SIDE_CHIPS, SEL_UIM_SIDE_TOOLS, SEL_UIM_SIDE_LIST } sel_uim_side_kind;

typedef struct { sel_uim_side_kind kind; int32_t y, h; int32_t door; } sel_uim_side_item;

typedef struct {
    int32_t brand_h, door_h, libhdr_h, chip_row_h, tools_h, brand_min_w;
    int rail;            /* icon rail: doors only */
    int top_bar;         /* top bar shown: Home and Search leave the sidebar */
    int32_t chip_rows;   /* from sel_uim_chips_flow */
} sel_uim_side_spec;

/* Fills items top to bottom inside height h; LIST takes the rest. Returns count. */
uint32_t sel_uim_side_stack(const sel_uim_side_spec* s, int32_t w, int32_t h,
                            sel_uim_side_item* out, uint32_t max);
```

Door items come from `sel_uim_door_shown_in` / `sel_uim_door_slot`. The brand row drops when `w < brand_min_w` or when the list would get fewer than 3 rows (decision 4). Text spec: all heights 1, `brand_min_w` 12. Desktop spec: 40 / 36 / chip pitch, `brand_min_w` 120. Moving desktop `draw_sidebar` onto the stack is a separate later row so this work does not touch desktop drawing.

### 4.4 Now Playing panel lines (`src/uimodel/nppanel.c`, new)

`draw_nowplaying` builds its content inline. Extract the content (not the drawing):

```c
typedef enum { SEL_UIM_NP_CONTEXT, SEL_UIM_NP_TITLE, SEL_UIM_NP_ARTIST, SEL_UIM_NP_PAIR,
               SEL_UIM_NP_LYRIC, SEL_UIM_NP_UPNEXT_HDR, SEL_UIM_NP_UPNEXT,
               SEL_UIM_NP_LIKE } sel_uim_np_kind;

typedef struct { sel_uim_np_kind kind; char key[32]; char val[256]; } sel_uim_np_line;

/* From the core state the desktop uses today (context name, current song meta,
   catalogue pairs up to 6, lyric slot, queue up next up to 3, like and rating). */
uint32_t sel_uim_np_lines(const sel_uim* m, sel_uim_np_line* out, uint32_t max);
```

Desktop `draw_nowplaying` switches to it (same pixels; dui golden unchanged). Text draws it in `detail`, and on the Now Playing page at two-column sizes.

### 4.5 Player bar

The text UI adopts `sel_uim` / `sel_uim_update` / `sel_uim_frame` (title, artist, format, clock, remaining, length, status, play, can_prev, can_next, no_seek, progress_q16, volume, subsong, subsong_count, repeat_mode, random_on). The inline header code in `tui_header` is removed. No new uimodel API.

## 5. Build rows (proposed ledger rows)

Order: c, then d, e, f in any order (d after I1221 merges), then g, h, i, then b last. Each row is sonnet-sized. Every row builds the Amiga text UI (`amiga-68020` preset) on the host; the rig check is one run in I1222b (RULES 4a, rig test at end).

| ID | Scope | Files | Tests |
|---|---|---|---|
| I1222c | Per-tier metrics table in layout.c; `SEL_UIM_TIER_TEXT`; `sel_uim_layout_compute_text`; `hints` region; `sel_uim_layout_check` covers it. Desktop and MUI values unchanged. | src/uimodel/layout.c, include/selecta/uimodel.h | New tests/uimodel/layout_text_test.c: region rects at 160x50, 120x40 (normal, detail hidden, expanded), 80x25, 80x24, 80x32, 60x24, 50x20 equal the section 3 table; `sel_uim_layout_check` passes at every size from 40x12 to 200x60. Existing dui goldens and mui_contract_test unchanged (byte-identical proof for default and bitmap tiers). |
| I1222d | Extract `sel_uim_chips_flow` from I1221's `side_chip_place`; desktop sidebar calls it. Starts after I1221 is on main. | src/uimodel/libfilter.c, include/selecta/uimodel/libfilter.h (or uimodel.h), plugins/ui/desktop/desktop_ui.c | tests/uimodel/libfilter_flow_test.c: one row when all fit, wrap at exact fit plus 1, single chip wider than the band, n = 0. I1221 desktop chip-wrap golden unchanged. |
| I1222e | `sel_uim_side_stack` (brand, doors, library header, chips, tools, list) over `sel_uim_door_shown_in` / `sel_uim_door_slot`. | src/uimodel/sidestack.c (new), header, CMake source list | tests/uimodel/sidestack_test.c: item order and heights for text spec and desktop spec; rail gives doors only; top bar removes Home and Search; brand drops under `brand_min_w` and when the list would get fewer than 3 rows; LIST takes the rest. |
| I1222f | `sel_uim_np_lines` from `draw_nowplaying`; desktop draws from it. Writes the cover art text exemption into RULES.md exemptions. | src/uimodel/nppanel.c (new), header, plugins/ui/desktop/desktop_ui.c, RULES.md | tests/uimodel/nppanel_test.c: lines for a song with context, 6 pairs, lyric, 3 up next, like; for an empty queue; for no song. Desktop Now Playing dui golden unchanged. |
| I1222g | Text player bar from `sel_uim_frame` (2-row and 1-row forms) in `player`; top bar row in `topbar`; `hints` row; `tui_header` removed; `tui_draw` computes the layout with `sel_uim_layout_compute_text` every frame. | plugins/ui/text/text_ui.c | tests/ui/text_ui_test.c: screen dump at 120x40 and 80x25 shows title, transport glyphs, clock/length in the player rows and the top bar labels; 1-row form at 80x24 has no status. Existing tests/m3 text dumps re-blessed in this row (rows moved). |
| I1222h | Text sidebar: draw the `sel_uim_side_stack` items (brand, doors, library header, chips from `sel_uim_chips_flow`, list with cursor), icon rail at 60 cols. Library list reuses the existing pane list data. | plugins/ui/text/text_ui.c | tests/ui/text_ui_test.c: at 120x40 the sidebar shows doors Playlists, Queue, Now Playing, Song Information, "Your Library" and the chip labels; at 60x24 only letters. If I1222d is not merged yet, chips render one per row and the row notes it. |
| I1222i | Main and detail made region-relative: `tui_pane`, `tui_styled_rows`, Song Information, Queue use `main.x/y/w/h` (no hardcoded y=3, x=0); `detail` draws `sel_uim_np_lines`; Now Playing page at two-column sizes; `tui_info` retired; region focus key (decision 1) cycles regions in layout order. | plugins/ui/text/text_ui.c, src/ui/keybind.c (focus key) | tests/ui/text_ui_test.c: grid and cover flow start at `main.y`; detail shows title and Up Next at 120x40 and is absent at 80x25; focus key moves focus sidebar, main, detail, player and back. |
| I1222b (re-scoped) | Assembly and reachability: one test drives `selecta --ui text --size 80x25`, `120x40` and `160x50` with `--screen-dump`, asserts sidebar, top bar, main, detail (120 and 160 only), player and hints at the section 3 cells, and asserts a sentinel stat (for example `text_frame_regions=6` from `sel_uim_layout_compute_text`) proving the shared layout ran. Re-bless remaining goldens; T55 (m2 vamos hash equals desktop console frame at 80x25) still passes. Amiga: build plus one rig run at the end of the batch reading the real UP-Term size; add that size to the test. | tests/ui (new cmake test), goldens | The one reachability test for the feature. Rig check: Amiga text UI shows sidebar, main and player bar at the real window size (name the platform in the test ask). |
| later (optional) | Desktop `draw_sidebar` geometry moves onto `sel_uim_side_stack`, and MUI sidebar after it. | desktop_ui.c, MUI | dui goldens unchanged. |

## 6. Open decisions for Johan

1. **Region focus key.** Tab is bound to `@switch_view` (keybind.c:141). Options: F6 (Recommended: standard pane-cycle key, free), Ctrl+W, or move `@switch_view` off Tab and give Tab to region focus (matches the desktop, breaks existing muscle memory).
2. **Hint row.** Keep a 1-row hint and command line at the bottom (Recommended: the command line needs a row anyway), or show hints only on `?` and give the row to main.
3. **80 columns.** Two-column with no right panel, Now Playing as a main page (Recommended: same as desktop 900 to 1099 px; main keeps 60 cols), or squeeze three columns (sidebar 20, main 30, detail 30; main under its minimum).
4. **Brand row.** Show "Selecta" at the top of the sidebar always, or drop it at short heights when the list would get fewer than 3 rows (Recommended).
5. **Three-column breakpoint.** 120 cols (Recommended: gives main 60 next to a 34 detail), or 100 cols (main 40).
6. **Right panel content.** Johan named the right panel as "Song Information, queue". On the desktop the right panel is Now Playing (pairs plus Up Next) and Song Information and Queue are main pages. Recommended: mirror the desktop exactly; Song Information and Queue stay doors that open in main.

## 7. Johan's answers (2026-11-09)

1. Region focus key: **Tab**. Move `@switch_view` off Tab (pick a free key, document it in keybind.c and the help page).
2. Hint row: **none**. Johan: "i am not sure textmode needs hints at all try without first". No hint row; the command line opens on demand (`:` or `/`) over the bottom row of main. Drop the "hints" region from I1222c and I1222g.
3. 80 columns: **squeeze three columns** (sidebar 20, main 30, detail 30). Main may go below its desktop minimum at 80 cols; truncate lists, do not drop the right panel.
4. Brand row: not answered; default drop at short heights (fewer than 3 list rows).
5. Three-column breakpoint: follows answer 3; three columns at every width from 80 up when the right panel is open.
6. Right panel: **mirror the desktop** (Now Playing pairs plus Up Next). Song Information and Queue stay pages in main.
