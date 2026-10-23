# M3 Phase 5: I168a, a foreground fetch never waits behind a prefetch

## Analysis (written before the first edit)

Violated invariant: no lock is held across a network transfer, and a user-initiated fetch is never queued behind a
background one. I168 broke it: `prefetch_main` took `cache_lock` around `sel_cache_fetch_song`, and
`sel_library_fetch` took the same lock, so a foreground fetch (the user playing or skipping to a remote song) blocked
for the whole remaining prefetch download. `sel_library_prefetch_cancel` only dropped a request that had not started.

Root cause level: the lock protects the wrong thing. The shared cache state (stats, mirror ranking) was guarded by
holding the lock over the whole download, instead of the download working on state of its own. Fix at the cache
ownership level, not with a timeout or a try-lock in the caller.

Measured wait today (fixture `slow.mirror.test`, `slowpf.mod` answers after 5 s, prefetch started about 0.5 s before
the foreground fetch of `quick2.mod`): the foreground `LIBRARY_FETCH` took 2553 ms with the old fetch.c. The wait is
the rest of the prefetch stall and grows with the file size and link speed.

## Fix

- `sel_cache` gets `part_tag` (temporary name suffix) and `cancel` (a flag checked by every body chunk and before a
  restart). A cancelled download returns FAILED, removes its `.part` and skips the remaining companions.
- The prefetch thread has its own `sel_cache` handle (`lib->pf_cache`): same directory, own mirror ranking and stats,
  temporaries named `*.pf.part` (the trim already skips `*.part`), so it never shares a temporary with a foreground
  download. It takes `cache_lock` only to merge its stats into the shared handle after the request, never during a
  transfer.
- `sel_library_fetch` calls `prefetch_preempt`: drops the pending request and sets `pf_cancel`; the running download
  stops at its next chunk and the foreground fetch proceeds at once. `sel_library_prefetch_cancel` (engine, next entry
  changed or not remote) and `_stop` use the same path. If the foreground wants the very file being prefetched, it
  downloads it itself (the prefetch is aborted, not awaited).
- Known limit: a prefetch stalled before its first byte cannot be interrupted, only abandoned; `sel_library_close`
  joins the thread and so waits for the transport timeout. The foreground never waits.
- Amiga (`SEL_LIBRARY_NO_THREAD`): real path instead of no-op stubs. `sel_library_prefetch` parks the request,
  `sel_library_prefetch_poll` runs it (the same `prefetch_run` as the thread, on the shared cache handle), and the
  engine calls the poll in `sel_engine_pump` right after `prefetch_sync`, that is on the tick after playback started.
  A foreground fetch drops a parked request. Limit: the transfer is one blocking call inside that pump; it cannot be
  pre-empted and costs the pump the transfer time. Slicing the transfer (HTTP Range chunks with a pinned mirror and a
  carried md5 state) is open and needs the transport to expose a step interface. Unverified on hardware or the rig;
  the Amiga build compiles and links.

## Test (T77 part 2, `prefetch-preempt`)

`tests/net/prefetch_preempt.cmake`, `tests/m3/prefetch_preempt.sel`, fixture files `quick1/quick2/slowpf.mod` on
`slow.mirror.test` (route `slowpf.mod`: `delay_first_byte` 5, in the existing fixture server). New script hooks in
`selecta-cli script`: `@sleep <ms>` and `@timed <line>` (prints `timed_ms=`), both host only. The test queues quick1 and
slowpf, plays, waits 500 ms, times `LIBRARY_FETCH quick2`, and asserts `timed_ms <= 1500`, quick2 in the cache and the
cancelled slowpf absent. The reachability sentinel is the engine-driven prefetch itself: with the old code the same
script waits on the lock.

- Before the fix (old fetch.c, new test): FAILED, `foreground fetch took 2553 ms (bound 1500)`.
- After the fix: passed, `timed_ms=4`.
- Reverting the fix makes it fail again (the "before" run was exactly that revert).

## Results

Host `ctest -R 'prefetch|cache|fetch' -E 'vamos|rig'`: 8 of 8 passed (includes T77 `prefetch`, T76 parts 1 and 2).
Amiga build (68020, Debug, `build/amiga`): links. `selecta-cli commands --unimplemented | wc -l` = 17 (unchanged).
