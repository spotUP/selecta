---
status: done
row: I166a
---
# I166a report: LIBRARY_FETCH on cfg.library, fetch in the selecta shell

Plan: `2026-10-25-m3-library.md` Phase 5, tests T75 and T75a.

## What changed
- `src/library/db/fetch.c` (new): `sel_library_fetch`, `sel_library_cache_stats`, `sel_library_cache_report`. The `sel_library` handle owns the `sel_cache` (mirror sets, cache dir), created on the first fetch and freed in `sel_library_close`.
- Engine: `cmd_LIBRARY_FETCH` calls `sel_library_fetch(e->lib, ...)` with the companion rules from `sel_companion_rules(e->registry_count, e->registry_get)`, same shape as `cmd_LIBRARY_SCAN`. No `cfg.library` answers `ERROR no_library`.
- Removed: `sel_library_fetch_fn`, `library_fetch`, `library_fetch_ctx` (engine.h, engine.c), `apps/selecta-cli/cache_fetch.c` and its cli.h declarations. One path only.
- `selecta-cli script` and `selecta` print `cache_hits= fetches= companions= md5_mismatches= mirror_tries=` at the end through `sel_library_cache_report(lib)` (only when a fetch was served).
- Consequence: `script` without `--catalogue` has no library, so LIBRARY_FETCH answers `no_library` there. T75 (`tests/net/cache_fetch.cmake`) now passes `--catalogue tests/m3/catalogue.tsv`.
- T75a `cache-fetch-shell` (new block at the end of `tests/CMakeLists.txt`, `tests/net/cache_fetch_shell.cmake`): drives `selecta --exec 'LIBRARY_FETCH ...'` three times against the fixture mirrors; asserts `cache_hits=1 fetches=3 companions=1 md5_mismatches=0` and the files in the cache.
- `apps/selecta/CMakeLists.txt` untouched; `apps/selecta-cli/CMakeLists.txt` only drops `cache_fetch.c` from the source list. No Amiga start-up files touched.

## Results
- Host and Amiga builds compile. `ctest -R 'cache-fetch|mirrors-ranking'`: cache-fetch and cache-fetch-shell pass.
- `selecta-cli commands --unimplemented | wc -l` = 17 (unchanged).

## Open questions
| topic | default | alternative |
|-------|---------|-------------|
| `script` without `--catalogue` | LIBRARY_FETCH answers `no_library` | open a throwaway `:memory:` library in `script` when none is given |
| Amiga cache route (`sel_cache_init` `amiga_route`) | 0, as the CLI had it | derive from `sel_machine` when the Amiga shell gets network (Phase 12) |
| Cache allocation | `mem_alloc(SEL_MEM_FAST)` on first fetch | embed in `sel_library` (larger handle for every user) |
