# M3 I167 report: LRU trim of the download cache

Rows: I167. Test: T76 `cache-budget` (tests/m3/cache_budget.sh), green.

- Budget: `sel_cache_budget(machine)` in src/library/cache.c, fast RAM tiers as in `sel_queue_default_sizes`: <8 MB 8 MiB, <64 MB 64 MiB, else 512 MiB. The disk tag has no size yet, so RAM alone decides. `sel_library_open` stores it in `lib->cache_budget`.
- Last access: file mtime. `sel_cache_fetch` touches on a hit and a stored file has the write time; `sel_file_stat` and `sel_file_touch` added to src/library/dir.h (POSIX in core/macos/dir.c, Amiga in core/amiga/dir.c). No library-DB or index column existed.
- Trim: `sel_cache_trim` walks `<cache>/files`, evicts oldest first until the total fits; exempt (kept, liked) and the just-stored file are never removed. `sel_library_fetch` (the existing LIBRARY_FETCH path) calls it after any fetch that stored a file; keep test is a query on the `kept` and `liked` tables.
- Entry point and sentinel: `selecta-cli library trim [--budget n] [--machine m] [--mark kept|liked:<key>]` prints `budget= trim_runs= bytes_before= bytes_after= trimmed= kept_exempt=`; `trim_runs` is `stats.trim_runs`.
- T76 numbers: 6 x 1000 bytes, budget 3500, 1 kept, 1 liked: trimmed=3, kept_exempt=2, bytes_after=3000; budget 100: only the unmarked file goes; tier budgets a500/a1200/a1200-fpu 8388608, a060 536870912.
- Added `sel_library_mark` (insert into kept or liked) for seeding; no product API for kept/liked existed.

Open: companions of a kept song are not exempt (only keys in kept or liked); trim not yet exercised through a networked LIBRARY_FETCH test; the library DB is used from the caller thread for the trim (worker idle). Build: host and Amiga build clean. `commands --unimplemented` stays 17.
