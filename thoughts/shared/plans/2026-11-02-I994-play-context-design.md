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
