# M3 I195a: cancel an in-flight URL fetch on STOP/OPEN

Root cause: read_song runs on the pump thread inside sel_stream_read -> sel_http_request, whose waits honour a
thread-local cancel flag (sel_net_set_cancel, src/core/posix/net.c, already used by the library job thread). The pump
never set one, so a STOP/OPEN pushed from another thread (the UI) waited out the HTTP timeouts (3 s first byte).

Fix (reuses the existing mechanism, no new one): engine field `net_cancel`; sel_engine_send sets it before pushing a
STOP or OPEN; sel_engine_pump registers it with sel_net_set_cancel and clears it when it pops a STOP/OPEN and the queue
is then empty (the served command's own fetch must run). No allocation, no float in the engine. Amiga: sel_net_set_cancel
is a no-op there (jobs run inline), build/amiga links.

Sentinel: `net_cancels` in sel_engine_stats, printed by script directive `@cancel_stats` (CANCEL net_cancels=N); counted in
read_song when a URL fetch fails with the flag set. Test hook `@stop_after <ms>` sends STOP from a second thread.

Test: tests/net/stream_url.cmake (stream-url), OPEN of http://hang.test (silent_server.py: accepts, never answers) with
STOP after 300 ms: timed_ms <= 1000 and net_cancels=1.
Fail-before (send hook reverted): "I195a OPEN of a hanging URL then STOP: 3001 ms (bound 1000)", test fails.
After: passes. Also green: net-guard, net-fixture*, prefetch*, lyrics, library-close-cancel, stream-url; unimplemented = 8.

Known limit: a cancel set between the pump's empty-queue check and a concurrent send can be lost (the fetch then ends at
its HTTP timeout, nothing worse).
