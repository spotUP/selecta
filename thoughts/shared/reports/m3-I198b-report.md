# I198b: long FTS prefixes inside the 512 KiB heap

Cause (from I198a): song_fts has prefix='2 3'. A prefix of 4+ letters has no prefix index, so FTS5
(fts5SetupPrefixIter) merges every matching doclist into one buffer; with 20000 hits that is a 64 KiB+ allocation
that fails on the 512 KiB heap (SQLITE_NOMEM).

Fix (src/library/db/search.c):
- A bare word of 4+ characters (ASCII alnum, no `_`) becomes the 3-letter MATCH term `"son"*`, served by the
  prefix=3 index as one streamed doclist.
- SQL function `sel_fts_prefix(words, name, artist, format, path)` (registered with regexp, same connection, so
  the attached idx arm uses it too) returns 1 when every long word is a prefix of some token of the row. It
  tokenizes with FTS5's own unicode61 tokenizer (fts5_api from `SELECT fts5(?1)` pointer binding, xFindTokenizer),
  query words and columns alike. The query words are tokenized once per statement (sqlite3_get_auxdata); fixed
  buffers (8 words x 48 bytes); no allocation per row.
- Both FTS arms get `AND sel_fts_prefix(:pfx, ...)` only when a long word exists; `:pfx` is bound in bind_query.
  The I198a paged per-schema sorter is unchanged.

Tests:
- tests/m2/library_vamos.cmake: `song` and `long` over the 20000-row bigidx.db at --machine a1200 require hits,
  no `ERROR <code>` and db_heap_peak <= 524288. Revert check: with the old search.c it fails (`song` rc=1),
  with the fix it passes. The check now matches `ERROR <code>` at a line start because vamos logs
  "path: ERROR: error removing temp dir" when another vamos shares ~/.vamos.
- tests/m3/search_prefix.{tsv,golden} + ctest library-search-long-prefix: `song` finds Songbird and Songwriter
  (diacritic folded) but not Prosong; `prosong`, `songb`, `songw`, `sona`, `songx` (no hit), `son`.

Measured under vamos, --machine a1200, 20000-row index (db_heap_peak bytes / query_ms):
som 473408 / 3740; artist:artist 336640 / 1720; song 469824 / 6060; long 473920 / 5800.
`purple` over the full Modland index: not measured (fixture not in this worktree).

Note: a worktree build needs third_party/nuade and third_party/libopenmpt populated (submodules were empty; the
worktree used symlinks to the main checkout, not committed).
