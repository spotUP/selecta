---
date: 2026-11-01
topic: UI thread waits on the library (I906 audit and design; covers I903, I904, I905)
tags: [library, threading, desktop, engine, beachball, I906, I904, I905, I903, I829]
status: design
---

# UI thread waits on the library (I906)

Ledger rows: I903 (todos.md:1348), I904 (1349), I905 (1350), I906 (1351). Prior work: I782 (nowait), I829 (watch
sentinel), I829b (sqlite owner check, 96d776eb, merge 63000224), design 2026-10-31-interactive-path.md 1.1.

## 1. Thread model

The desktop app has no separate engine command thread. The engine's command pump runs on the UI main thread.

- `apps/selecta/main.c` ~755-771: the shell loop calls, per UI, `g_uis[u].wait(state, wait_ms)` (desktop: `desk_wait`,
  `plugins/ui/desktop/desktop_ui.c:14570`), then `pump_turn(e, clockless, sink, ctx)` (`main.c:118-121`).
- `pump_turn` calls `sel_engine_pump` (`src/engine/engine.c:7752`), which pops `cmd_q` (`engine.c:7761`) and runs
  `dispatch` (`engine.c:7298`, X-macro `SEL_CMD_IMPLEMENTED` to `cmd_<ID>`).
- `desk_send` (`desktop_ui.c:11680`) goes to `sel_engine_send_line` (`src/protocol/parse.c:7`) and
  `sel_engine_send` (`engine.c:7523`), which only pushes to `cmd_q`.
- Result: every `cmd_*` the desktop sends runs on the UI thread one pump later. Any synchronous library call inside a
  `cmd_*` handler, inside a pump helper, or in a direct desktop call into the engine API blocks the UI thread.

Library side (`src/library/db/worker.c`):

- `sel_library_request` (worker.c:173-208) takes the recursive `call_lock`, pushes the request to the worker thread and
  `pthread_cond_wait`s for the reply. `call_lock` is held for a whole job (`sel_library_run`, worker.c:66) and between a
  sync's requests, so a waiting caller can sit behind a folder scan or a source sync for minutes.
- `g_nowait` (`__thread`, worker.c:18; set by `sel_library_nowait`): trylock, on failure count `g_deferred` and return 0
  (caller shows stale data and retries).
- `g_watch` (`__thread`, worker.c:20; `sel_library_watch`) and `g_watched_waits` (worker.c:21, 194): sentinel counting
  waits made by a watched thread. **No caller in src/, apps/ or plugins/ sets watch today**; only
  `tests/engine/library_ui_nowait_test.c` (ctest `engine-library-ui-nowait`) uses it.
- Debug owner check `owner_check` (worker.c:112-129, sqlite authorizer, `library_thread_violation`, backtrace + abort),
  installed in `sel_worker_start` under `!NDEBUG`. This is the pattern for the new guard.
- Job thread: `src/library/db/job.c` (`job_main`, `job_lock`, `sel_library_task_start/take`). `sel_library_pin`
  (`pin.c:75`) is async: copies into `pin_q` under `job_lock` and wakes the job thread; `sel_pin_job_run` calls
  `pin_write` (`pin.c:24`), which sends one `SEL_LIB_REQ_SCAN_BATCH` through `sel_library_request`.

Measurement gap: `turn_ms_max` is measured only inside `desk_wait` (desktop_ui.c:14570-14583, excludes poll time). It
does **not** include `pump_turn`, where the `cmd_*` waits happen. A dui test that asserts `turn_ms_max < 50` today
cannot see a blocking `cmd_FORMATS_SHOW`. Row I906a adds a pump-inclusive measure.

### Existing async patterns to reuse (do not invent new ones)

- Read slots: `sel_lib_read` slots `rd_home`, `rd_search`, `rd_folders` (engine.c:538), `lib_read_post` /
  `lib_read_take` / `lib_read_idle` / `lib_read_end` (engine.c:3593-3640; a thread per read under `SEL_LI_THREAD`,
  inline on Amiga), collected in `lib_reads_collect` (engine.c:6417), `sel_engine_reads_pending` (7737),
  `sel_engine_pump_reads` for clockless runs and scripts. `LIBRARY_SEARCH` already uses `rd_search` (engine.c:3735-3743).
