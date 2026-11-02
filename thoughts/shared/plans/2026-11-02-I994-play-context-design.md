---
status: approved
row: I994
date: 2026-11-02
rows: [I994a, I994b, I994c, I994d, I994e, I994f, I994g, I994h]
---

# I994 Play context: a row click continues through its list

## Problem

Johan, Mac desktop: "selecta doesnt continue to the next song in the list". Live STATUS after the song
ended: `state=stopped queue_pos=0/0`.

Cause (code-evident): every desktop row click goes through `desk_play_path` (desktop_ui.c:13487),
which sends `OPEN <path>` then `PLAY`. `cmd_OPEN` (engine.c ~2865) sets `e->q_playing = 0` and calls
`begin_song`. The song plays detached from the queue, so at song end `queue_next_index` has no
playing entry and nothing follows. The list the row came from never reaches the engine.

## Decisions already made by Johan (not re-litigated)

1. Spotify play context. A row click plays from that row and continues through the rest of the
   list; the list is the play context. Songs added by hand (Add to Queue, Play Next) still play
   first, before the context continues ("Next in queue" vs "Next from <context>").
2. A Search result click continues through the following results in order.

## Checklist

- [ ] I994a Engine: context flag, `PLAY_CONTEXT` verb, queue placement, stat, unit tests
- [ ] I994b Engine: list kinds `collection` and `hits` in `songs_open`, unit tests
- [ ] I994c Engine: shuffle, repeat, PREV with a context, unit tests
- [ ] I994d Engine: collection and playlist sort and filter move into the engine (if D1 = yes)
- [ ] I994e Desktop: every row-play site sends `PLAY_CONTEXT`; `col_kind_play` rewritten
- [ ] I994f Desktop: Queue page sections "Next in queue" / "Next from <name>"
- [ ] I994g dui reachability test: album row 3 ends, row 4 starts, sentinel moves
- [ ] I994h (optional) Amiga MUI and text UI adopt `PLAY_CONTEXT`

## 1. Engine command

New verb in `include/selecta/commands.def` (rule 2: the only place commands exist):

```
PLAY_CONTEXT "suus?"   list version start [name]
```

- `list`: the same list grammar as the I632b `SONGS_*` verbs: `queue`, `view`, `playlist:<id>`,
  plus two new kinds from I994b: `collection` (engine `state.collection`, version = its `serial`)
  and `hits` (the source-search hit list `search_hit[]`, version = a hit-list serial, unverified
  whether one exists today; I994b adds one if not).
- `version`: the list version the UI rendered. A mismatch reports `stale` and bumps `songs_stale`,
  exactly as `songs_open` does now. The UI then reloads and the user clicks again; no silent wrong
  song.
- `start`: the row index in the list (engine list order; see D1 for display order).
- `name`: optional label for "Playing from <name>" and "Next from <name>" (album title, playlist
  name, "Search"). Stored in the engine (`ctx_name`, fixed 128-byte buffer, truncated on UTF-8
  boundary) so every UI reads the same label from STATUS / the queue snapshot (rule 1: no derived
  state in the UI; the desktop `np_ctx` becomes a read of engine state).

Implementation reuses, not duplicates, the `SONGS_*` machinery:
`songs_open(e, "PLAY_CONTEXT", list, version, NULL, &s)` binds the list, then the verb resolves the
range `start..(count-1)` with `songs_row_path` / `songs_resolve` (dividers, dirs, album and heading
rows skipped, as today). No path ever travels over the command line, so there is no line-length or
pool-ring limit.

Why a new verb rather than a `SONGS_PLAY` mode: `SONGS_PLAY` inserts after the playing entry and
keeps everything else; `PLAY_CONTEXT` replaces the previous context block. Different queue
semantics, one verb each (the X-macro arity check stays strict).

`list = queue` is the degenerate case: it calls `queue_start(e, start)` and touches nothing else
(the queue itself is the context). The Queue page and Songs page use this.

## 2. Context and the queue

One queue, no second list. Context entries carry a new flag
`SEL_QUEUE_FLAG_CONTEXT 4u` in `sel_queue_entry.flags` (include/selecta/engine.h; 1 and 2 are taken
by DIVIDER and HIDDEN). The queue stays the single source of truth for what plays next; history,
undo (`queue_step`), snapshots and `QUEUE_CHANGES` keep working unchanged.

Queue shape after a `PLAY_CONTEXT`:

