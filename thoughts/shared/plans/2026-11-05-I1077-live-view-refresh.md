---
status: decided
row: I1077
parent: I847c
date: 2026-11-05
author: Opus design agent
---

# I1077 Live view refresh: one library write generation, one invalidation point

Ledger row: `thoughts/spot/todos.md` I1077. Line numbers are against main `b3255d18` (2026-11-05).

Johan's report: after Sync, the Podcasts middle column (the source collection page) still says 0 until he
switches source and back.

## 1. Violated invariant and root-cause level

**Invariant (RULES 1, engine is the single source of truth):** every model the engine publishes from the library,
and every UI cache built from one, must be at the library's current state after any committed write.
No view may need the user to navigate before it shows a write.

**Root cause: engine level, not UI level.** The engine already has a library generation (`sel_state.library_serial`,
I348), but it fails in two ways:

1. **It is bumped by hand at the end of some operations, not at the commit.**
   - `e->library_changes++` happens at exactly 4 sites in `src/engine/engine.c`:
     - `emit_library_path` :4670 (scan end :4984, sync end :8196, LIBRARY_FETCH :8229)
     - `library_catalogue_changed` :7471 (library remove and clear :7484, :7513, :9881)
     - the pin-row poll :10768 (I406b, `sel_library_pin_rows`)
   - Grep is not enough to trust that count, so I checked it a second way. `wq_new` has 40 call sites, which is also
     the number of `wq_submit` sites. They use 38 distinct run/done pairs. Of those writes, only the 3
     remove/clear/catalogue ops reach `library_changes`. Likes, history, Recents touch, ratings, notes, stickers,
     cues, flags, playlist create/delete/rename/move/add/remove/import, folder add/move/delete, the sync time,
     new-this-week, stream add, leveler and analyse all commit without moving the generation.
2. **The engine's own derived models do not follow the generation.**
   - The collection page (`e->coll`, which the Podcasts middle column reads) is rebuilt in only two cases:
     - a COLLECTION_OPEN or COLLECTION_VIEW command (:7398, :7413)
     - after a folders read, and only for `SEL_LIB_ITEM_PLAYLIST` (:8984, I994d3)
   - A source page (`SEL_LIB_ITEM_SOURCE`) is never rebuilt when `library_changes` moves, so after Sync
     `state.collection` still holds the pre-sync page with count 0.

**The UI layer makes it worse with a per-page, ad hoc follow.** All of these are in `plugins/ui/desktop/desktop_ui.c`:

- `desk_sync_follow` :12878 compares the sync progress counters. When they move it calls only `overlay_refresh`
  (:12895, the card repaint). It re-reads no list.
- `desk_library_follow` :14300 does see `library_serial` move, and calls `desk_page_load(d->page)`. For
  `SEL_UIM_PG_COLLECTION`, `desk_page_load` :10385 sends only `VIEW_SOURCE folders` / `VIEW_WINDOW 64`. It never
  re-sends COLLECTION_OPEN or SOURCES, so the source page and its status block stay stale. (SOURCES is sent only on
  page open, :5231.)
- The I994d4 serial check :16114 re-reads the open page only under `pls_id != 0u`, and only when the engine
  republished it, which it does not do for a source page.
- `side_stale` (field :487) and `pls_stale` (field :271) are set by hand after the UI's own writes:
  - :3508/3509 (song menu)
  - :5595/5596 (`pls_add_key`)
  - :6060 (`pl_send`)
  - :14454/14455, :14472/14473, :14485 (playlist drops and edits)
  - :16145 (folders hash)

  Each one guesses which list a write touched. A write from another path (a sync, the CLI `send`, the remote
  protocol, the MUI UI) sets none of them.
- Other UI caches that hold library rows, each with its own signature:
  - queue groups `d->qg` :4371/:4404 (keyed on `sel_engine_song_text_generation` and the queue hash)
  - `home_sig` :16149
  - `side_pl_sig` :16145
  - `pls_find_sig`
  - `si_sig` (song info)
  - `library_sig` (`lib_hash`, Library page)
  - the art miss list (`art_forget_misses` :14314)

