# I527 follow-up: LIBRARY_SYNC off the shell thread

Bug: Sync (banner or Sources page) froze the Mac window; cmd_LIBRARY_SYNC ran src->sync inline on the shell thread (sample: sel_engine_pump > cmd_LIBRARY_SYNC > sel_zxart_net_sync > sel_net_recv).

Fix (src/engine/engine.c): LIBRARY_SYNC is a library task like LIBRARY_SCAN (design: the shell loop never waits on library work, I336).
- sync_start / sync_task (job thread runs src->sync, then sel_sync_ctx_release) / sync_collect (pump: art_generation, record_sync_stats, sync_time_set, persist_new_this_week, sync_failed or emit_library_path, unchanged).
- Scan and sync share the one task slot: each command starts only when neither runs, else queues (scan_next, new sync_next of 4; full sync queue answers ERROR sync_busy). Both collects start the next queued scan, then sync.
- sel_engine_scan_busy / sel_engine_scan_wait now cover a running sync. selecta-cli `library sync` now calls sel_engine_scan_wait after the pump (it relied on the synchronous result).
- Deadlock check: sync's library requests (catalogue batch, sync_services.c) go through sel_library_request, serialised by lib->call_lock and answered by the worker thread, not the job thread; the scan's writes use the same path. No deadlock.
- Not done: no progress reporting for sync; destroy waits (sel_library_task_cancel) for a running sync, which does not poll cancel, so quitting mid-network-sync can wait for the sync's I/O timeout.

Tests: new library-sync-async (tests/engine/library_sync_async_test.c): blocking source, 50 pumps return while the sync runs, second sync and a scan queue, calls==2 after release. Existing library-sync-dispatch and src-plugin-reach now pump via sel_engine_scan_wait before asserting. 56 affected ctests (source, library-sync, src-plugin, scan, sources, unit) pass, including the *-net sync tests.
Revert-check: engine.c restored to HEAD, library-sync-async fails (FAIL line 107: g_done == 0, the inline sync had already finished when the first pump returned); fixed engine.c restored and passes.
Unverified: desktop UI (SDL) build, dui-sync-banner; live Mac check by Johan.
