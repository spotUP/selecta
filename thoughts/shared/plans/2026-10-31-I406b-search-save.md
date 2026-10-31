---
date: 2026-10-31
topic: Online search results savable into the library (I406b): a played, queued or playlisted Modules.pl hit is pinned into library.db
tags: [plan, design, source, search, modulespl, library, I406b, D-OS-4, D-YT-3]
status: design, awaiting Johan's answers to section 11; no code yet
rows: I406b in thoughts/spot/todos.md (depends on I406a; respects I782)
---

# I406b: save online search hits into the library

Implements D-OS-4 (`2026-10-26-online-sources.md`:167) with the D-YT-3 pattern (`2026-10-26-youtube-source.md`:27):
search results are transient; a hit the user acts on is pinned into `songs` as `<source>:<path>`, so history, ratings,
playlists and the library list keep it after a restart. No YouTube code exists to reuse; everything below reuses
Selecta's own scan-write and job-thread machinery.

Finish line: after `SOURCE_SEARCH modulespl chip` and a click that plays row 0, a restart with the same `library.db`
shows one `modulespl` song in the library; `dui-search-online` asserts it through `--ui desktop --backend mem`.

## 1. Facts measured in the code (2026-10-31, main 748e0e99)

- Hit metadata today lives in the engine: `sel_engine.search_hit[SEL_SEARCH_HITS]` (`src/engine/engine.c`:472) holds
  `key` (`modulespl:<dir>/<file>.zip`), `title`, `artist`, `format` per hit. It is filled by `search_row` on the
  library job thread (`search_task`, engine.c ~5806-5852) while `search_running`, and stays valid until the next
  `search_start` resets `search_n`. The desktop keeps its own copy in `desk.on_row[120]` (`desktop_ui.c`:338) for
  drawing only.
- The sink carries no size: `sel_search_sink.row(ctx, path, title, artist, format)` (`include/selecta/source.h`:49).
  `songs.size` will stay NULL for pinned hits (scan.c binds `size <= 0` as NULL).
- A play of a hit is `OPEN modulespl:<path>` + `PLAY` (`desk_play_path`, desktop_ui.c:12750, called at :8429). The
  engine opens it through the normal remote path (mirror `SELECTA_MIRRORS_MODULESPL`, unzip in `sel_cache_fetch`);
  no songs row is needed to play. The online rows have no song menu yet, so Queue/Playlist come only from the control
  socket, CLI scripts or a later UI row.
- `cmd_OPEN` (engine.c:2388), `cmd_QUEUE_ADD` -> `queue_insert` (:2955), `cmd_PLAYLIST_ADD_SONG` (:5259) and
  `search_collect` (:7470) all run on the same engine pump thread (`dispatch` loop, engine.c ~7456). The only
  concurrent writer of `search_hit` is `search_task` while `search_running` is set.
- `SEL_LIB_REQ_SCAN_BATCH` (`src/library/db/scan.c`) writes `files` (INSERT OR REPLACE) and `songs`
  (`INSERT ... ON CONFLICT(key) DO UPDATE`, user tags kept by `SEL_SONG_TAG_KEEP`, `added` untouched on conflict) in
  one transaction; key = `"%s:%s", source, path`. It is idempotent per key.
- I782 constraint, precisely: `sel_library_request` takes `call_lock`, which a running sync holds for its whole job
  and between its requests (`worker.c`:135). On the engine thread a library request during an HVSC sync blocks the
  pump for minutes; under `sel_library_nowait(1)` it try-locks and fails (`lib_deferred`). Neither is acceptable for a
  pin, so the write must not run on the engine thread.
- The library job thread (`src/library/db/job.c`) already runs small queued jobs beside the long caller task:
  `job_rating` (`rating_push.c`), `job_vote`, `job_lyric`, `job_hash`, each a flag in the library handle, woken by
  `sel_job_wake`, run with `job_lock` dropped, and run inline in builds without threads (Amiga).
- MUI and text UIs have no online search (`grep SOURCE_SEARCH plugins/ui/mui plugins/ui/text`: none).

## 2. Where hit metadata lives after the search ends

Unchanged: in `e->search_hit[]`, valid until the next `SOURCE_SEARCH` starts. The pin copies the hit into a library
owned queue at the moment of the user action (section 4), so a later search that overwrites `search_hit` cannot lose
or corrupt a pin. Nothing new is kept per hit in the engine or the UI.