**Measured: unverified.** I read the code but did not run it. The trace above (sync end -> `emit_library_path` ->
`library_serial` moves -> `desk_library_follow` -> `VIEW_SOURCE folders` only -> `state.collection` unchanged) is a
code reading. Row I1077a's red test confirms it before the fix.

## 2. Inventory

### 2a. Library write paths (counted from `wq_new` call sites, 40, and the engine's non-queue writers)

| Write path | Where it runs | Moves `library_serial` today? |
|---|---|---|
| Scan end (LIBRARY_SCAN, Add Folder, watch) | scan task -> `scan_collect` :4960 -> `emit_library_path` | yes, once at the end (not per batch) |
| Sync end (Modland, Hippo, podcast refresh, any source) | sync task -> `sync_collect` :8182 -> `emit_library_path` | yes, once at the end; a failed sync that stored rows: no (only `art_changed` :8187) |
| LIBRARY_FETCH / download into the cache | `cmd_LIBRARY_FETCH` -> `emit_library_path` | yes |
| Pin of played online hits (I406b) | pin ring -> poll :10766 | yes, plus `library_pins` |
| Library remove / clear / catalogue op | `library_catalogue_changed` :7470 | yes |
| Playlist create, delete, rename, move, add song, remove song, import; songs to playlist | wq ops (`wq_playlist_*`, `wq_songs_playlist_add`, `wq_folder_done`) | no (`folders_reload_posted` only) |
| Folder add, move, delete | wq ops | no |
| Like, history, Recents touch (COLLECTION_OPEN), rate, vote, note, sticker set/delete, cue, flag, mark, view save, leveler, analyse, stream add | wq ops | no (some bump `panel_serial` through their done callback) |
| Sync time, new-this-week | wq ops, no done callback | no |
| Read-only wq ops (search, sticker get/list, playlist play) | wq | n/a (reads) |

### 2b. View caches that hold library data

| Cache | Owner | Invalidated how today |
|---|---|---|
| `state.collection` (album, artist, composer, source, Liked, Recent, Local, playlist page) | engine `e->coll` | command only; playlist kind also after a folders read (:8984). **Source page never.** |
| `state.view` home / folders / browse / query | engine view | its own re-read on VIEW_SOURCE; folders on `folders_reload_posted` |
| `state.composers`, `formats`, `composer_page`, `stats` | engine | only on the `*_SHOW` command |
| SOURCES rows (counts, sync status) | engine reply | only on request |
| `library_songs` / `library_playlists` count | engine | on `library_serial` (:10622), the only engine model that follows it |
| sidebar list (`side_*`) | desktop | `side_stale` by hand (14 sites) |
| open playlist / collection page model (`pls_*`, `col_*`) | desktop | `pls_stale` by hand; the I994d4 serial check only when `pls_id != 0` |
| queue groups `qg` | desktop | queue hash + song text generation |
| Home (`home_sig`), Library page (`library_sig`), song info (`si_sig`), playlist find (`pls_find_sig`) | desktop | per-frame hash of engine state |
| art miss list | desktop | `art_forget_misses` in `desk_library_follow` only |
| MUI collection list (`col_serial` `mui_ui.c`:2136), `home_sig`, `tile_sig`, `list_sig`, `detail_sig` | MUI | follows engine serials and hashes only: correct once the engine republishes |

## 3. Design

What I searched before choosing this: `library_serial` / `library_changes` (I348), `sel_library_pin_rows` (I406b,
a store counter polled at pump), `sqlite3_update_hook` (`srcmirror.c`:81, I961c, the only hook in use),
`commit_hook` / `total_changes` / `data_version` (none in use; I838 note in `schema.c`:255 on why
`total_changes` must not key expensive store caches), `lib_read_post` with `.again` (coalesced re-reads),
`sel_engine_song_text_generation`, and the MUI `col_serial`. The design reuses `library_serial`,
the pin-row poll pattern and `lib_read_post`/`again`. It adds no second generation.

### 3.1 Store: one write generation, bumped at commit (one place)

- Add `volatile unsigned write_gen` to `sel_library` (`db_internal.h`, next to `pin_rows`). Set it with
  `sqlite3_commit_hook(lib->db, write_gen_hook, lib)` in `sel_library_open` (`db.c`:177), next to
  `sel_library_source_mirror_open`. The hook adds 1 and returns 0.
