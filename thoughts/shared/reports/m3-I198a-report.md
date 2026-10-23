# M3 I198a: multi-hit search on the full Modland index at heap 524288

Status: partly resolved. The I198 failures do not reproduce at HEAD; a different multi-hit failure is reproduced
and its cause measured; the fix is not made yet.

## Exact error
- `rc=7 ext=7 msg="out of memory"` (SQLITE_NOMEM) from the search statement, surfaced with a temporary
  `sqlite3_errcode/errmsg` print in the search error path (removed again).
- Search errors now go through `set_sql_error` (src/library/db/search.c): SQLITE_NOMEM reports
  `ERROR out_of_memory`, every other failure stays `bad_value`. No NOMEM code existed in the vocabulary.

## The searches at HEAD (full Modland index, 516,189 rows, vamos 68020, heap 524288)
| run | result | db_heap_peak | query_ms |
|---|---|---|---|
| bekiffte | src_count=1 | 455616 | 20 |
| jester (FTS) | src_count=163 | 469440 | 360 |
| purple (FTS) | src_count=319 | 493440 | 700 |
| artist:jester | src_count=165 | 437920 | 24420 |
| format:xm | src_count=11 | 437920 | 24440 |
| year:1995..1997 | src_count=0 (the Modland listing carries no year) | 437920 | 19300 |
| stats after a search | index_songs=516189 | n/a | n/a |

A rebuilt amiga binary and the main checkout's binary agree. The earlier bad_value and index_songs=0 therefore
came from a binary or index older than the idx cache_size cap, not from the current design. purple peaks at 94%
of the heap: little margin.

## Remaining failure (reproduced)
A search matching very many rows fails: the FTS word `song` over a 20000-row index (every row a hit) gives
`rc=7 out of memory`. Cause: search.c wraps both files in `SELECT key FROM (... UNION ALL ...) ORDER BY key`;
the sorter holds every hit, and SQLITE_TEMP_STORE=3 (third_party/sqlite/CMakeLists.txt) puts any spill into the
same 512 KB heap. Memory grows with the number of hits, so a 100k-hit word cannot work whatever the cache size.
Evidence so far: the NOMEM code and the query shape; sqlite3_status numbers were not taken (call budget).

## Fix direction (not made)
Bound the sorter: ORDER BY key LIMIT <rows that fit the result buffer> (a top-N sorter is O(limit)); keep the
`result_truncated` count (engine.c:2902) with a second count statement only when the limit was reached. Needs the
row bound (out_cap over the smallest key length, or the queue capacity) and a check that the top-N sorter fits.

## Regression test
Prepared in the worktree but not committed because it is red: tests/m2/library_vamos.cmake, after the 20000-row
stats check, runs `song` and `artist:artist` on bigidx.db and requires hits, no ERROR and db_heap_peak <= 524288.
It fails now with `ERROR out_of_memory` (verified) and should pass once the sorter is bounded.
