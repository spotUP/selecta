# I173c: DEViLBOX HTTP off the engine command thread

## Changes
- New `src/library/db/rating_push.c`: push thread (same shape as the prefetch thread of fetch.c). `sel_library_rating_push` sets a work flag and starts the thread lazily; `_wait` drains it; `_stop` joins it (called from `sel_library_close`). Builds without threads (`SEL_LIBRARY_NO_THREAD`, Amiga) push inline on the caller task.
- `engine.c`: `ratings_flush` removed. `cmd_RATE` stores the rating via the worker, then calls `sel_library_rating_push`. The engine-create flush of leftover pending ratings does the same.
- `worker.c` / `db_internal.h`: recursive `call_lock` so the engine thread and the push thread cannot steal each other's reply or the job slot.
- `src/core/posix/net.c`: the socket slot table was unlocked global state; two threads doing HTTP collided (http err IO/PROTOCOL, failed pushes and failed batch reads). Added a mutex, slot reservation during connect, locked close.
- `devilbox.c`: counters `g_pushes`/`g_batches` atomic on threaded builds, plain on Amiga (no libatomic: first Amiga link failed on `__atomic_fetch_add_4`).
- CLI: `@ratings` now drains the push thread before printing; new hook `@ratings_wait`. `tests/m3/rate.sel` calls `@ratings_wait` after RATE so the batch read sees the pushed rating.
- Test: `tests/net/rate_devilbox.cmake` section 5. Fixture `fixture_server.py` delays a PUT/DELETE of a key containing "slowrate" by 2 s (the client first-byte timeout is 3 s, so 3 s would fail the push). Asserts `@timed RATE 5` and `@timed VOLUME 80` take at most 700 ms, then pushes=1, pending=0, correct PUT body. Fixture file `tests/net/fixtures/fast.mirror.test/mods/Protracker/test/slowrate.mod`.

## Tests
- `rate-devilbox` (T79): PASS, 5 of 5 consecutive runs after the fixes.
- Fail-before: with `engine.c` reverted to HEAD (inline flush), section 5 FAILS: "an engine command took 2008 ms (bound 700)". With the fix the timed commands take 0 to 5 ms.
- Also PASS: net-guard (alone), cache-fetch, http-client, prefetch cases, library/view/text-ui matches (18 selected, only net-guard failed when run with `-j2` because tests share one fixture log; passes alone).
- Amiga build (`build/amiga`): PASS after the atomic fix.

## Open items
- Amiga/no-thread path pushes inline on the caller task (no off-thread there until Phase 12).
- `sel_library_close` joins the push thread, so close can wait up to the HTTP first-byte timeout (3 s) behind a stuck push.
- Other threads that use HTTP (prefetch) now also benefit from the net.c slot lock; handles themselves are still owned by one caller.
