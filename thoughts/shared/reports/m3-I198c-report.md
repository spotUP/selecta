# I198c: one-letter search words inside the 512 KiB heap

## Measurement before (bigidx.db, 20000 rows, vamos --machine a1200, heap 524288)
- `a`: `ERROR out_of_memory` (rc 1). `s` is the same path (no prefix index for one letter).
- Control searches at the same HEAD: som 473408, artist:artist 336640, song 469824, long 473920 peak.

## Design: option (b), a one-letter word leaves the MATCH and is checked by sel_fts_prefix
- A bare one-letter word is no MATCH term. It joins `lib->fts_long`, the word list that `sel_fts_prefix()` (I198b)
  checks against each row's unicode61 tokens, so semantics stay "token prefix", identical to what FTS gave.
- Only one-letter words in the query: the arm becomes `SELECT key FROM songs WHERE 1 AND sel_fts_prefix(...)`
  (and the same over `idx.songs`), run through the existing paged per-schema statement (`ORDER BY key LIMIT`),
  so memory is O(page). Mixed queries (`s song`) keep the MATCH terms of the other words and check `s` per row.
- Why not (a) prefix='1 2 3': one letter prefix indexes hold a doclist per letter spanning most rows, so the
  index (shipped prebuilt, exported for Modland's 516k rows) grows a lot, every existing index DB must be rebuilt
  (no migration path for prebuilt indexes), and it fixes one length only. Not measured; rejected on design.
- Why not (c): `a`/`s` are legal searches; an error code would be a functional loss when (b) works.
- Cost: time. A scan checks every row; on the 20000-row index `a` took 2460 ms, `s` 1980 ms under vamos
  (som 3760 ms). On the 516k Modland index a one-letter search will take tens of seconds (same order as the
  compiled-SQL full scans measured in I198a, ~20 s); unmeasured here.

## After
| query | result | db_heap_peak | query_ms |
|---|---|---|---|
| a | hits, no ERROR | 337376 | 2460 |
| s | hits, no ERROR | 337888 | 1980 |
Other rows unchanged (som 473408, song 469824, long 473920).

## Revert check
The test with `a`/`s` added was run first against the old search.c: `RED[T96]: multi-hit search 'a' ... failed
(rc=1) ... ERROR out_of_memory`. After the change it passes (amiga-library-vamos).

## Tests
- tests/m2/library_vamos.cmake: foreach adds "a" "s" (hits, no `ERROR <code>` at line start, peak <= 524288).
- tests/m3/search_prefix.golden: rows `p`, `s`, `z` (fts_queries 0, token-prefix results, empty result);
  library-search-long-prefix expects library_searches=10.
- `ctest -R library`: 11/11 pass. `commands --unimplemented | wc -l` = 8. Both builds: 0 warnings.

## Open
- `selecta-cli library search` rejects a multi-word query (`ERROR argument_count`), so the mixed `s song` path is
  not covered by a host golden; it is built by the same code and untested end to end.
- Time on the full Modland index for one-letter words is unmeasured.