Lookup rule: a key is a hit when `!e->search_running` and some `search_hit[i].key` equals it (linear scan over at most
120). While a new search runs, an action is not matched (the table is being rewritten by the job thread); the UI only
shows rows of a finished search, so this window is a re-search race, not a normal path.

## 3. Which event saves it (recommended: play, queue and playlist add; no explicit Save)

Recommended: the engine pins a hit on every user action that commits to it: `OPEN` (play), `queue_insert` (Add to
Queue, Play Next, any path that inserts into the queue) and `PLAYLIST_ADD_SONG`. This is D-OS-4 as approved, plus the
playlist add D-YT-3 names (a playlist entry with no songs row has no title after restart).

Trade-off: the library collects every hit the user merely sampled once; an explicit Save would keep it curated but
needs a new UI affordance and a new command, and plays/queues would still lose history and ratings after restart
(history rows keyed to a song that the library list cannot show). D-OS-4 already chose the automatic pin; an explicit
"Save to Library" item can be added later on top of the same pin call without any change here (open decision Q1).

The engine is the single choke point, so every UI (desktop, a future MUI search, control socket, CLI scripts) gets the
same behaviour. The UI sends no new command.

## 4. Which thread writes, through which existing request

- Engine (pump thread): `search_pin(e, key)` finds the hit (section 2) and builds one `sel_scan_row`
  {source = `e->search_source`, path = key after the colon, name = title, artist = artist, format = hit format,
  album = "", md5 = NULL, size = 0, duration_ms = 0, year = 0}. It calls a new library entry
  `sel_library_pin(lib, const sel_scan_row*)` and counts `e->source_pins++`. It never waits.
- Library: `sel_library_pin` copies the row into a fixed ring in the library handle under `job_lock`
  (`pin_q[SEL_PIN_MAX]`, string buffers sized like `search_hit`: key/path, title, artist, format; `SEL_PIN_MAX` 32),
  sets `job_pin = 1` and wakes the job thread, the same shape as `sel_library_rating_push` (`rating_push.c`:80).
  A full ring drops the row and counts `pin_dropped` (only reachable when more than 32 actions queue behind a long
  sync).
- Job thread (`job.c` `job_main`): a new `job_pin` branch next to `job_rating` takes the queued rows out under the
  lock, then sends them as ONE `SEL_LIB_REQ_SCAN_BATCH` with `new_day = 0` (pins are not "New" in the source page)
  through `sel_sync_request`/`sel_library_request` with the lock dropped, and adds the reply's rows to `pin_rows`.
  It waits behind a running long task (a sync) like every other job kind does; a pin made during an HVSC sync lands
  when the sync ends. The engine thread is never blocked (I782).
- Builds without threads (Amiga): `sel_library_pin` writes inline, like `rating_push.c`:100, one 1-row SCAN_BATCH on
  the caller.
- Close: `sel_library_close` drains any rows still in `pin_q` inline after the job thread is joined and before the
  worker stops, so a pin made just before quit is not lost.
- Engine refresh: the pump compares `sel_library_pin_rows(lib)` with `e->pin_rows_seen`; when it moved it bumps
  `library_changes` / `state.library_serial` (the same thing `emit_library_path` does, engine.c ~3636) so open library
  listings and the Sources count reload.

No new library request id, no new command, no new thread. New code is one job kind (flag + ring + drain function) and
one engine helper, both copies of existing patterns.

## 5. Schema impact on library.db

None. `files` and `songs` already take any source id; `source = 'modulespl'`, `path = '<dir>/<file>.zip'`,
`key = 'modulespl:<dir>/<file>.zip'`. No migration, no new table.

## 6. How a saved remote row shows and plays after restart

- Library Songs list, search and Sources page: an ordinary row with the site title, artist and format; the Sources
  page `modulespl` row shows `SRC_COUNT` = pinned songs (today 0). No UI code change expected; verify in step 8.
- Play: the library row sends `OPEN modulespl:<path>` exactly as the search click does; the remote fetch path, mirror
  and unzip from I406a run unchanged; a cached zip plays offline. No search is needed.
- History, likes, ratings, stickers and playlist entries key on the same `songs.key`, so rows written for the song
  before the pin (history from I406a plays) now join to a visible song.