- Items thread: `sel_engine_library_items_post/take` (`li_thread`, engine.c:6574).
- Nowait with stale data and retry: `cmd_SOURCES` (engine.c:4562-4571, `sources_retry`), `song_total` (7650),
  `rating_pending` in `stats_get` (8042), desktop `pls_reload` (desktop_ui.c:4492).
- Async write: the pin ring (`pin.c`) on the job thread.

## 2. Site table

Found from structure: every `sel_library_*` call in engine.c (129 call lines) traced to its caller; every public
library function routes through `sel_library_request` or `sel_library_run` except `task_*`, `pin`, `pin_rows`,
`hash_stats` (takes `job_lock` briefly), `csv_has` (pure) and the poll functions (`hash_poll`, `prefetch_poll`,
`lyrics_result`; locking not re-verified, see Open). No direct sqlite3 use in src/engine or plugins/ui/desktop.

"Waits" = can block the UI thread behind a scan or sync (yes unless nowait).

### 2a. Command handlers (run on the UI thread via pump)

| Entry | Chain | Library call | Waits | Proposed change | Row |
|---|---|---|---|---|---|
| Browse open (I905) | desk_page_load PG_BROWSE (desktop_ui.c:9507-9512) sends FORMATS_SHOW | cmd_FORMATS_SHOW (engine.c:5245) `sel_library_format_list` (whole-catalog group by) | yes | new read slot `rd_formats` via lib_read_post; reply when collected | I906b |
| Browse open (I905) | same, COMPOSERS_SHOW | cmd_COMPOSERS_SHOW (5263) composers_fill_list (5211) `sel_library_composer_list`, `composer_page` | yes | read slot `rd_composers` | I906b |
| Browse open | same, SOURCES | cmd_SOURCES nowait (4562) | no | none | - |
| Collection open | COLLECTION_OPEN | `touch`, coll_build_page (`collection`, `composer_page`) | yes | read slot; touch to write queue | I906g |
| Stats page | STATS_SHOW | `sel_library_stats` | yes | read slot | I906g |
| Formats/composers drill | VIEW_ACTIVATE, PLAY_QUERY, QUEUE_ADD_QUERY | run_search (engine.c:3682) `sel_library_run` | yes | post via rd_search-style slot; act on collect | I906g |
| Save view | VIEW_SAVE | `sel_library_run` | yes | write queue | I906f |
| Fetch | LIBRARY_FETCH | `sel_library_fetch` (may hit network) | yes | job thread task, reply event | I906g |
| Writes | LIKE (like/liked/unlike), RATE (rating_set/push), FLAG, NOTE, CUE_GOTO/CUE_SET, STICKER_DEL/GET/LIST/SET, VOTE (sticker_set, vote), PLAYLIST_ADD_SONG/CREATE/DEL/IMPORT/MOVE/REMOVE_SONG/RENAME, SONGS_PLAYLIST_ADD, SONGS_REMOVE, FOLDER_ADD/DEL/MOVE, LIBRARY_CLEAR, LIBRARY_REMOVE, LIBRARY_TOUCH, ANALYSE (loudness_set), PREF_SET (set_sources_off), STREAM_ADD (items, playlist_add, add_song) | many `sel_library_*` setters | yes | write queue on the job thread (pin ring pattern generalised), engine updates its own state optimistically, `LIBRARY_RESULT` event on completion; STICKER_GET/LIST become read slots | I906f |
| Save to Library (I904) | song menu (desktop_ui.c:3314-3317) SOURCE_PIN, cmd_SOURCE_PIN (2460), search_pin (2443) | `sel_library_pin` | no (async) | none at the pin; see I904 below | I906c |

### 2b. Pump helpers (run on the UI thread inside sel_engine_pump or a command)

