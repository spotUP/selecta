---
status: decided
row: I1079
date: 2026-11-05
author: Opus design agent
---

# I1079: row text takes its column's real share of the row

Johan 2026-11-04: "both columns are truncated early" (podcast episode list), "this is very common throughout the app".
Shot: title cut at ~196 pt, second column at ~112 pt, ~330 pt of the row empty (canvas ~703 pt wide main area).

## 1. Violated invariant and root cause

**Invariant:** a text box in a list row or card is as wide as its column's share of the *real* row width, where
a column that has no content in the list takes no share and no gap, fixed columns take only the width they need,
and the text elides only at that edge.

**Level of the root cause: column geometry in the desktop plugin, not the text layer.**
The elide itself is right: `draw_text` (`plugins/ui/desktop/desktop_ui.c:957`) passes `box.w` as the max width to
`sel_gui_text_draw` (`src/gui/text/gui_text.c:140-160`), so text elides exactly at its box edge. The boxes are wrong:
every view computes its own column split from hard-coded fractions and reserved fixed slots, with no shared rule
and no notion of an empty column.

**The podcast row (proved by the numbers).** A podcast source page is a collection page, drawn by
`draw_playlist_page` (`desktop_ui.c:5819`):
- `pls_cols_w` (`desktop_ui.c:5791-5794`): `r.w - lpx(56) - lpx(180)`: the 180 pt Plays slot is reserved on
  every collection page, also when no row has plays (podcasts).
- `pls_title_w` (`:5796-5798`): title = 45 % of that; `pls_link_rects` (`:5801-5817`): artist = 28 %, album = rest,
  each link capped at column minus 8.
- Title drawn at `:5946` (`rect_of(r.x + 56, r.y, pls_title_w(d, r), r.h)`), artist at `:5951`, album at `:5955`.
- With main ~703 pt: cols = 703 - 56 - 180 = 467; title 45 % = 210 (shot: ~196 + ellipsis), artist 28 % = 131 - 8
  = 123 (shot: ~112 + ellipsis). The album column (~126 pt) is empty for every episode and the Plays slot (180 pt)
  is empty: 126 + 180 + pads = the ~330 pt Johan sees. Exact match.

**How I counted the fixed / ad hoc widths.** Script
`/private/tmp/claude-501/-Users-spot-Code-MusicPlayer/2461f1e9-e786-4357-9e14-d1f8454d00b2/scratchpad/textw.py`
walks `desktop_ui.c`, attributes every call of `draw_text`, `draw_text_centered[_pad]`, `sel_gui_text_draw` and
`sel_gui_text_wrap` to its enclosing `static` function, extracts the box width argument and classifies it.
Result: **188 text draw calls** in 54 functions:
- 13 literal fixed widths (`lpx(d, N)`): `5895, 5944, 5969, 5974, 6000` (collection page: number 36, Played 156,
  Plays 108, count 156), `7012-7015, 7032` (tracker page), `9764` (settings), `12209` (transport), `1488` (text sheet).
- 12 hard-coded ratios: `5582-5583` (playlist find: 1/2 + 1/2), `7033, 7036` (tracker 1/2), `8415, 8420, 8422,
  8448, 8450` (library tree: name 2/3, count 1/3), `11720` (sidebar compact), `12924-12925` (overlay 1/2).
- Hidden ratios inside helpers (counted from the helpers, not the call): `pls_title_w` 45 % / `pls_link_rects` 28 %
  + rest + reserved 180 (`:5791-5817`); `songs_header_col` 3/4 + 1/4 (`:1881-1886`, used at `:2001, :2003`);
  side library table 52 / 68 / 84 % (`:11666, :11738`, used at `:11667-11670, :11743-11746`); song info label
  160 fixed (`si_value_w`, `:6530`).
- 73 "parent minus constant" (full row/panel width minus pads: these reach the edge, fine), 59 other (mostly
  `ar.w` card widths, `nx.x - r.x` "up to the next control": fine), 31 precomputed rects.