## 7. Dedupe on key

`songs.key` is unique; SCAN_BATCH upserts (`ON CONFLICT(key) DO UPDATE`), keeps user-edited tags, keeps `added`.
`files` is INSERT OR REPLACE on (source, path). Pinning the same hit twice writes the same row; no engine-side
"already pinned" set is needed. A song that exists in the library already and is played from the library is not a hit
of the current search unless the same key is in `search_hit`; then the upsert rewrites the same values (harmless,
one row).

Not deduped: the same module under another source (e.g. a modland copy). Cross-source twins stay the md5 job of
D-M3-14; pinned hits have no md5 until fetched (out of scope, open decision Q3).

## 8. Amiga (MUI) impact

- MUI has no online search UI, so nothing to change there now. When MUI gains a search page it gets the pin for free
  (engine side).
- Amiga library build has no threads: the pin writes inline on the engine task (one 1-row transaction, the same cost
  class as a rating flush). The ring is ~32 x ~600 bytes, about 19 KB in fast RAM inside the library handle; fine per
  the fast-RAM baseline memory. A smaller `SEL_PIN_MAX` on Amiga is not needed because the inline path never queues.
- No rig run for this row: there is no Amiga entry point that reaches the pin. Record that in the ledger.

## 9. Reachability test (one, through the real entry point)

Extend `tests/dui/search_online.cmake` (`dui-search-online`): `--ui desktop --backend mem --view search --stats`,
scripted `--input`, recorded `www.modules.pl` fixture. The `library.db` in `${WORK}` already persists across passes 2-4
(pass 1 deletes it; passes 2-4 do not).

- Sentinel: a new stats field from the `--stats` `SOURCESEARCH` line (`apps/selecta-cli/main.c`:513), extended to
  `SOURCESEARCH runs=<n> searches=<n> worker=<n> pins=<n> pin_rows=<n>`. `pins` = `e->source_pins` (the engine hook
  ran), `pin_rows` = rows the job thread's SCAN_BATCH wrote (`sel_library_pin_rows`). If the desktop run does not print
  that line (it prints from selecta-cli main), add `pins=` and `pin_rows=` to the `online ...` dump line instead.
