# I517 phase 1 report: source SDK descriptor, net and catalogue services, sync dispatch

Commit: 0501a7d (main, not pushed). Plan: thoughts/shared/plans/2026-10-26-source-plugins.md (D1, D2, D5, Phases item 1).

## Done
- include/selecta/source.h (descriptor plus `sync`, `flags`, `sync_default`), net.h, catalogue.h.
- `net` and `catalogue` accessors appended to `sel_host`; SEL_API_VERSION 2; ABI goldens (arm64, x86_64, m68k) and apps/selecta-cli/abi.c updated.
- Engine implementations: src/library/sources/sync_services.c and sync_host.h.
- Generic sync stats as SEL_SYNC_* tags (0xD000-0xD019) in `sel_sync_info.stats`; legacy per-source fields kept and filled from the tags.
- `cmd_LIBRARY_SYNC` dispatches through `src->sync`; the if-chain is gone. `local` has no sync, so it answers `unknown_source`.
- `sel_source_is_remote` reads SEL_SOURCE_REMOTE; `persist_new_this_week` keys on SEL_SOURCE_NEW_THIS_WEEK.
- Seven built-in sources got `<id>_sync_host` wrappers plus flags; sources were not moved to plugins/.

## Tests (build/host unless noted)
- New `library-sync-dispatch` (tests/engine/library_sync_dispatch_test.c): registers a counting test source, drives LIBRARY_SYNC through the engine command channel. Asserts call count, arg, clock (also via `catalogue->now`), host net and catalogue present, begin/song/commit succeed, stats tags and legacy fields, `sync_failed`, `unknown_source`.
- Revert check: replacing the `src->sync(...)` call in cmd_LIBRARY_SYNC with `ok = 0` makes the test fail ("FAIL line 123: g_calls == 1"); restored, it passes.
- 63 of 63 selected tests green: all source/sync/net tests, sdk-isolation, sdk-isolation-fixture, sel_incheck_*, sources, sources-off, abi-host-layout, abi-voices-layout, dui-library-sources.
- build/sdl: `ctest -L dui -LE dui-red` 56 of 56 green. Lint and format targets clean.

## Deviations and open items
- Public type `sel_playlist` in source.h renamed `sel_source_playlist` (clashed with src/library/playlist/playlist.h). Only the `read_list` field uses it.
- SEL_SYNC_STATS_MAX lives in tags.h (include cycle avoidance).
- Catalogue table lacks album, art, list and annotate calls (phase 4).
- Built-in sources still use `scfg` through the sync context; they move to the services in phases 3-4.
- m68k ABI golden numbers (net 44, catalogue 48, size 52) pass abi-host-layout but no Amiga build was run.
- Amiga net table pending I199.
