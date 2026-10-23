# I170a report: smart.sel history seed, KEEP test, deterministic play-query

## Changed
1. `tests/m3/smart.sel` no longer has `@mark history:` lines. New `tests/m3/smart_history.sel` plays play2 to its end twice
   (`@render_until_end`), so the engine's SONG_ENDED writer records two plays. `tests/net/smart.cmake` runs it first per run
   with its own throw-away cache dir and a per-run `--db`, then runs smart.sel over the same DB (cache fresh, so the smart plan
   still downloads play2 and `fetches=3` holds). The `history` branch of `sel_library_mark` (src/library/db/fetch.c) is gone;
   nothing else used it (`@mark` in main.c and `library trim --mark` only pass kept/liked).
2. New test `keep-command` (`tests/net/keep.cmake`, `tests/m3/keep.sel`): play1 plays, KEEP is sent, budget 10, LIBRARY_FETCH of
   play2 runs the trim. Run A (KEEP): play1 survives, the old unmarked file is evicted. Run B (no KEEP): play1 is evicted.
   Observable is the cache trim, because `kept` has no query field and no report (`liked` has `liked:yes`, `kept` has nothing).
3. `selecta-cli script` now calls `sel_library_prefetch_wait(lib)` before `sel_library_cache_report(lib)`. The smart plan
   runs on the engine thread but the job thread mutates the same `cache->stats` (prefetch/smart downloads), so the report was
   read while the counters could still change. `play_query.sh` no longer filters the cache lines; `play_query.expected` now
   contains the 11 counter lines (all 0 there).

## Measured
- Host build and Amiga build: 0 warnings/errors outside third_party. `commands --unimplemented | wc -l` = 8.
- ctest host, `keep-command|play-query|smart-downloads|user-data|cache-budget`: 8/8 passed (smart-downloads unchanged assertions).
- play-query output identical in 20 consecutive runs (idle machine) with the wait.

## Revert checks
- smart: smart_history.sel emptied -> `T77 smart A smart_fetches: got '1', want '2'`. Restored.
- KEEP: cmd_KEEP body disabled -> `T78 keep A: play1 was evicted although KEEP marked it`. Restored.
- play-query wait: NOT demonstrated. With the wait removed, 40 consecutive runs on an idle machine still gave identical output,
  so the race is only a scheduling hazard here; no failing-before test exists for part 3 (the golden now includes the lines,
  so it would fail if the report ever differed). Not claimed as a proven regression test.

## Open items
- Design doc D-M3-10 (plans/2026-10-25-m3-library.md:112) says `KEEP on|off`; `commands.def` has `KEEP` with no args, so KEEP can only
  set, never clear. Not redesigned. There is also no `kept:yes` query field; adding `{"kept", K_BOOL, "kept", 0}` to
  src/library/query.c would allow a direct assertion (a decision for you).
- The worktree needed symlinks for third_party/{libopenmpt,nuade} (empty dirs); they are not committed. `git status` errors on them
  (submodule path is a symlink), so commit used explicit file adds.