| Entry | Chain | Library call | Waits | Proposed change | Row |
|---|---|---|---|---|---|
| Play a song | OPEN, begin_song, read_song | `best_source`, `sel_library_fetch`, song_config_load, loudness_slot | yes | resolve source on a read slot before begin_song; fetch on job thread | I906g |
| Song end | hist_end | `history_add` | yes | write queue | I906f |
| Shuffle/order | order_ensure | `flag_skipped` | yes | read slot or cache | I906g |
| Playlist play | playlist_play, songs_unstage | `playlist_keys` | yes | read slot | I906g |
| Radio | radio_fill | `radio_next` | yes | read slot, prefetch one ahead | I906g |
| Source drill | drill_sample_row | `source_drill` | yes | read slot | I906g |
| Source rows | sources_row, source_name_row | `source_songs` (nowait), `items` | items yes | items on read slot | I906g |
| Votes/likes | view_votes, vote_store, like_sync | `sticker_get`, sticker set | yes | read slot / write queue | I906f |
| Loudness | leveler_writeback | loudness set | yes | write queue | I906f |
| New this week | persist_new_this_week, sync_time_get/set | raw `sel_library_request` | yes | write queue | I906f |
| Add folder finish | addf_finish | folder writes | yes | write queue | I906f |
| New song | refresh_snapshot song_panel (engine.c:7634) | song panel reads | yes | read slot `rd_panel`, panel shows on collect | I906d |
| View build | view.c:755-768 row_art, view_art_id / view_art_artist (engine.c:6246-6277) | `art_find`, `album_artist` per row | yes, N per view | batch per view on a read slot, or read art ids in the view query itself | I906d |
| Smart/prefetch sync | smart_sync, prefetch_sync | request (since I829b) | yes | job thread only (check it is not reached from pump) | I906g |

### 2c. Direct desktop calls into the engine API (UI thread, synchronous)

| Entry | Call site | Library call | Waits | Proposed change | Row |
|---|---|---|---|---|---|
| Remove dialog | dlg_open_remove (desktop_ui.c:2944, 3347) | `sel_engine_song_title` | yes | use the row's own title (already in the view) | I906e |
| Clear dialog, new playlist | dlg_open_clear (2954), side_create_playlist (5712) | `sel_engine_library_playlists`, folder_counts | yes | use the sidebar's cached playlist list (pls_reload data) | I906e |
| Add to playlist submenu | song menu k==3 (3319) | `sel_engine_library_items` | yes | `sel_engine_library_items_post/take` (li_thread) | I906e |
| Queue groups | queue_groups_sync (4131-4165) from draw_queue (4214), queue_pointer (4291), library_dump_build (7223), queue_list_key (12165) | `sel_engine_song_group` per queue row | yes, N per queue change | engine resolves groups when it builds the queue (on its read slot) and keeps them in queue state; desktop reads state only | I906e |
| Sidebar playlists | pls_reload_run (4505, 4598) | playlist_keys, song_briefs | no (nowait) | none | - |
| Song info composer | si_composer_load (6050) | `sel_engine_song_meta` | yes | read slot | I906e |
| Stats in UI | 12 `sel_engine_stats_get` calls | nowait for rating_pending; hash_stats takes job_lock briefly | no | none (job_lock is short) | - |
| Artwork | art_load (plugins/ui/desktop/art.c:194) from load_tile_art (desktop_ui.c:1660, call 1667) and col_cover_refresh (4702, call 4707) | prov_find/prov_get via art_call, `sel_library_request` (src/library/db/art.c ~154-170) | yes: `art_thread_join(art_thread_start(...))` starts a thread and joins it at once | drop the join: keep the thread handle per request, collect in desk_wait, repaint on arrival | I906c |

### Counts by verdict

- Command handlers that wait: about 40 commands (2a rows; the write row alone covers 30).
- Pump helpers that wait: 14 helpers (2b).
- Direct desktop calls that wait: 6 sites (2c), plus art_load.
- Already non-blocking (nowait or async): cmd_SOURCES, LIBRARY_SEARCH, sidebar playlists, stats_get rating, pin.

