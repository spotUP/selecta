# I173f: library close cancels a stuck net job

Design: architecture doc section 9 (job thread, I173d).

Root cause: the job thread's net waits (poll in src/core/posix/net.c wait_fd) only ended at their timeout, and close joined the thread.

Fix (no second mechanism, the existing poll loop):
- `sel_net_set_cancel(const volatile unsigned*)` (src/library/net/net.h): per-thread cancel flag. posix `wait_fd` now polls in 50 ms slices and fails the wait when the flag is set (connect, send, recv all go through it). Amiga: no-op stub (jobs run inline).
- `job_main` registers `&lib->job_quit`; `sel_job_stop` already sets it. The cancelled push fails like an unreachable server, so `sel_rating_job_run` leaves the row pending.

Test: ctest `library-close-cancel` (tests/net/library_close_cancel.cmake, silent_server.py accepts and never answers). Measures wall time of a script that RATEs against the silent server and exits, minus an idle baseline; asserts <= 500 ms; then reruns with the fixture up and asserts rating_pushes=1 rating_pending=0.

Results (host): with fix passes (close cost ~0 ms, baseline noise); with the `sel_net_set_cancel` call in job_main disabled: FAILS, 2679 ms. Also passing: rate-devilbox, vote-devilbox, prefetch, prefetch-preempt, net-guard, http-client, loudness-fetch. Amiga build compiles.

Open: the worst-case wait is now one 50 ms slice plus TLS handshake CPU; a download stuck inside mbedtls code with no poll is not covered (none observed).
