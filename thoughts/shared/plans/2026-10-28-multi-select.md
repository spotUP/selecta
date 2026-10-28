---
date: 2026-10-28
author: spotUP
status: decided
implements: ledger row I632 (multi-select songs and bulk song-menu operations, desktop and MUI)
related: thoughts/shared/plans/2026-10-24-desktop-ui.md, thoughts/shared/plans/2026-10-25-amiga-mui.md (MUI-P4-6), thoughts/shared/plans/2026-10-22-architecture-design.md
---

# Multi-select songs and bulk operations (I632)

## 1. Goal and finish line
Every song list (Library view, Search, drilldowns and the composer list, Queue page, saved playlist page) lets the user select
several songs and run the song menu on all of them at once. Desktop: Cmd/Ctrl-click toggles, Shift-click extends a range,
Cmd/Ctrl-A selects all, Escape clears, right-click on a selected row opens the menu for the whole selection. MUI: the song
listview selects several entries (MUI 3.8 only).

Done = rows I632a..I632g below ticked, each with its test run green, and the reachability test `dui-multiselect` (section 7)
green: select 3 rows, Add to Playlist through the real menu, the playlist gains exactly 3 entries in list order, and the
sentinels prove the bulk path ran and the per-song path did not.

## 2. What exists today (searched: `song_row_reg`, `sm_open`, `sm_ref`, `sel_uim_song_menu_label`, `sel_uim_`, `MultiSelect`, `marks`, `selection`, `VIEW_CURSOR`, the engine X() verb table)

- **Song row registry** (`plugins/ui/desktop/desktop_ui.c:96-104`, `2265-2302`): `sm_rows[SM_ROWS=96]` is rebuilt every frame from
  the rows drawn on screen. It is a hit-test list of the visible window, not a list model: a Library view row outside the
  window (`LIB_WINDOW_ROWS = 12`, `desktop_ui.c:3062`; Search 64) is never in it. Selection therefore cannot live in the
  registry; it must address the whole list.
- **song_ref** (`desktop_ui.c:~86-94`) carries `path`, `qid`, `removable`, `home`, `pl_id`, `pl_index`: no list index for the
  view lists. Registered from: queue page `1508`, `3503`; Home tiles `2855`, `6385`; playlist page `3828`; Song info `4647`;
  Library view `5305-5308` (`lib_view_row_reg`); composer list/drilldown `5496`; Search `6243`.
- **Shared song menu**: one opener `song_menu_open` (`2235-2243`) for right-click, ctrl-click, "..." and Shift+F10
  (`song_menu_key`, `2730-2743`); dispatch in `song_menu_pointer` (`2634-2727`) sends one engine line for `d->sm_ref` only.
  Labels come from the shared `sel_uim_song_menu_label(item, on_queue)` (`src/uimodel/features.c:25`,
  `include/selecta/uimodel.h:112-113`), which MUI already shares (MUI plan line 140 note).
- **Keyboard focus** is per page and separate from any selection: `row_focus`, `q_focus`, `lib_focus` (`desktop_ui.c:270-276`).
- **Engine cursor** is engine-side view state (`VIEW_CURSOR`, `engine.c:144`, `cmd_VIEW_CURSOR` `5313`; `sel_view_state.cursor`,
  `cursor_id`, `include/selecta/engine.h:105-124`). The view is windowed: `rows[0..row_n)` from `window_start`, `row_count` is the
  whole list. There is no view version; the queue has `queue_version` (`engine.h:164`).
- **Only existing multi-selection**: the TUI's private mark list, 16 paths (`plugins/ui/text/text_ui.c:35-50`, `219-244`).
  Nothing in `src/uimodel` or MUI (`grep MultiSelect plugins/ui/mui` = 0 hits; MUI list is single-select, `mui_ui.c:440-451`).
- **Modifier collision**: `song_menu_pointer` opens the menu on Ctrl+left-click (`desktop_ui.c:2719`), and the SDL backend
  maps macOS Command to `TXT_MOD_CTRL` (`backend_sdl.c:372-391`). Today Cmd-click on macOS opens the song menu, so "Cmd-click
  toggles" cannot be added without moving the macOS Ctrl-click convention.

## 3. Q1: where the selection model lives (single source of truth)

**One pure model in `src/uimodel/select.c` + `include/selecta/uimodel/select.h`**, consumed by desktop, MUI and (row I632g) the TUI:

```c
typedef struct sel_uim_list_id { uint32_t kind; uint32_t id; uint32_t version; } sel_uim_list_id; /* kind: VIEW, QUEUE, PLAYLIST */
typedef struct sel_uim_selset {
    sel_uim_list_id list;                 /* the list the indices address; a different list or version clears the set */
    uint32_t anchor;                      /* Shift-click/Shift-arrow extends from here */
    uint32_t base_n, run_n;               /* runs fixed at the last non-Shift click, then the live Shift range */
    struct { uint32_t lo, hi; } run[SEL_UIM_SEL_RUNS]; /* sorted, disjoint, inclusive listing-index runs; SEL_UIM_SEL_RUNS = 32 */
} sel_uim_selset;
```
Operations: `select_one`, `toggle`, `extend` (anchor..i over the base set), `select_all(row_count)` (one run), `clear`,
`contains`, `count`, `bind(list_id)` (clears when the list or its version changed), `ranges_format` / `ranges_parse`
("0-11,40,52-60", fits the 512-byte line pool at 32 runs). No allocation, no floats, C99: runs on 68020 as is.

Why listing indices in runs, not paths: the UI knows paths only for the drawn window; Cmd-A on a 30 000-song library is one run,
not 30 000 paths; Shift-click across rows never loaded works. Why in uimodel and not in the engine: selection is per-window
interaction state that needs no engine round trip per click; the cursor is engine-side because keyboard moves must scroll the
window, selection does not. The engine only sees a selection at the moment an operation runs (Q2).

Desktop holds one `d->sel` (one active list at a time; switching page binds a new list and so clears). `song_ref` gains
`uint32_t list_index` and each `*_row_reg` call site passes it (view: `window_start + k`; queue: queue index; playlist:
`pl_index`). The registry stays the hit-test list; selection never lives there.

## 4. Q2: engine protocol

Today every song verb takes one song: `QUEUE_ADD path` (`engine.c:2777`), `QUEUE_PLAY_NEXT path` (`3121`), `QUEUE_REMOVE id`
(`3126`), `PLAYLIST_ADD_SONG id path` (`4738`), `PLAYLIST_REMOVE_SONG id index` (`4749`), `LIBRARY_REMOVE` (`4801`).

**One line per song is ruled out**, measured from the code:
- `desk_send` keeps decoded string arguments in a ring of 16 pools until the next pump (`desktop_ui.c:7721-7729`): line 17 of
  one turn overwrites line 1's path.
- the command queue is 64 deep by default (`engine.c:5567`): a 65-song selection drops lines.
- every `queue_insert` records one history snapshot (`engine.c:2766-2775`) and the history is 64 deep (`history.h:12`): 50 songs
  of Play Next cost 50 Undo presses and evict all earlier undo steps.
- `QUEUE_PLAY_NEXT` always inserts at playing+1 (`engine.c:3121-3124`): N lines reverse the selection's order.
- N toasts, N `folders_reload` (`engine.c:4744`) for N playlist adds.

**Recommended: one bulk verb family that resolves a selection against the engine's own list**:

| Verb | Args | Effect |
|---|---|---|
| `SONGS_PLAY` | list version ranges | see decision D4 |
| `SONGS_PLAY_NEXT` | list version ranges | inserts at playing+1, +2, ... in list order |
| `SONGS_QUEUE_ADD` | list version ranges | appends in list order |
| `SONGS_PLAYLIST_ADD` | playlist-id list version ranges | appends in list order, one transaction, one `folders_reload` |
| `SONGS_REMOVE` | list version ranges | queue: removes entries; playlist: removes indices high to low; view: `LIBRARY_REMOVE` per song (after the UI's one confirm dialog) |

`list` is `view`, `queue` or `playlist:<id>`; `version` is `queue_version`, a new `sel_view_state.version` (bumped on every
relist, sort, filter, search, drill), or a playlist revision. A mismatch answers `ERROR stale <verb>` and changes nothing; the UI
clears the selection and toasts "List changed, nothing done". Each bulk verb records **one** history snapshot (one Undo reverts
the whole operation), emits one `QUEUE_CHANGED`, and one result event carrying the count for the toast ("Added 3 songs to
Workout"). The single-song verbs stay for the TUI, scripts and non-list rows (Home tiles, Now Playing title, Song info header).
Cost: 5 new verbs in the X() table plus `actions.c:104-115` rows, a view version field (appended, `sel_host`-style rule), and
one bulk path per verb; benefit: correct order, one undo, no ring/queue overflow, no N toasts.

## 5. Q3: interaction details (desktop)

- **Plain click** on a row: selects that row only, sets anchor and keyboard focus (D1). Double-click still plays (`8654-8659`).
- **Cmd-click (macOS) / Ctrl-click (others)**: toggles the row, moves the anchor. Requires the modifier fix: the SDL backend
  turns macOS Ctrl+left-click into a right click (`button 1`), where the platform convention belongs, and the Ctrl branch at
  `desktop_ui.c:2719` is dropped. Command keeps mapping to `TXT_MOD_CTRL` (`backend_sdl.c:385-388`).
- **Shift-click**: selection = base set + run(anchor..row). **Shift+Up/Down** extends the same way from the focus row; plain
  Up/Down moves focus and selects the focus row (Finder/Music). Focus and selection stay separate fields.
- **Cmd/Ctrl-A**: selects the whole list (one run, `row_count`), only when no text field has focus (Search field keeps Cmd-A for
  its text). No `C-a` binding exists today (`src/uimodel/keybind.c:134-176`).
- **Escape** precedence: open menu (`8294`) > dialog > non-empty selection clears > playlist page leaves (`8287`).
- **Scroll**: selection survives (indices address the whole list). **Sort/filter/search/drill/page change**: list version changes,
  selection clears (D2).
- **Spans beyond the window**: yes; rows outside the window are selected in the model and painted when they scroll in.
- **Right-click** on a selected row: menu for the whole selection. On an unselected row: selection becomes that row, then the
  menu opens (D3). "..." button and Shift+F10 follow the same rule.
- **Labels for N > 1**: `sel_uim_song_menu_label(item, on_queue, n)` (shared, MUI gets it free): "Play 3 Songs", "Play 3 Songs Next",
  "Add 3 Songs to Queue", "Add 3 Songs to Playlist", "Remove 3 Songs from Queue/Playlist/Library". N = 1 keeps today's labels.
  Library removal of N asks once: "Remove 3 songs from the library?" (`dlg_open_remove` takes a count).
- **Paint**: selected rows take a theme token fill (accent at low weight; add `SEL_GUI_TOKEN_SELECTION` if none fits), never a
  hardcoded colour; the focus row keeps its own fill (`5492-5493`).

## 6. Q4: MUI (MUI 3.8 only)

MUI 3.8 multi-select is `MUIA_Listview_MultiSelect` on the **Listview** (`MUIV_Listview_MultiSelect_Default` / `_Shifted` /
`_Always`), read with `MUIM_List_NextSelected`, set with `MUIM_List_Select` (`MUIV_List_Select_On/Off/Ask`). `MUIA_List_MultiSelect`
on the List is a later-MUI attribute: do not use it (unverified against the 3.8 autodocs; check the SDK on the rig before coding).

The MUI list is a window over the engine view (`mui_ui.c:440-451` builds it, `926-930` reads `MUIA_List_Active` against
`view->window_start`), so MUI's own entry flags cannot be the truth. The plugin holds one `sel_uim_selset` (same model as the
desktop). After each window refill it pushes the set into the visible entries with `MUIM_List_Select`; on a selection change
notify it walks `MUIM_List_NextSelected` over the window and diffs into the set (a pure sync function under
`plugins/ui/mui/map/`, host-testable). RAmiga-A selects all, Esc clears. The bulk operations use the same `SONGS_*` lines and the
same N labels. The context menu itself is MUI-P4-6, still open (no `MUIA_ContextMenu` in `mui_ui.c`), so I632f depends on it.

## 7. Rows

- [ ] **I632a** Shared selection model `src/uimodel/select.c`. Test `uim-select` (host ctest): toggle, Shift range over a base set,
  select-all as one run, clear, 32-run cap, `bind` clears on list or version change, ranges format/parse round trip.
- [ ] **I632b** Engine `SONGS_*` verbs + `sel_view_state.version` + action table rows. Test `engine-songs-bulk`: Play Next of 3
  keeps list order; one UNDO restores the queue; one `QUEUE_CHANGED`; stale version changes nothing; `SONGS_PLAYLIST_ADD` adds 3;
  playlist removal of indices {1,3} removes the right two; 70 songs pass (beats the 64-deep queue and 16-pool ring).
- [ ] **I632c** Modifier fix: macOS Ctrl+left-click becomes button 1 in `backend_sdl.c`; drop the Ctrl branch at
  `desktop_ui.c:2719`. Test (host, desktop input): Cmd-click no longer opens the menu; Ctrl-click on macOS still does. Regression
  test fails before, passes after.
- [ ] **I632d** Desktop wiring: `song_ref.list_index` at every list call site (section 2), click/Cmd/Shift/Cmd-A/Escape, Shift+arrows,
  selection paint. Test `dui-select-lists`: per list (Library view, Search, drilldown, composer list, Queue, playlist page) the
  scripted clicks give the expected `count`/runs in `--stats`; scroll keeps it, sort clears it.
- [ ] **I632e** Song menu for N: label `n` argument, dispatch through `SONGS_*`, one confirm dialog for N library removal, count
  toasts. **Reachability test `dui-multiselect`** drives the desktop UI top-level entry (the scripted-input path the other `dui-*`
  tests use) on the Library view: click row 0, Cmd-click rows 2 and 5, right-click row 2, Add 3 Songs to Playlist, pick playlist
  P. Asserts: playlist P gains exactly 3 entries, in list order; sentinels `d->sm_bulk_sends == 1` (new) and engine
  `songs_bulk_cmds == 1` (new) prove the bulk path ran; `d->sm_pl_adds` (old per-song path, `2659`) unchanged.
- [ ] **I632f** MUI multi-select (after MUI-P4-6): `MUIA_Listview_MultiSelect`, window/set sync in `plugins/ui/mui/map/`. Tests: host
  ctest `mui-select-sync` (window refill pushes the right entries, NextSelected diff updates the set); rig test with
  `virgill-that_meme_again.mod` library: select 3 rows, context menu Add 3 Songs to Playlist, playlist has 3.
- [ ] **I632g** TUI marks (`text_ui.c:35-50`, 16 paths) move onto `sel_uim_selset`, so the TUI has no private selection. Test: the
  existing TUI `!` mark test passes on the shared model.

## 8. Product owner decisions (to put to Johan)

1. **D1 Plain click.** (Recommended) A single click selects the row (Music, Spotify, Finder). Alt: single click does nothing new,
   only modifiers select. Trade-off: the recommended way changes today's feel (a click now highlights), the alternative leaves
   a selection with no plain-click way to reset it.
2. **D2 Sort/filter/search change.** (Recommended) Clears the selection. Alt: keep it by song identity. Trade-off: keeping it needs
   per-song ids for rows outside the window and risks bulk-acting on songs the user can no longer see.
3. **D3 Right-click on an unselected row while a selection exists.** (Recommended) Selection becomes that row, menu for it
   (Finder/Music). Alt: menu for that one row, selection kept. Trade-off: the alternative keeps work but makes "which songs does
   this menu act on" ambiguous.
4. **D4 "Play" on N songs.** (Recommended) Insert all after the playing song and start the first (queue kept). Alt: replace the
   queue with the selection. Trade-off: replace matches Spotify's "play selection" but discards the queue (one Undo restores it).
5. **D5 Remove from Library on N.** (Recommended) One confirm dialog naming the count. Alt: not offered for N > 1. Trade-off: bulk
   delete is powerful and irreversible in the library.
6. **D6 Cmd-A scope.** (Recommended) Whole list, rows not yet loaded included. Alt: rows on screen only. Trade-off: whole list on a
   30 000-song library makes one menu pick add 30 000 songs (the count label shows it).
7. **D7 Dragging a selection** (queue reorder, drop on a sidebar playlist). (Recommended) A later row, not I632. Alt: part of I632.
   Trade-off: drag is expected on desktop but doubles the input work.
8. **D8 MUI select mode.** (Recommended) `MUIV_Listview_MultiSelect_Default` (the user's MUI prefs decide). Alt: `_Shifted`
   (Shift always needed). Trade-off: Default follows each user's MUI setup; Shifted is predictable across machines.
9. **D9 Selection limit.** (Recommended) 32 separate runs (Cmd-A or any Shift range is one run); a 33rd toggle toasts "Too many
   separate selections". Alt: unlimited runs with an allocated list. Trade-off: the cap keeps the model allocation-free and
   the bulk line under 512 bytes.

## 9. Decisions taken (Johan, 2026-10-28)

- D1 Plain click selects that row only: taken as recommended (standard desktop convention, not put to Johan).
- D2 Sort, filter or search change clears the selection: Johan, recommended.
- D3 Right-click on an unselected row makes the selection that row: Johan, recommended.
- D4 Play on N songs inserts them after the playing song, in list order, and starts the first: Johan, recommended.
- D5 Remove from Library on N songs: one confirm dialog naming the count: Johan, recommended.
- D6 Cmd-A selects the whole list: taken as recommended (standard convention, not put to Johan).
- D7 Dragging a selection is a later row, not part of I632: Johan, recommended.
- D8 MUI uses `MUIV_Listview_MultiSelect_Default` (the user's MUI prefs decide): Johan, recommended.
- D9 **Unlimited selection runs** (Johan chose against the recommendation). Section 3's fixed 32-run model changes: the run array grows on the heap (doubling, owned by the shared model in `src/uimodel/select.c`), allocation failure keeps the existing selection and shows a toast, and the model gets an explicit free on list change and UI shutdown. The SONGS_* verbs must accept a range list longer than one command line: rows I632a/I632b must settle how (for example the engine reads the ranges from the shared model, or the UI sends them in continuation lines) before coding.