So the defect class is the **row tables with more than one text column**: 6 split rules in 6 places, none of
which drops an empty column or a reserved slot.

## 2. Reuse first (what I searched)

- `src/uimodel/*.c`, `include/selecta/uimodel.h`, `include/selecta/uimodel/*.h`: grep `elide|ellips|truncat|weight|
  flex|split|column|col_w`. Only hits: breakpoint names (`layout.c`), `"..."` in `display.c`. **No column helper.**
- `src/gui`, `include/selecta/gui`: the single elide path `sel_gui_text_draw` / `ellipsis_of` (`gui_text.c:140`).
  Correct; reused unchanged.
- `plugins/ui/mui`: **`mui_aa_list_col_widths`** (`plugins/ui/mui/map/mui_aa.c:777`): weights + fixed sample
  widths, called by the AAList draw (`mui_aalist_class.c:172`), weights from `mui_columns_weights`
  (`map/mui_list.c:152`), unit-tested in `tests/ui/mui_aa_test.c:926-931`. This is the closest existing case and
  the right algorithm family. It lacks: minimum widths, absent columns, exact remainder handling.
- Desktop dump: `plsong i x y w h file= title=` (`desktop_ui.c:7683`), `row i x y w h` (`:7251`), label ink stats
  (`d->labels`, `:979`). The reachability dump extends `plsong`, not a new format.

**Decision:** lift `mui_aa_list_col_widths` into `src/uimodel` as the one helper (extended), and make the MUI
function a thin wrapper over it. One splitter for desktop and MUI. `mui_map` and the desktop plugin already link
`selecta_uimodel` (`plugins/ui/mui/CMakeLists.txt:22`, `plugins/ui/desktop/CMakeLists.txt:135`).

## 3. Design

### 3.1 Helper (pure C99, no allocation, `src/uimodel/cols.c`, `include/selecta/uimodel/cols.h`)

```c
typedef struct {
    int32_t weight; /* > 0: a flex column, takes this share of the room left after fixed columns */
    int32_t fixed;  /* > 0 (weight 0): a fixed column, exactly this wide (measured: "999 plays", "Format", count) */
    int32_t min_w;  /* flex only: never narrower than this while the row allows it */
    int32_t max_w;  /* flex only, 0 = none: the widest content; room past it goes to the other flex columns */
} sel_uim_col;      /* weight 0 and fixed 0: the column is absent: width 0, no gap */

/* Splits row_w into n columns left to right with `gap` between present columns. Writes x (relative to the row) and
 * w per column; the last present column ends exactly at row_w. Returns the number of present columns. */
uint32_t sel_uim_cols_split(int32_t row_w, int32_t gap, const sel_uim_col* c, uint32_t n, int32_t* x, int32_t* w);
uint32_t sel_uim_cols_calls(void); /* sentinel for the reachability test */
```

Algorithm: present = weight > 0 or fixed > 0. room = row_w - fixed sum - gaps between present. Flex columns share
room by weight; a column under `min_w` is pinned there and the rest re-shared (at most n passes, n <= 8); a column
over `max_w` (when set) is pinned at max and its surplus re-shared. If room < sum of minimums, minimums shrink
proportionally (never negative). Integer remainder goes to the last flex column so the row is filled to the pixel.

### 3.2 Who calls it (one spec per view, no per-view arithmetic)

Each view keeps one `static const sel_uim_col` table (the weights are design data) and flips a column to absent
when the list has no content for it (empty for every row of the list, decided when the list loads, cached with the
list version, so the header does not jump while scrolling):

