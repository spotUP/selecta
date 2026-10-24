# I168b: incremental, resumable no-thread prefetch

Design doc section 9.3. Plan: 2026-10-25-m3-library.md.

## Violated invariant

No single `sel_engine_pump` tick may block longer than a bounded slice, and a partial download must stay
resumable and adoptable. On the Amiga (SEL_LIBRARY_NO_THREAD) `sel_library_prefetch_poll` ran one blocking
`sel_cache_fetch_song` per tick; the transfer could not stop mid-way, a stall before the first byte held the
call for the transport timeout, and a foreground fetch of the same file started over.

## Root cause level

The cache fetch (`src/library/cache.c`) had no notion of a partial transfer: the part file was deleted on any
non-OK result and the mirror loop could not be told to stop after one attempt. Slicing therefore lives in the
shared cache and mirror code (testable on host); only the poll glue in `db/fetch.c` is Amiga-only.

## Fix

- `net/mirrors.c`: `sel_mirrors_fetch_opts` with `from` (Range start; restarts cleanly when the server answers
  200 instead of 206), `stall_ms` (connect and first-byte limit), `max_tries` (attempts per call), `yielded`
  (a deliberate stop is not a mirror failure) and `resumable` (HTTP yes, FTP no).
- `cache.c`: `sel_cache.slice_ms`. A fetch with `slice_ms > 0` stops at the first chunk boundary past the
  deadline, keeps the part file, md5 state and offset in `sel_cache.slice`, and returns `SEL_CACHE_PENDING`.
  The next fetch of the same key adopts it (Range request); a different key drops it. A sliced failure tries
  the next ranked mirror from byte 0 and ends FAILED after one failure per mirror.
  `sel_cache_slice_drop` discards the partial download.
- `db/fetch.c` (Amiga only): the poll keeps its request in `pf_cur` across ticks, runs it with
  `slice_ms = 100`, returns 1 while PENDING. `prefetch_preempt` no longer touches the partial download, so a
  foreground fetch of the same file adopts it. `prefetch_stop` (library close) drops it.

## Measurements (host, loopback server, 200 kB body over 1 s, test `cache-slice`)

| Case | Before (slice_ms = 0, old behaviour) | After (slice_ms = 100) |
| --- | --- | --- |
| Pump tick on a slow download | one call 1031-1047 ms | worst call 135-250 ms over 9 calls |
| Stalled before first byte | one call 3004-3018 ms | one call 1005-1007 ms |
| Foreground fetch after a partial prefetch | full re-download (Range from 0, 230000 bytes sent for a 200000 byte file) | Range from 24096, 2 requests, 205904 bytes sent |

## Tests (ctest, label m3, all in `cache-slice`, `tests/library/cache_slice_test.c`)

- `pump_tick_stays_short_during_slow_download` (defect 1)
- `stalled_prefetch_tick_returns_within_bound` (defect 2)
- `foreground_fetch_adopts_partial_prefetch` (defect 3)

## Revert checks (each fix disabled in cache.c, test built and run, then restored)

1. Deadline yield off: FAIL lines 197, 198, 244, 245, 249, 261, 269.
2. `stall_ms = 0`: FAIL line 223 (sliced stall call 3006 ms).
3. Adoption off: FAIL lines 196, 199, 200, 261, 262, 263 (Range from 0, 230000 bytes sent).

Affected ctest set (cache-slice, prefetch, prefetch-preempt, cache-fetch, cache-budget-fetch,
library-close-cancel and other cache tests): 10 of 10 pass. Host and Amiga builds have no warnings outside
third_party. `selecta-cli commands --unimplemented | wc -l` = 8.

## Caveats

- The poll glue in `db/fetch.c` compiles only for Amiga; it is verified by the Amiga build, not run on host.
  No real-rig run was made (ledger item for the rig).
- FTP mirrors are not resumable and not sliced; they keep the old blocking behaviour.
- The stalled bound is 1 s per tick, not zero. Close cannot overlap a tick in a single task, so its latency is
  at most one slice.
- On re-poll after a finished companion fetch the song is a cache hit, so the `companions` and `cache_hits`
  counters count again.