- The hook fires once per committed write transaction on the read-write connection: autocommit statements,
  explicit BEGIN/COMMIT in scan, folders and index, and the sync task. It covers every path in 2a without touching
  any of them. Under I847c this connection is the writer thread's only read-write handle, so nothing can bypass it.
- Add `uint32_t sel_library_write_gen(sel_library*)` (`db.h`, beside `sel_library_pin_rows`). Pin batches also
  count their commits (`pin_gen`), so the engine can still tell a pins-only change (I1038).
- Never key a store-side cache (item_formats, I838) on `write_gen`. It is for views only.

### 3.2 Engine: one place publishes it, and the engine's own models follow it

- Replace the 4 hand bumps with one pump step. Generalise the pin poll at :10766: if `sel_library_write_gen` moved,
  then `library_changes++` once per pump (coalesced), and `library_pins++` when only pin commits moved.
  - `emit_library_path` and `library_catalogue_changed` stop bumping. They still emit LIBRARY_RESULT and
    `art_changed`.
  - A failed sync that stored rows now moves the generation too.
- A bulk sync commits in chunks (I847d). Publishing per pump would make every follower re-read on every frame of a
  long sync, so the engine rate-limits what it publishes:
  - during a running scan or sync, `state.library_serial` moves at most every 500 ms (`SEL_LIB_GEN_MIN_MS`,
    measured cost decides the value, I1077a)
  - at the task's end it always moves
  - outside tasks it moves on the next pump
- New `lib_models_follow(e)` in the pump, after `lib_reads_collect`. When `library_serial` differs from the last one
  followed, it re-posts every open engine model through the existing `lib_read_post` slots (`.again` coalesces a
  second change while one is in flight):
  - the open collection page, any kind, with `rdc_next_follow = 1` so identical content is dropped (I994d3a)
  - the folders view
  - composers, formats and stats when open
  - the SOURCES rows: republished as state (`state.sources_serial`), so no UI has to ask
  - the existing `library_songs` count (:10622) moves into this function

  This is the fix for the reported bug, and it is UI-independent.

### 3.3 Desktop UI: read once per frame, one invalidation function

- `desk_library_follow` (already called once per turn from `desk_wait_turn` :16024) stays the only reader of
  `library_serial`. Its body becomes one call to a new `desk_library_invalidate(d, pins_only)`, which is the only
  place that:
  - sets `side_stale` and `pls_stale`
  - resets `qg.valid`, `home_sig`, `side_pl_sig_set`, `pls_find_sig`, `si_sig`, `library_sig`
  - runs `art_forget_misses`
  - re-requests the page through `desk_page_load`
  - marks `uim.dirty |= SEL_UIM_DIRTY_ALL`

  It also counts `d->lib_reloads++`, the sentinel, already dumped as `library_reloads=` (:7972). It keeps the I1038
  pins-only rule for the Search page.
- `desk_page_load` for `SEL_UIM_PG_COLLECTION` gets no new send: the engine republishes the page (3.2), and the UI
  reads `state.collection` by serial. Drop the `pls_id != 0u` guard at :16114 so the serial check covers every
  collection kind.
- `desk_sync_follow` stays a card-only follower: progress counters are not library state. The list refresh comes
  from the generation.
- **Flags folded in:** the "own write sent, read again" sets go, because the write's commit now moves the
  generation:
  - :3508/3509
  - :5595/5596
  - :6060
  - :14454/14455, :14472/14473, :14485
  - :16117 (the I994d4 set becomes the generic serial read)
  - the :16145 folders-hash set

  `side_stale`/`pls_stale` stay as "reload wanted" bits for the reasons that are not library changes: a busy retry
  (:4825, :11271, :11338), a filter or sort change (:11037, :11420), the playlists-only mask (:11332), first draw
  (:15649). The invariant in the comment: only `desk_library_invalidate` sets them for library changes.

### 3.4 Amiga MUI UI and text UI