- Pass 2 (search only, no click): assert `pins=0` and `pin_rows=0` (search alone saves nothing).
- Pass 3 (click row 0, plays=1): assert `pins=1` and `pin_rows=1` (needs the job drained before exit: the existing
  `wait 200` plus close's inline drain make it deterministic).
- Pass 4 (restart, same db, no search): assert the dump line `source <i> modulespl|<kind>|Modules.pl|<enabled>|1`
  (count exactly 1, the `source` dump line at desktop_ui.c:6679) and that pass 4's `SOURCESEARCH ... searches=0`
  (the row came from the library, not a new search).
- No extra pass plays the saved row from the library: that is the unchanged `OPEN modulespl:<path>` path pass 3
  already drives.
- Revert proof: remove the `search_pin` call from `cmd_OPEN`; pass 3 must go red on `pins=1` and pass 4 on count 1.
  Record red/green in the ledger row.

Behaviour tests where they live (not reachability):
- `tests/net/modulespl.cmake` (CLI script, `@search_wait`): `SOURCE_SEARCH modulespl chip`, `QUEUE_ADD` of hit 1,
  `PLAYLIST_ADD_SONG` of hit 2, `QUEUE_ADD` of hit 1 again; then a second CLI run on the same db lists songs of source
  `modulespl`: exactly 2 rows, titles equal the fixture's hit titles, `pin_rows=3` (the repeat upserts, no new row).
- A library unit test for `sel_library_pin` ring overflow: 33 pins with the job thread held -> `pin_dropped=1`,
  then 32 rows written; and close drains a non-empty ring.

## 10. Build steps (for a sonnet agent; design sections in brackets)

1. Read this doc, `src/library/db/job.c`, `rating_push.c`, `scan.c` header, `db_internal.h` lines 70-95. No code yet.
2. `src/library/db/db_internal.h`: add `job_pin, job_pin_busy` flags, `pin_q[SEL_PIN_MAX]` (a struct with own string
   buffers plus a `sel_scan_row` view), `pin_n`, `pin_rows`, `pin_dropped`. [4]
3. `src/library/db/pin.c` (new, beside `rating_push.c`; add to the library CMake source list):
   `sel_library_pin` (copy under `job_lock`, set `job_pin`, `sel_job_wake`; no-thread build: inline request),
   `sel_pin_job_run` (take rows under lock, one `SEL_LIB_REQ_SCAN_BATCH` with `new_day = 0`, add reply `args[0]` to
   `pin_rows`), `sel_library_pin_rows`, `sel_library_pin_dropped`. Declare them in `src/library/db/db.h` next to
   `sel_library_rating_push`. [4]
4. `src/library/db/job.c`: add `job_pin` to the wait condition, a branch after `job_rating` that runs
   `sel_pin_job_run` with the lock dropped, and to the idle wait at :103. [4]
5. `src/library/db/db.c` `sel_library_close`: after the job thread joins and before the worker stops, run
   `sel_pin_job_run` once if `pin_n > 0`. Check the actual stop order there first. [4]
6. `src/engine/engine.c`: `search_pin(e, key)` (section 2 lookup + section 4 row), called from `cmd_OPEN` (after the
   share-link branch, before `begin_song`), `queue_insert` (once per inserted key) and `cmd_PLAYLIST_ADD_SONG`;
   `uint32_t source_pins`, `pin_rows_seen`; in the pump after `search_collect`, the `library_changes` bump when
   `sel_library_pin_rows` moved. Confirm `sel_song_key_of("modulespl:a/b.zip")` returns the same string as the scan
   key `source:path`, else the history join in section 6 fails; note the answer in the ledger. [2, 3, 4, 6]
7. `include/selecta/engine.h` stats struct: `source_pins`, `pin_rows`; fill in the stats getter (engine.c ~7666);
   `include/selecta/tags.def`: `STAT_SOURCE_PINS` next to `STAT_SOURCE_SEARCHES`; `apps/selecta-cli/main.c`:513 prints
   `pins=` and `pin_rows=`. [9]
8. `tests/dui/search_online.cmake`: passes 2/3/4 assertions of section 9; update the header comment list. Run only
   `dui-search-online`, `source-modulespl-net`, `modulespl-parse` and the new unit test (nice 19, 2 workers). [9]
9. `tests/net/modulespl.cmake`: queue + playlist + repeat case of section 9. [9]
10. Revert proof (section 9), type-check/build clean, ledger row I406b updated with commit, red/green and the Amiga
    "no entry point" note. Johan Mac check: search Modules.pl "chip", play row 0, quit, relaunch, the song is in
    Library > Songs and plays.

## 11. Open decisions for Johan (recommended first)

- Q1 Save event: (a, recommended) automatic pin on play, queue and playlist add, as D-OS-4 says; no Save button.
  (b) the same plus an explicit "Save to Library" song-menu item on online rows (adds the online-row song menu first).
  (c) explicit Save only: curated library, but history and ratings of unsaved plays point at songs the library
  cannot show.
- Q2 Format stored: (a, recommended) the site's own format string as the hit has it (`mod`, `xm`), consistent with
  what the Search page showed; (b) map it to Selecta's format names at pin time (needs a mapping table; better
  grouping in Formats, more code).
- Q3 md5 for pinned hits: (a, recommended) none now; a later row fills md5 on first fetch so D-M3-14 can find local
  twins; (b) fetch and hash at pin time (a network download per pin, slows the pin and costs bandwidth for
  songs only queued).
- Q4 Pins count as "New" on the Modules.pl source page: (a, recommended) no (`new_day = 0`), they are the user's own
  picks; (b) yes, so the source page shows today's saved hits under New.

## 12. Decisions (Johan, 2026-10-31)

- Q1: (b) automatic pin on play, queue and playlist add, plus a "Save to Library" song-menu item on online rows
  (Mac desktop now; the MUI item follows in a later row). Both call the same `search_pin`.
- Q2: (b) map the site's format string to Selecta's library format names at pin time, the same names a local scan
  stores in `songs.format` (Modland style): `mod` -> `Protracker`, `xm` -> `Fasttracker 2`, `it` -> `Impulsetracker`,
  `s3m` -> `Screamtracker 3`. Reuse the existing extension/format table the scanner uses; do not write a second one.
  An unmapped string is stored as-is.
- Q3: (a) no md5 at pin time; filled on first fetch by a later row.
- Q4: (a) pins are not "New" (`new_day = 0`).