| View | Columns (weight / fixed) | Absent when |
|---|---|---|
| Collection page `draw_playlist_page` (playlists, album, artist, composer, source pages incl. Podcasts, Liked, Recent) | lead fixed 56, Title 45, Artist 28, Album 27, Plays/Played fixed (measured) | Artist / Album empty in every row; Plays when no row has plays |
| Songs table `draw_songs` | Title 3, Format fixed (measured widest label) | - |
| Library tree `draw_library` | Name flex (min 120), Count fixed (measured "N songs" / "Folder") | Count on leaf-only lists |
| Side library table `side_lib_draw` | Title 52, then the three meta columns 16 each | per content |
| Playlist find `draw_playlist_find` | Title 1, Artist 1, Add fixed | Artist empty |
| Song info `si_draw_row` | Label fixed 160, Value flex | - (already fills; moves to the helper for one rule) |

Podcast result at 703 pt: Title 45/(45+28) of 647 = 399 pt, second column 248 pt, ending at the row edge.

Out of scope (already reach their edge or are fixed by design): search rows and online results (full width minus
badge), queue rows (up to the next control), cards and shelves (card width is the design), detail pane, now
playing page, tracker page channel grid, settings controls.

### 3.3 Spotify reference (Mac layout is the truth)

Spotify's tables keep stable proportional columns (# / Title / Album / Date added / Duration) and drop a column a
list does not have (podcast episode lists have no Album column). Matches 3.2: proportions stay, absent columns go,
fixed numeric columns are as wide as their content.

### 3.4 Amiga MUI

`mui_aa_list_col_widths` (`mui_aa.c:777`) becomes a wrapper: builds `sel_uim_col` from `weight[]` / `fixed[]`,
gap = `2 * m->gap + 1`, row = `total - 2 * m->pad_x`, calls `sel_uim_cols_split`. Existing outputs for the cases
in `mui_aa_test.c:926-931` must not change (equal-weight, fixed-sample and too-narrow cases). The MUI main list
then also drops a column that is empty for the whole list (same absent rule, filled in `mui_main_cols_fill`,
`mui_ui.c:1367`). Rig check: one run at the end of the batch (MUI main list showing a list with an empty album
column; columns fill the row). Whether the MUI shows podcast episodes in the main list is **unverified**
(grep of `mui_ui.c` for podcast/episode: no hits).

## 4. Audit

| View | Function / line | Current width rule | Truncates early |
|---|---|---|---|
| Podcasts (source page) | `draw_playlist_page` `:5946-5955` via `pls_cols_w` `:5791` | 45/28/rest of row - 56 - 180, always | **yes** (the shot) |
| Source pages (Modland, AMP, ...) | same | same | **yes** when album empty or no plays (180 pt lost always) |
| Playlists, album/artist/composer, Liked, Recent pages | same | same | **yes** (Plays slot reserved 180 on playlists with no Plays column) |
| Library songs table | `draw_songs` `:2001-2003`, `songs_header_col` `:1881` | Title 3/4, Format 1/4 of main | **yes** (Format needs ~110 pt, gets ~1/4) |
| Library tree (composers/folders) | `draw_library` `:8415-8450` | name 2/3, count 1/3 | **yes** ("12 songs" needs ~60 pt, gets 1/3) |
| Side library table (expanded sidebar) | `side_lib_draw` `:11666-11670, :11738-11746` | 52/16/16/16 % | **yes** when meta columns short or empty |
| Playlist find (add songs) | `draw_playlist_find` `:5582-5583` | title 1/2, artist 1/2 - Add | **yes** when artist empty |
| Search sections (songs, online) | `draw_search` `:9630`, `online_draw_results` `:9205` | full width - badge | no |
| Queue | `draw_queue` `:4486-4492` | up to the next control | no |
| Playlists list | `draw_playlists` | full width - pads | no |
| Detail pane | `draw_detail` `:1666` | pane - 2 * spacing | no |
| Home cards / shelves | `draw_home_content` `:10041`, `draw_shelves` `:3728` | card width | no (card is the design) |
| Song info | `si_draw_row` `:6572-6577` | label 160 fixed, value rest | no |

