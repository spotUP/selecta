# I528 composer list froze the UI after Sync

## Cause
sel_library_composer_list fetched one composer per library request; op_composer_list ran the full GROUP BY over
song_composers with LIMIT 1 OFFSET n each time. Live DB (516,675 songs, 21,817 composers): about 3.05 s per row
(measured with sqlite3 on a copy, OFFSET 10000), so about 18.5 hours extrapolated (21,818 rows), quadratic in effect.
COMPOSERS_SHOW runs on the UI thread inside sel_engine_pump and waits on the library worker in sel_genre_call.

## Fix (library layer)
sel_genre_op gained a row sink (row, row_ctx). The list ops now run ONE query and stream every row through the sink
inside a single library request; the reply id carries the row count. Same treatment for the other per-row OFFSET
lists: SEL_SIMILAR_NEXT (similar.c, was LIMIT 1 OFFSET per row, up to 100 requests, each sorting the library),
SEL_ALSOBY_NEXT (similar.c), SEL_DUP_NEXT (duplicates.c). user.c:411 has no OFFSET (single LIMIT ?1 query), unchanged.
Public API (sel_library_composer_list / similar / also_by / duplicates) keeps order, counts and callbacks.
Note: the sink runs on the library worker while the caller is blocked in the request.

## Test
tests/library_scan_composers_test.c (ctest library-scan-composers): seeds 300 composers, asserts the list makes exactly
one library request (lib->served delta, the existing worker counter), returns 303 rows (300 + 3 from earlier cases),
in display order. With the src changes reverted the test fails: "FAIL composer list is one library request".
Restored: passes. Also green: composer-unit, library-scan-default-db, library-task-thread, library-sync-dispatch,
dui-library-unknown-composer, devilbox-genre, similar-duplicates, meta-utf8-latin1-composer-name-unit.

## Timings (live DB copy, read-only source, Mac-sized heap)
- Before: about 3.05 s per composer row, 21,818 rows = about 18.5 h (extrapolated, not run to the end).
- After: one request, 21,818 rows, about 1.9 s (driver ran 2 requests in 3.9 s).
- Finding: with the fake "a060" machine tag (small SQLite heap) the same query fails with SQLITE_NOMEM on the live DB
  (COUNT(DISTINCT) over 516k rows). Old code had the same query, so Amiga-class heaps cannot list composers of a library
  this size; not touched here.

## Remaining UI-thread blocking
COMPOSERS_SHOW still waits about 1.9 s synchronously on the UI thread for the single GROUP BY (> 100 ms). Similar,
also-by and duplicates use the same synchronous sel_genre_call pattern and were not measured on the live DB; the
sel_library_duplicates path additionally hashes local songs first. Not measured through the engine during a running Sync.
The remaining cost is the aggregate query itself (a cached or incrementally maintained composer table would remove it).