### I904: Save to Library beach ball

`SOURCE_PIN` is async (`sel_library_pin` only copies into the ring). The beach ball comes after it:

1. The job thread runs `pin_write`, one `SCAN_BATCH` through `sel_library_request`, holding `call_lock` and the worker.
2. The pump sees `sel_library_pin_rows` change (engine.c:7777) and bumps `library_changes`.
3. The desktop sees the new `library_serial` in `desk_library_follow` (desktop_ui.c:13197), which calls
   `desk_page_load(current page)` and `art_forget_misses`.
4. The reload sends sync reads (page commands from 2a, view rebuild with per-row `art_id` from 2b, `art_load` joins
   from 2c, song panel). Each waits behind the SCAN_BATCH and behind each other on the UI thread.

Unverified: no `sample` of the real app during a pin was taken. Fix: make the reload path's reads posted (I906c with
I906d/e); the pin itself needs no change. Done = test pins while the worker is busy and the pump-inclusive turn max
stays < 50 ms.

### I905: Browse beach ball and wrong Modland tile

1. Beach ball: `desk_page_load` for `SEL_UIM_PG_BROWSE` (desktop_ui.c:9507-9512) sends `FORMATS_SHOW` and
   `COMPOSERS_SHOW`. Both handlers (engine.c:5245, 5263) run whole-catalog group queries synchronously on the UI
   thread, and wait behind any running scan or sync.
2. Modland tile opens Modules.pl: `browse_click` (desktop_ui.c:8682) for a source tile (kind 2) sets the query
   `source:<key>` and calls `desk_search_fire` (8569) without clearing `d->scope`. `online_scope_load` (8396) restored
   the pref `search.last_source` (for example `modulespl`), so `desk_search_fire` takes `online_fire` (8419) and sends
   `SOURCE_SEARCH modulespl "source:modland"`.
   Fix: a source tile opens that source's own page (grid/drill), the same target as the sidebar source row, and never
   goes through the search scope. Alternative: fire with library scope for that one search without changing the pref.
   Choice is Johan's (see Open); the recommendation is the source page.

### I903: top bar search from a source page is slow

Library search is already posted (`rd_search`), so it is not a direct UI wait. Hypotheses: the restored online scope
from `search.last_source` sends the search online; or the source page's VIEW_SOURCE/VIEW_WINDOW rebuild does per-row
`art_id` (2b). Measure first with `sample` of the Mac app during the search.

## 3. Debug guard design

Goal: in debug builds the UI thread can never wait on the library without the build saying where.

- Mark the thread: `apps/selecta/main.c` calls the existing `sel_library_watch(1)` on the shell loop thread after the
  library opens (all UIs pump on that thread). No new flag; `g_watch` already exists.
- Check: in `sel_library_request` (worker.c:173), under `!NDEBUG`, when `g_watch && !g_nowait && !g_allow`: print
  `library_ui_wait: req=<id>` with `backtrace_symbols_fd`, then act by mode. Same shape as `owner_check`.
- Mode: env `SELECTA_LIB_UI_GUARD` = `log` (print and count, keep running) or `abort` (print and abort). Default in
  debug is `log` until I906i, then `abort`. Release builds compile it out.
- Allow list: `int sel_library_ui_allow(int on)` (returns previous, like `sel_library_nowait`) sets a `__thread
  g_allow`. A site that must wait wraps the call and carries a comment naming why (example: shutdown flush). The allow
  list is the set of `sel_library_ui_allow(1)` call sites, counted by grep and by a counter `g_allowed_waits`; the goal
  is zero.
- Also cover: `art_load`'s start-then-join (a thread join on the UI thread is the same wait); add the same check in
  `art_thread_join` when the caller is watched. `job_lock` holds stay short and are exempt.
- Sentinel for tests: `sel_library_watched_waits()` already counts; tests assert it stays 0 across the scenario.
- Pump-inclusive timing: add `pump_ms_max` measured around `pump_turn` in main.c (or inside `sel_engine_pump`) and
  expose it next to `turn_ms_max`, so dui tests see waits inside `cmd_*` handlers.