## 5. Implementation rows (one sonnet agent each; build + host tests per row, one rig run at the end)

- **I1079a Helper + unit tests.** `include/selecta/uimodel/cols.h`, `src/uimodel/cols.c`, add to
  `src/uimodel/CMakeLists.txt`; test `tests/uimodel/cols_test.c` (ctest `uim-cols`): weights split exactly to
  row_w; absent column takes 0 and no gap; fixed column exact; min pins and re-shares; max_w surplus re-shared;
  room < minimums shrinks proportionally, no negative; remainder lands on last flex; calls counter increments.
- **I1079b MUI on the helper.** `mui_aa_list_col_widths` wraps `sel_uim_cols_split`; `mui_aa_test.c:926-931`
  unchanged and passing; absent-column rule in `mui_main_cols_fill`. Build `build/amiga`. Rig check deferred to
  the batch end.
- **I1079c Collection page (the podcast fix) + the ONE reachability test.** Replace `pls_cols_w`,
  `pls_title_w`, the column part of `pls_link_rects` and the Plays/Played fixed rects with one spec table +
  `sel_uim_cols_split`; absent flags computed when `pls_meta` / `col_plays` load (cached per `pls_ver`). Extend the
  `plsong` dump (`:7683`) with `cols=<x>:<w>,...` (row-relative), `row_w=`, and per text box `tw=<text width>
  elided=0|1`; add `cols_calls=<sel_uim_cols_calls()>` to the stats line. Test `dui-row-text-widths`
  (`tests/dui/row_text_widths.cmake`, through the `selecta` binary, `--ui desktop --backend mem --canvas 1280x800
  --stats`): open a collection whose songs have title + artist and no album and no plays (the podcast shape; use
  the podcast source fed by the `tests/net/podcast.cmake` local feed if it can run under dui, else a playlist
  fixture with album empty: agent states which). Assert: last present column x+w == row_w (row edge); title
  w >= 0.6 * (row_w - lead); no `elided=1` on a box whose `tw` <= its column w; `cols_calls` > 0 and grows
  across a resize. Same test at 900x700 for the second breakpoint. Regression: fails on current main (title w =
  0.45 * (row_w - 236)).
- **I1079d Songs table + library tree.** `songs_header_col` and the library tree name/count on the helper, fixed
  columns measured (widest Format label, widest count text). Behaviour tests where they live: extend the existing
  dump-based dui test of each view (agent names it) with the column x/w; no second reachability test.
- **I1079e Side library table + playlist find + song info.** Same pattern; `side_lib_draw` both sites
  (`:11666`, `:11738`) share one spec.
- **I1079f Goldens + rig batch.** Run `nice -n 19 ctest --test-dir build/sdl -L golden --output-on-failure -j2`
  once before push; rebless the frame hashes that change and name each in the commit with "column split I1079".
  Expected to move (unverified, list with `ctest -L golden -N`): collection page, songs table, library tree,
  expanded side library, playlist find goldens. One rig run (MUI main list) at the end per RULES 4a.

## 6. Decisions (Johan, 2026-11-05)

Decided: D1 no content-width cap in v1; fixed proportions, absent columns give their width away. D2 the second podcast column keeps the feed name.

Options as they were put:

1. **Content cap (`max_w`) on flex columns.** (Recommended) **Off in v1**: columns keep stable Spotify-style
   proportions, absent columns and reserved slots go; that alone gives the podcast title ~400 pt instead of 210.
   Alternative: cap each flex column at its widest text in the list (cached per list version): fills more when one
   column is short (e.g. a 10-character podcast name), but column positions differ between lists. The helper
   supports both; this only decides what callers pass.
2. **Podcast second column content.** It shows the feed name today ("This Week in Chi..."); Johan calls it the
   description. (Recommended) keep the feed name on the source page and leave a real episode description to the
   detail pane; alternative: show the episode description in that column (needs the podcast source to report it,
   separate row).
