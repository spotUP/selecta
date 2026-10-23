# I173d: one library background-job thread

## Changes
- New `src/library/db/job.c`: the one background job thread (`sel_job_start/_wake/_wait/_stop`, one lock `job_lock`, one cond `job_wake`). Loop order: rating flush, then prefetch, then smart downloads.
- Queue is fixed size, in the library handle, no per-job allocation: one prefetch slot (`pf_spec`), `SEL_SMART_MAX` smart slots (`sm_spec`), one rating flag (`job_rating`). A repeated rating flush request sets the same flag, so it coalesces into one pending job.
- `fetch.c`: `prefetch_main` and its pthread scaffold removed; now `sel_fetch_job_pending` / `sel_fetch_job_run` (called by the job thread). Public calls unchanged (`sel_library_prefetch`, `_cancel`, `_wait`, `_stop`, `_poll`). The Amiga no-thread path (cooperative `sel_library_prefetch_poll`) is untouched.
- `rating_push.c`: `push_main` and the rp_* scaffold removed; `sel_library_rating_push` queues the flag, `_wait` calls `sel_job_wait`; `sel_rating_job_run` (was `flush_pending`) is the job body. No-thread build still flushes inline.
- `db_internal.h`: pf_thread/pf_lock/pf_wake/pf_started/pf_quit and all rp_* fields removed; job_* fields added. `sel_library_rating_stop` removed (db.h, db.c); `sel_library_close` calls `sel_library_prefetch_stop` which now calls `sel_job_stop` (stops the whole job thread). `CMakeLists.txt` adds `db/job.c`.
- Callers (engine.c, CLI hooks `@ratings_wait`, tests) unchanged.

## Thread count (pthread_create in src/library/db, counted by grep)
- Before: 3 (worker.c DB worker, fetch.c prefetch, rating_push.c push).
- After: 2 (worker.c DB worker, job.c background jobs). One pthread_create left for background jobs: job.c.

## Close
`sel_job_stop`: sets job_quit and pf_cancel, wakes, joins. Queued jobs are dropped (pending ratings stay flagged in the DB and are re-pushed at the next engine create). A prefetch/smart download in flight stops at its next chunk. A rating push in flight is bounded by the net timeouts: connect 3 s, first byte 3 s, total 30 s (`SEL_HTTP_*_MS`). Worst-case close time: about 30 s (total timeout) for one stuck push, typically 3 to 6 s. The flush pass is not interrupted between rows (a pass sends at most the 2048-byte buffer of rows, and stops at the first unreachable one). The task wording "not beyond the net timeout" is met in the sense of the per-request total timeout.

## Tests (build/host, nice 19)
- PASS: cache-fetch, prefetch-* cases, rate-devilbox, net-guard, loudness-fetch (9 of 9 in that selection), smart-downloads, http-client.
- Amiga build (`build/amiga`) links: PASS.
- No new test for close-behind-stuck-push; the bound comes from the net timeouts, not verified by a timed close test.

## Open items
- `sel_job_wait` waits for the whole queue; `sel_library_prefetch_wait` therefore also waits behind a rating flush.
- A single thread means a stuck push delays prefetch (and the reverse) by up to the net timeout.
- `pf_cache` is never freed (pre-existing).