## 4. Rows (each one sonnet agent, each with its own test)

Common test shape: a dui test (or engine test where the site has no desktop path) with a fixture that keeps the worker
busy (a long scan or a test hook that holds `call_lock`), drives the real entry (page open, menu click, command), and
asserts `pump_ms_max < 50`, `turn_ms_max < 50`, and `sel_library_watched_waits() == 0` across the scenario, plus that
the page shows the data after the worker frees (sentinel that the posted read ran).

Order and reasoning: the guard and the pump-inclusive measure first, because without them no other row can prove done
and the log mode lists the remaining sites from structure. Then the two rows Johan reported (I905, I904), then the
reads on every page, then the writes, then the long tail, and last the switch to abort.

| Row | Scope | Test |
|---|---|---|
| I906a | Guard in log mode (section 3), `sel_library_watch(1)` on the shell loop thread, `pump_ms_max` measure, busy-worker fixture hook | engine test: watched thread waiting prints `library_ui_wait` and counts; dui test reports pump_ms_max; log mode run of the dui suite writes the site list to the ledger |
| I906b | I905: FORMATS_SHOW and COMPOSERS_SHOW on read slots; Modland tile opens the source page (after Johan's choice) | dui test: open Browse while worker busy, pump_ms_max < 50, formats appear after release; dui test: Modland tile with `search.last_source=modulespl` sends no SOURCE_SEARCH and shows Modland |
| I906c | I904: art_load async (no join, collect in desk_wait); desk_library_follow reload uses posted reads only | dui test: Save to Library on an online row while worker busy, pump_ms_max < 50, row appears in library after release, watched_waits == 0 |
| I906d | Song panel read slot on new song; view art ids batched per view on a read slot | dui test: start song and scroll a 500-row view while worker busy, < 50 ms |
| I906e | Direct desktop calls (2c): queue groups in engine queue state, song_title from row, playlists from sidebar cache, items via li_thread, song_meta read slot | dui test per entry group: queue change, remove dialog, add-to-playlist submenu while worker busy |
| I906f | Write queue on the job thread for all write commands and write helpers (2a writes, 2b history/loudness/votes/sync_time/addf), `LIBRARY_RESULT` event | engine test: every write command while worker busy returns within 50 ms, write lands after release; dui test like/rate/add to playlist |
| I906g | Remaining reads: run_search callers, STATS_SHOW, COLLECTION_OPEN, STICKER_GET/LIST, OPEN read_song/best_source/fetch, radio, order, drill, playlist_play | dui test: play a song, open stats, open a collection, play a query while worker busy |
| I906h | I903: sample the Mac app during source-page top-bar search, then fix the measured cause | test written from the measured cause |
| I906i | Guard default to abort; allow list empty; whole dui suite green | dui suite run in debug with abort mode; grep for `sel_library_ui_allow(1)` returns 0 sites |

Done for I906 (from the ledger): guard on in debug, dui tests green with an empty allow list, a dui test proves
`turn_ms_max` (and `pump_ms_max`) < 50 ms during a scan, Johan Mac checks of Browse, Save to Library and search.

Amiga: posted reads run inline there (`SEL_LIBRARY_NO_THREAD`), so the changes keep Amiga behaviour; the guard is a
no-op without threads.

## Open

- `tests/dui` harness was not checked for an existing `turn_ms_max` assertion or a busy-worker fixture; I906a must
  look before adding one (`tests/engine/library_ui_nowait_test.c` is the closest existing sentinel test).
- Locking of `hash_poll`, `prefetch_poll`, `lyrics_result` not re-verified.
- I904 cause is from code reading only; no `sample` taken. I906c should sample first to confirm which reload read
  blocks longest.
- Modland tile target (source page versus library-scope search) is Johan's decision.
- Command counts in section 2 are from the call-line scan, not from the dispatch table; I906a's log mode gives the
  structural count.