- MUI (`plugins/ui/mui/mui_ui.c`) already follows the engine models by serial and hash (`col_serial` :2136,
  `home_sig` :2010, `tile_sig` :2056, `list_sig` :2294). With 3.2 the source page republishes and the MUI list
  follows with no MUI code change. The one MUI change is a sources-status line that reads `state.sources_serial`, if
  it shows source counts (I1077e checks that).
- The store hook and engine follow are portable C99. `SEL_LIBRARY_NO_THREAD` (Amiga) runs the same commit hook.
  Amiga cost: one re-post per generation move, rate-limited during a sync, which is fine on 68020 (no exemption).
  RULES 4a rig check in I1077e.
- Text UI: `plugins/ui/text` reads engine state the same way. No change expected; I1077e greps it for its own
  library caches.

## 4. Implementation rows (each sized for one sonnet agent)

| ID | Work | Test |
|---|---|---|
| I1077a | Red reachability test first. A `dui-*` test drives `Selecta --ui desktop` with scripted `--input` on a podcast fixture source (reuse the existing podcast or sync fixture; grep `tests/dui` and `tests/net` for one, unverified that it exists): open the Podcasts source page, press Sync, wait for the sync card to end, `--screen-dump`. Assert the page count = the fixture's episode count, `library_reloads>=1`, and no source switch in the input. It must fail on main. Also log the measured re-post cost to choose `SEL_LIB_GEN_MIN_MS`. | the test, red on main |
| I1077b | Store: `write_gen` + `pin_gen`, `sqlite3_commit_hook` in `sel_library_open`, `sel_library_write_gen()` in `db.h`. | unit test in `tests/library`: 1 autocommit insert -> +1; a BEGIN with 100 inserts and COMMIT -> +1; a SELECT -> +0; ROLLBACK -> +0; a pin batch moves both counters |
| I1077c | Engine: replace the 4 hand bumps with the pump poll (coalesced, rate-limited during tasks), add `lib_models_follow` re-posting collection (all kinds, follow mode), folders, composers, formats, stats, and SOURCES as `state.sources_serial`. Move the `library_songs` count in. | engine unit test via `selecta-cli script`: a LIKE / PLAYLIST_ADD / RATE each moves `library_serial` by 1; an open source page's `collection.serial` moves after a sync end with a new count; pins-only still moves `library_pins` (I1038 test stays green) |
| I1077d | Desktop: `desk_library_invalidate`, the only setter of the library-change stale bits. Fold the flag sites listed in 3.3, drop the `pls_id` guard at :16114, and keep `library_reloads=` as the sentinel. | I1077a goes green; existing dui-search pin-keep (I1038), playlist edit (I633) and sidebar (I608, I948) tests stay green; a negative test: an idle page with no writes keeps `library_reloads=0` over 100 frames (no reload storm) |
| I1077e | Amiga and text UI: build `build/amiga` with MUI, grep the MUI and text UIs for caches that read SOURCES or library rows outside the engine serials, wire `sources_serial` if needed. One rig check at the end of the batch. Rig: Amiberry, `--purpose "<session>: I1077 podcast sync refreshes MUI list"`, rig 2 adds `--ram 256`, no sound. | rig: Podcasts page, Sync, the list fills without switching source |

Order: a -> b -> c -> d -> e. b and c are engine-only and can merge before d. The test from a stays red until c and
d land, so mark it `dui-red` until then.

## 5. Decisions (Johan, 2026-11-05)

Decided: (1) refresh at most every 500 ms during a sync or scan, and always at the end. (2) The engine publishes SOURCES rows as state (`sources_serial`).

Options as they were put:

1. **How often the page refreshes during a running sync.**
   - Recommended: at most every 500 ms during a sync or scan, and always at the end. Episodes appear while the sync
     runs, at bounded cost.
   - Alternative: only at the end. Cheapest, but a long Modland sync shows nothing new for minutes, which is the
     "stale" feel he reported.
2. **Engine publishes SOURCES rows as state (`sources_serial`) instead of on request.**
   - Recommended: yes. The source status block and Browse tiles update after every write in every UI, Amiga
     included.
   - Alternative: the UI re-sends SOURCES from `desk_library_invalidate`. Less engine work, but every UI then has to
     do the same (a second copy, RULES 2).