```
[ already played rows ]  [ playing ]  [ hand-added block ]  [ context block, flag CONTEXT ]
```

`PLAY_CONTEXT` does, as one history step and one `QUEUE_CHANGED`:

1. Remove every CONTEXT entry after the playing entry (the old context's remaining rows).
   Hand-added entries (no flag) stay.
2. Insert the clicked row as the new playing position: directly after the current playing entry
   (or at `q_hint` when nothing plays), flagged CONTEXT.
3. Insert rows `start+1 .. count-1` flagged CONTEXT after the hand-added block, so hand-added songs
   play first, then the context continues.
4. `queue_start` on the clicked row. Store `ctx_name`. Bump stat `context_plays` and add the
   inserted count to `context_rows`.

Hand-added verbs:

- `QUEUE_ADD` (and `SONGS_QUEUE_ADD`): insert at the end of the hand-added block, i.e. before the
  first CONTEXT entry after the playing entry; at `queue.count` when there is none. No flag.
- `QUEUE_PLAY_NEXT` (and `SONGS_PLAY_NEXT`): unchanged, right after the playing entry. No flag.
- `QUEUE_MOVE` of a row across the boundary: the row keeps its flag (it stays what it was). A
  user who drags a context row up into "Next in queue" sees it there only if the flag clears;
  rule: a move into the hand-added block clears CONTEXT, a move into the context block sets it.
  The boundary is computed, never stored.
- `QUEUE_CLEAR`: clears both blocks and `ctx_name`.

Played rows stay before the playing entry as history (existing behaviour), so PREV walks back
through the context. A new `PLAY_CONTEXT` does not remove played rows; the queue length is bounded
by capacity (section 7).

Early prepare (`nx_id` / `nx_qver`, ~3297-3330): `PLAY_CONTEXT` bumps `queue.version`, so the
existing check cancels a stale prepared next song. No change needed; I994a adds a unit test that
proves it.

## 3. Shuffle and repeat

- Shuffle on, then a context click: the clicked row plays first, then `order_ensure` builds the
  random order. Change in `order_ensure`: non-CONTEXT entries after the playing one go first in
  queue order, then the CONTEXT entries shuffled (D3). Dividers and skip-flagged songs stay
  excluded (I171).
- `PLAY_CONTEXT` resets the random order (`order_reset`) because the set changed; the existing
  `order_qver` check already does this on a queue version change. I994c proves it with a test.
- Big shuffle button on a collection page (`col_kind_play` with shuffle): sends
  `SHUFFLE on` then `PLAY_CONTEXT <list> <ver> <random start>`; the engine picks the start when
  `start` is the sentinel `0xFFFFFFFF` so the UI does no Fisher-Yates of its own (rule 1). The
  UI-side shuffle in `col_kind_play` goes away.
- Repeat all (`repeat = 2`): wraps within the queue, as today. With a context this replays from
  the first row of the queue, history included. Spotify replays the context only; D4 asks.
- Repeat one: unchanged.
- `SEL_OPT_SCOPE album` and `SEL_OPT_STOP_AFTER_QUEUE`: unchanged; stop-after-queue counts the
  context as part of the queue.

## 4. PREV

`cmd_PREV` (~3422) is unchanged: previous non-divider queue row (or `order_prev` in random), restart
the current song past `SEL_PREV_RESTART_MS` or when there is no previous row. Because played
context rows stay in the queue before the playing entry, PREV after row 4 goes to row 3. A PREV on
the first row of a fresh context goes to whatever played before it (the previous context's row);
that matches Spotify's behaviour of PREV across contexts and needs no code.

## 5. Queue page

Engine owns the boundary. The queue snapshot (`SNAPSHOT_QUEUE` / `QUEUE_CHANGES` payload) already
carries per-entry flags; the UI reads `SEL_QUEUE_FLAG_CONTEXT` and `ctx_name`.

Queue page sections (D2):

- "Now playing": the playing entry.
- "Next in queue": entries after the playing one without the flag. Hidden when empty.
- "Next from <ctx_name>": entries after the playing one with the flag.
- History rows before the playing entry: shown as today (scrolled out of view above).

The section headings are UI rows, not queue entries (no dividers inserted). The Songs page keeps
its own UI sort (`d->order`); see Found 3.

## 6. UI sites

Desktop (`plugins/ui/desktop/desktop_ui.c`). A shared uimodel helper
`sel_uim_context_line(list_kind, list_id, version, index, name, buf, n)` formats the command from
the list identity the I632d multi-select code already tags rows with (`song_row_set_list`,
`SEL_UIM_LIST_VIEW / QUEUE / PLAYLIST`), plus a new `SEL_UIM_LIST_COLLECTION` and
`SEL_UIM_LIST_HITS`. One formatter, every UI uses it.

| Site | Line | Today | After |
| --- | --- | --- | --- |
| `pls_play_pos` (album, playlist, collection rows) | 5862 | OPEN+PLAY | `PLAY_CONTEXT collection|playlist:<id>` |
| `col_kind_play` (big Play / shuffle) | 5266 | OPEN+PLAY + N x QUEUE_ADD | `PLAY_CONTEXT` start 0 or random |
| `songs_play_row` (Songs page) | 12883 | OPEN+PLAY | `PLAY_CONTEXT queue <ver> <queue index>` |
| `song_row_dblclick` (Search, Queue) | 13048 | OPEN+PLAY | `PLAY_CONTEXT view|queue` from row tag |
| Queue page Enter (`q_focus`) | 12488 | OPEN+PLAY | `PLAY_CONTEXT queue` |
| Search page Enter (view cursor) | 12773 | OPEN+PLAY | `PLAY_CONTEXT view <ver> <cursor>` name "Search" |
| `online_click` (source search results) | 8740 | OPEN+PLAY | `PLAY_CONTEXT hits` (D6) |
| Song "..." menu Play | 3361 | OPEN+PLAY | `PLAY_CONTEXT` from the row tag the menu was opened on |
| Radio stream | 8732 | STREAM_ADD + play | unchanged (a stream has no next) |
| Typed http URL | 12695 | OPEN+PLAY | unchanged |
| File drop | 13518 | OPEN+PLAY | unchanged (single file) |

Row-tag gap: collection pages (`col_kind != 0`) are not tagged by `song_row_set_list` today; I994e
tags them with `SEL_UIM_LIST_COLLECTION`, `serial`, index.

Amiga MUI, text UI and other UIs: no change is required for them to keep working. Their
`VIEW_ACTIVATE` already inserts the row with `queue_insert` and starts it with `queue_start`, so a
song plays inside the queue and continues to whatever follows. They do not get the context
behaviour until they send `PLAY_CONTEXT` (I994h, optional, after the desktop proves it). The new
flag is ignored by any UI that does not read it. ARexx gets the verb for free through commands.def.

## 7. Memory and size limits

- Context size is bounded by queue capacity from `sel_queue_default_sizes`: 8192 entries / 2 MB
  text at >= 64 MB fast RAM, 2048 / 256 KB at >= 8 MB, 256 / 32 KB below.
- Overflow: the context is truncated at capacity (entries or text arena, whichever fills first),
  with one `queue_full` log naming how many rows were dropped. Never a failure; the clicked row
  always plays (it is inserted first).
- Before inserting, step 1 frees the old context rows. If played history then still blocks room,
  the oldest played CONTEXT rows before the playing entry are dropped first (oldest history goes
  before new context). This keeps a 256-entry Amiga queue usable after many album clicks.
- `ctx_name`: 128 bytes static in the engine struct. No allocation per command.
- `songs_resolve` copies paths up front today; for a 2000-song collection that is a transient copy
  bounded by the queue text arena size, already the case for `SONGS_*`.

## 8. Test plan

Reachability (one test, I994g): dui test drives the real desktop UI.

1. Open an album collection page with at least 5 songs (fixture library).
2. Click row 3 (the real row click path, `pls_play_pos`).
3. Assert STATUS `queue_pos` = 3 of N and the playing path is row 3.
4. Fast-forward to song end (existing test seek / short fixture song).
5. Assert row 4 plays (`queue_pos` = 4 of N, path = row 4).
6. Sentinel: engine stat `context_plays` went 0 to 1 and `context_rows` = N - 2; proves the new path
   ran, not OPEN+PLAY.

Unit tests where the behaviour lives (engine script tests, tests/m1-style `.sel`):

- I994a: placement (hand-added before context; a second `PLAY_CONTEXT` replaces the old context
  rows and keeps hand-added ones; `QUEUE_ADD` lands before the context block; `QUEUE_PLAY_NEXT`
  right after playing); `stale` on a version mismatch; truncation at capacity with `queue_full`;
  undo restores the previous queue in one step; a prepared next song is cancelled.
- I994b: `collection` and `hits` lists resolve the right paths; heading and album rows skipped.
- I994c: shuffle keeps hand-added first; random start sentinel; repeat all wrap; PREV after row 4
  goes to row 3.
- I994e: the uimodel formatter produces the exact line per list kind (pure function test).
- I994f: a dui golden of the Queue page with both sections (rebless once per push).

Regression: the I994g test fails on current main (song ends, `queue_pos=0/0`), passes after.

## 9. Decisions (Johan 2026-11-02: D1-D6 all yes, as recommended)

Answered 2026-11-02: D1 screen order (engine sort/filter, I994d in scope); D2 two Queue sections; D3 and D4 Spotify rules; D5 and D6 Play/Shuffle button and Online/Modland clicks use the context.

Original questions:

- D1. Sort and filter of collection and playlist pages. The desktop sorts and filters rows in the UI
  (`col_rebuild`, `pls_ord`, `col_sort`, `col_q`), so the engine list order differs from what the
  user sees when sorted or filtered. Recommended: move sort and filter into the engine (I994d), so
  the context follows what is on screen and rule 1 holds. Trade-off: one extra engine row before the
  desktop rows; the alternative (context follows stored order) plays songs in an order the user did
  not see.
- D2. Queue page shows "Next in queue" and "Next from <name>" sections. Recommended: yes (Spotify).
- D3. With shuffle on, hand-added songs still play first, context shuffled after. Recommended: yes
  (Spotify).
- D4. Repeat all with a context: wrap over the whole queue (today) or over the context only.
  Recommended: the context only (Spotify); played rows of older contexts are not replayed. Costs a
  small change in `queue_next_index` wrap target (I994c).
- D5. Big Play / Shuffle on collection pages becomes a context. Recommended: yes; it also fixes the
  over-16-songs bug (Found 1).
- D6. Online and Modland source-search result clicks continue through the following results.
  Recommended: yes, consistent with decision 2.

## 10. Rows (each sized for one sonnet agent, about 40 tool calls)

- I994a Engine core: `SEL_QUEUE_FLAG_CONTEXT`, `PLAY_CONTEXT` in commands.def, `ctx_name`,
  placement and replace logic, `QUEUE_ADD` placement, capacity truncation, stats
  `context_plays` / `context_rows`, unit tests. Design sections 1, 2, 7.
- I994b Engine lists: `collection` and `hits` list kinds in `songs_open`, hit-list serial, unit
  tests. Section 1.
- I994c Engine order: `order_ensure` hand-added first, random start sentinel, repeat-all wrap per
  D4, PREV test. Sections 3, 4.
- I994d Engine sort and filter for collection and playlist pages; `col_rebuild` reads the engine
  order (only if D1 = yes). Section 9 D1.
- I994e Desktop wiring: uimodel formatter, collection row tags, every site in the section 6 table,
  `col_kind_play` rewrite, `np_ctx` reads `ctx_name`. Section 6.
- I994f Desktop Queue page sections, golden. Section 5.
- I994g dui reachability test with sentinel. Section 8. Can be written first against the verb
  stub and kept red until I994e lands.
- I994h (optional) Amiga MUI and text UI send `PLAY_CONTEXT`. Section 6.

Order: a, b, c, (d), e, f, g; g's test file may land with e.

## Found

1. `col_kind_play` (desktop_ui.c:5266) sends one `QUEUE_ADD` per song in one frame. `desk_send`
   uses a static ring of 16 x 512-byte pools, and the engine reads string args at pump time, so an
   album over 16 songs queues overwritten or duplicated paths; over about 63 lines the `cmd_q`
   (depth 64) also fills and reports `queue_full`. Code-evident, not verified live. I994e fixes it.
2. The UI sort and filter on collection and playlist pages is derived state in the UI (rule 1);
   D1 covers it.
3. The Songs page shows the queue in a UI sort (`d->order`), so a row index there is not the
   queue index; `songs_play_row` already maps through `d->order[row]`, which I994e keeps.

## 11. I994d engine sort and filter (interface)

Written 2026-11-02 for ledger row I994d (decision D1 = yes). Numbered 11 because section 10 already holds the row list. Goal: the
engine owns the order of a collection or playlist page; the row the UI draws at position `p` is the row `PLAY_CONTEXT collection
<serial> p` plays. Rule 1: no sort or filter left in a UI.

### 11.1 Today (measured by reading the code)

- Desktop `col_rebuild` (desktop_ui.c:4814) builds `pls_ord` from `col_q` with `col_match` (case-insensitive substring of
  `pls_title`) and sorts with `qsort` / `col_order_cmp` when `col_sort == 1` (title, ties by stored index). Callers: `pls_reload_run`
  (4774), `pls_briefs_follow` (4634, the titles landed), `pls_open` (4850), the sort button (5949 -> `col_changed`), the search
  key handler (6130 -> `col_changed`). `pls_ord` maps a shown position to a stored row index; `pls_play_pos` (5891), row hit tests
  (6023) and the dump (7306, 7355) read through it. Sort keys today: 0 stored order ("Custom Order"), 1 title. No direction.
- Album, artist, composer, Liked, Recent, Local and source pages come from the engine page `state.collection` (built by
  `coll_build_run` on the read thread, engine.c:7051, published by `coll_publish` with a new `serial`). Rows are `key\0plays\0`
  (stride 2) in library order. `coll_song_row` gets the title (`f[1]`) and drops it.
- Playlist pages (`col_kind == 0`) are not engine pages: the desktop reads keys through `sel_engine_playlist_keys_post/take`
  (I961c) and titles through the `song_briefs` posted read. `COLLECTION_OPEN` with kind PLAYLIST only touches Recents (engine.c:7161).
- `songs_open_ex` list `collection` (I994b, engine.c:8735) resolves `key_at[i]` = stored row `i`, version = `coll.serial`. List
  `playlist:<id>` reads `sel_library_playlist_keys` in stored order, version = `view.pub.version`.
- Amiga MUI (`mui_col_row`, mui_ui.c:1423) shows `state.collection` rows in stored order, no sort, no filter.
- Precedent to copy: `COMPOSERS_VIEW filter sort` (I611, engine.c:6399). The engine stores the filter and sort, the read slot
  rebuilds the list, the filter uses the shared `sel_uim_lfilter_match`, stat `composer_views` is the sentinel.

### 11.2 Command

New verb in `include/selecta/commands.def` next to `COLLECTION_OPEN` (rule 2):

```
SEL_CMD(COLLECTION_VIEW, "suu?", "playlists", "filter sort [descending]", "Filter And Sort Collection Page")
```

- `filter`: text, at most 63 bytes, matched with `sel_uim_lfilter_match` against the row title (the same match the Library page
  uses; it replaces `col_match`). Empty: every row.
- `sort`: `SEL_COLL_SORT_STORED = 0` (the page's own order: album track order, playlist position, plays for an artist),
  `SEL_COLL_SORT_TITLE = 1`. New enum in `include/selecta/engine.h` beside `SEL_DRILL_PAGE_*`, with `SEL_COLL_SORTS 2u`; an
  unknown value clamps to 0 (as `COMPOSERS_VIEW` does). More keys (artist, album, date added, duration) append to the enum
  later without a new verb.
- `descending`: 0 or absent ascending, 1 descending. Ties always break by stored index ascending so the order is stable and
  reproducible.
- Applies to the open page (`e->coll`). `COLLECTION_OPEN` without `restore` resets the view to stored, unfiltered, ascending (the
  desktop does this today in `pls_open`, 4872); `COLLECTION_OPEN ... restore` (Back / Forward) keeps the view the engine holds.
- No list argument: there is one open page per engine, exactly like `COMPOSERS_VIEW` applies to the one Library page.

### 11.3 Where it runs

- In memory, not SQL `ORDER BY`. Reasons: the filter is per keystroke and must not re-query the library; the stored order of an
  album or playlist is itself a library order the page already holds; on Amiga (no read thread) a re-query per keystroke would
  block the engine; the title filter has to match exactly what the row shows.
- One pure function in engine.c: `coll_order_build(const char* rows, const char* titles, uint32_t n, const char* filter,
  uint32_t sort, uint32_t desc, uint32_t* out)` returns the shown count. No library call, no engine state; unit-testable.
- The titles: `coll_song_row` and `coll_composer_row` keep the title they already receive in a second block `rdc_titles`
  (`title\0` per row, same row order), published as engine-private `e->coll_titles`. The public `rows` stride stays 2, so the
  MUI UI and `songs_open` keep parsing it unchanged.
- Full build: `coll_build_run` (read thread) calls `coll_order_build` with the view current at post time (copied into
  `rdc_filter` / `rdc_sort` / `rdc_desc` by `coll_post`, as `composers_post` copies `rd_comp_*`).
- View change: `cmd_COLLECTION_VIEW` stores `coll_filter[64]`, `coll_sort`, `coll_desc`, bumps `collection_views`, and posts a
  re-order job on the same `rd_coll` slot (`rdc_page = 0`, new `rdc_reorder = 1`). The job reads the published `coll.rows` and
  `coll_titles` (only the pump thread replaces them, and only in `coll_publish`, which cannot run while the slot is busy) and
  writes a fresh `rdc_order`. The engine thread never sorts. Without a thread the job runs inline, as every posted read does.
- A view change while a read is in flight: the newest view wins, the same idle check as `coll_post` / `composers_post`.

### 11.4 What the page carries

Appended to `sel_collection_page` (include/selecta/engine.h, after `shelf`, so existing fields keep their offsets):

```
    const uint32_t* order; /* I994d: shown position -> stored row index; NULL means identity (drill group pages) */
    uint32_t shown;        /* rows in order (<= songs): the filtered count */
    uint32_t sort, descending;
    char filter[64];       /* the view this order was built for */
```

- `coll_publish` publishes a re-order exactly like a page: new `serial = ++coll_serial`, new `order`, `shown`, view fields. Rows
  and titles are reused on a re-order (only the order block is swapped). The serial is the single version: a UI that rendered
  serial S and clicks after the order changed gets `stale` from `songs_open`, never a wrong song.
- Drill root and group pages (`SEL_DRILL_PAGE_ROOT` / `GROUPS`): `order = NULL`, `shown = songs`; the view does not apply.
- Memory: `order` is 4 bytes per row (32 KB at the desktop's 8192-row cap); titles add the sum of title lengths (about 30 bytes
  per row). Both freed with the page. No allocation on the engine thread.

### 11.5 PLAY_CONTEXT and the other bulk verbs

- `songs_open_ex` list `collection`: `s->count = pg->shown`, `s->key_at[i] = row(pg->order[i])` (identity when `order` is NULL).
  So `start` and every `SONGS_*` range index are shown positions, the context is the shown rows from `start` to the end, and a
  filtered page continues through the filtered rows only. Version check unchanged (`pg->serial`).
- Playlist pages become engine pages (11.6), so the desktop playlist page sends `PLAY_CONTEXT collection <serial> <pos>`.
  `playlist:<id>` stays for callers that mean the stored playlist (Playlists tree, ARexx, multi-select on a tree row) and stays in
  stored order.
- Edits that need the stored position (`PLAYLIST_REMOVE_SONG playlist index`, drag reorder) keep sending the stored index the UI
  reads from `order[pos]`, as `pls_ord` is used today.

### 11.6 Playlist pages as engine pages

- `COLLECTION_OPEN 0 <id>` (kind `SEL_LIB_ITEM_PLAYLIST`, key = decimal id) builds the page on the read slot: keys from
  `sel_library_playlist_keys` in position order, titles from the library song briefs in one request, plays column `0`. Title =
  the playlist name, `sub` empty.
- Any playlist change that bumps the folder generation while the open page is that playlist re-posts the build (with the current
  view), so the page republishes with a new serial. This replaces the desktop's `pls_want` / `pls_min` polling for that page.
- `sel_engine_playlist_keys_post/take` stays for its other callers; the desktop collection page stops using it.

### 11.7 UIs

- Desktop `col_rebuild` becomes a copy: `pls_ord[i] = pg->order != NULL ? pg->order[i] : i`, `pls_vn = pg->shown`. Deleted:
  `col_match`, `col_order_cmp`, `col_ci_cmp`, `g_col_d`, the `qsort`. `col_sort` and `col_q` stay only as the input field state;
  the sort button and every search keystroke send `COLLECTION_VIEW <q> <sort>` and the page redraws when the new serial lands
  (`pls_reload_run` already reloads on a serial change). The `pls_briefs_follow` re-sort (4634) goes: the engine sorted on its
  own titles.
- `pls_reload_run` reads `state.collection` for playlist pages too (the `col_kind == 0` keys branch goes).
- Amiga MUI: no sort UI today and none required, but `mui_col_row` and the list count must read through `order` / `shown`. Reason:
  the view is engine state, so after `UI_SWITCH` from a desktop with a filter set, a stored-order MUI list would send indices
  that are not shown positions once MUI sends `PLAY_CONTEXT` (I994h). Small change, same row as the build check.
- Text UI and others: they do not render collection pages (unverified beyond a grep of plugins/ui/mui and plugins/ui/text).

### 11.8 Stats, dump and tests

- `sel_engine_stats` gains `collection_views` (COLLECTION_VIEW commands applied) and `collection_orders` (orders published: full
  builds plus re-orders). Existing `songs_collection_opens` proves PLAY_CONTEXT bound list=collection.
- Desktop dump line `collection ...` gains `views=` and `orders=` from the stats and `eshown=` from `pg->shown`, so a dui test sees
  that the shown count came from the engine.
- Unit (where it lives): `tests/engine/collection_view_test.c` drives the engine command entry: title sort ascending and descending
  on an album fixture, filter `intro` keeps one row, stable ties, drill page identity, `COLLECTION_OPEN` resets and `restore`
  keeps, `PLAY_CONTEXT collection <serial> 1` after a title sort plays the second title (not stored row 1), the old serial after a
  re-order gives `stale` and `songs_stale` + 1.
- Existing `dui-collection-page` (sort button Custom Order -> Title, filter `intro` gives shown=1 of=3) stays green unchanged; it
  now also asserts `views>=1` and `orders` moved, which proves the desktop sort came from the engine.
- The one reachability test (REACHABILITY_PROTOCOL): `dui-collection-sorted-context`, through the real desktop UI entry
  (`-DSELECTA` binary, scripted clicks): open playlist alpha (Main Theme, Intro, Bass Line), click Sort (Title: Bass Line, Intro,
  Main Theme), click shown row 1 (Intro), let it end. Asserts: the playing path is Intro, then Main Theme starts (shown row 2, not
  stored row 2 Bass Line); sentinels `collection_views` 0 -> 1, `collection_orders` >= 2, `songs_collection_opens` 0 -> 1,
  `context_plays` 0 -> 1. Revert proof: make `songs_open_ex` ignore `order` (stored index); the test must fail on the song after
  Intro. It needs I994e (the row click sends PLAY_CONTEXT) and may share the fixture and script helpers of I994g.

### 11.9 Rows

| ID | Deps | Work | Test |
| --- | --- | --- | --- |
| I994d1 | I994b | Engine order model: `SEL_COLL_SORT_*`, appended page fields, `rdc_titles` / `coll_titles`, `coll_order_build`, order in `coll_build_run` and `coll_publish`, `songs_open_ex` collection through `order`, stat `collection_orders`. engine.c, engine.h. | collection_view_test.c: pure order cases + PLAY_CONTEXT shown index + stale |
| I994d2 | I994d1 | `COLLECTION_VIEW` verb (commands.def), `cmd_COLLECTION_VIEW`, re-order job on `rd_coll`, reset on open, keep on restore, stat `collection_views`. engine.c, commands.def. | collection_view_test.c: view change republishes, reset/restore, old serial stale |
| I994d3 | I994d2 | Playlist pages as engine pages: `COLLECTION_OPEN` kind PLAYLIST builds on the read slot (keys, titles), republish on playlist change while open. engine.c. | collection_view_test.c: playlist page rows in position order, add/remove republishes, sorted PLAY_CONTEXT |
| I994d4 | I994d3 | Desktop playlist page reads `state.collection` (drop the keys branch in `pls_reload_run`, the `pls_want` / `pls_min` polling for it); local UI sort still in place. desktop_ui.c. | dui-collection-page, dui-playlist-page, playlists green |
| I994d5 | I994d4 | Desktop `col_rebuild` copies engine order; sort button and search keys send `COLLECTION_VIEW`; delete `col_match`, `col_order_cmp`, `col_ci_cmp`, `g_col_d`, briefs re-sort; dump `views=` `orders=` `eshown=`. desktop_ui.c. | dui-collection-page asserts views/orders; revert proof: local qsort back, views stays 0 |
| I994d6 | I994d1 | Amiga MUI `mui_col_row` and count read through `order` / `shown`. mui_ui.c. | Amiga cross build; rig check in the batch run at the end |
| I994d7 | I994d5, I994e | Reachability test `dui-collection-sorted-context` (11.8) with sentinels and revert proof. tests/dui. | itself, red before I994d5 + I994e, revert proof recorded |
