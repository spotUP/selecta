# M3 Phase 5: I167a (companion exemption in the LRU trim) and I168 (next-song prefetch)

Branch `worktree-agent-ae1f8d6755f0506a2`. Tests T76 (cache-budget, cache-budget-fetch) and T77 (prefetch).

## I167a: companions of kept and liked songs survive the trim

- `sel_cache_trim` takes a `rules` argument (companion rules, `SEL_RP_COMPANIONS`). After marking kept, liked and
  just-stored files exempt, `mark_companions` marks every companion of an exempt entry exempt, using the existing
  `sel_companion_paths` lookup (no second rule parser). `sel_cache_trim_result.companion_exempt` counts them.
- `sel_library_fetch` remembers the rules it was given (`lib->rules`), so the trim it runs afterwards knows the
  companion model. `library trim` gets `--rules <rules>` for the CLI path.
- T76 (`tests/m3/cache_budget.sh`): kept `mdat.x` keeps its older `smpl.x`, the unmarked file goes, `companion_exempt=1`.
- T76 part 2 (`cache-budget-fetch`, `tests/net/cache_budget_fetch.cmake`, script `tests/m3/cache_budget_fetch.sel`):
  LIBRARY_FETCH of a kept TFMX song over the fixture mirror with a 10 byte budget. `trim_runs` moves (the sentinel for
  the fetch.c wiring), the song and its `smpl.*` stay, a seeded old unmarked file is evicted.
- Script-only directives added to `selecta-cli script`: `@mark <kept|liked>:<key>`, `@budget <bytes>`, `@prefetch_wait`.

## I168: prefetch the next queued remote song

- Engine: `prefetch_sync` (called in `sel_engine_pump` after `prepare_next`) asks for the queue's next entry when the
  engine is playing, repeat is not "one", and the entry is remote (`sel_cache_is_remote`: `modland:` and `hvsc:`).
  It re-requests when the entry id or the queue version changes, and cancels when the next entry is not remote or
  there is none.
- Engine `read_song` now resolves remote keys through `sel_library_fetch` (before: plain `fopen`), so a prefetched
  song is a cache hit when it starts. No second downloader: the prefetch thread calls `sel_cache_fetch_song`, the
  same code `sel_library_fetch` uses.
- Library: one low-priority prefetch thread (`sel_library_prefetch`, `_cancel`, `_wait`, `_stop`). One request at a
  time. Cancel drops a pending request; a download already running finishes (its file is valid cache content).
  `cache_lock` serialises cache access between the prefetch thread and `sel_library_fetch`. The prefetch thread never
  trims and never touches the database. `sel_library_close` stops it first.
- Counters: `prefetches` (requests accepted), `prefetch_fetches` (downloads the prefetch did); the script report prints
  `trim_runs`, `prefetches`, `prefetch_fetches`.
- T77 (`prefetch`, `tests/net/prefetch.cmake`, `tests/m3/prefetch.sel`): queue play1.mod and play2.mod (new playable
  fixture files on fast.mirror.test, copies of the gen_testmod song), PLAY, wait for the prefetch thread, NEXT.
  Asserts `prefetch_fetches=1`, `prefetches>=1`, `fetches=2`, `cache_hits>=1`, exactly one request per file in the
  fixture log (play2 was requested only by the prefetch).

## Open

- Amiga: `SEL_LIBRARY_NO_THREAD` builds get no-op prefetch stubs (no pthreads); the Amiga build compiles and links.
  Prefetch on Amiga needs a cooperative or exec-task variant. Open.
- Prefs `library.prefetch` and `library.metered` (on/off, metered networks) belong to I169 and are not implemented;
  prefetch is always on when the next entry is remote.
- I167a and I168 share files (cache.c, fetch.c, db.h, main.c); the work was done in one tree, so the commits split by
  test and report, not by hunk.
- A prefetch of a very large song blocks a foreground `sel_library_fetch` on `cache_lock` until it ends.

## Results

Host: `ctest -R 'cache|fetch|prefetch|queue|library|mirrors'`: 19 of 19 passed. Amiga build (68020, Debug): builds and
links. `selecta-cli commands --unimplemented | wc -l` = 17 (unchanged).
