# I198d: one-character non-ASCII words and punctuation-only words

## Index used
No 516k Modland index exists locally (only a 100-row listing under tests/m3/modland_listing). Built the largest the
tooling makes: a synthetic listing of 516000 rows (`Protracker/Artist N/some long song name number N.mod`, every
5000th name "café"), `library sync` + `export-index` -> 313 MB bigidx.db, index_songs=516000 (not the real
Modland names). Run under vamos `-C 68020 -m 8192 --machine a1200`, from build/host as cwd.

## Premise check (HEAD before the change)
`is_bare_words()` rejected every non-ASCII byte and every punctuation byte, so `é` and `-` never reached the FTS
MATCH: they were compiled by query.c to `LIKE '%x%'` substring scans over the index. Neither OOMs.

## Before / after (516000 rows, vamos)
| query | before: db_heap_peak / query_ms | after |
|---|---|---|
| a | 337376 / 97100 (wall 98 s) | 337376 / 108640 (wall 113 s), same path, noise |
| é | 336352 / 96260 (LIKE full scan) | 262912 / 0 |
| - | 336352 / 100440 (LIKE full scan, matches "selecta-test") | 262624 / 0, no rows |
One-letter ASCII words stay a ~100 s full per-row scan at 516k rows under vamos (I198c's estimate, now measured).
Caveat: under vamos the argument `é` reaches the 68k CLI as one Latin-1 byte (0xE9), not UTF-8 (the LIKE scan found 0
rows before although "café" rows exist; the host finds them). Invalid UTF-8 tokenizes to nothing, so after the change
it is "no rows" at once. The per-row UTF-8 one-character path is therefore proven on the host golden, not timed on 68k.

## Change (src/library/db/search.c)
- `is_bare_words()` accepts UTF-8 bytes, so non-ASCII words take the FTS path instead of substring LIKE.
- `fts_fold_word()` tokenizes a non-ASCII word with the same unicode61 tokenizer as the index and sel_fts_prefix()
  (`fts_open()` extracted from the registration, nothing duplicated). Length is counted in characters of the folded
  token (é folds to e): 1 character -> `fts_long` per-row check, no MATCH term; over 3 -> MATCH on the first 3
  characters plus per-row check; no token -> word dropped.
- A query whose words all drop returns no rows (an empty MATCH would be a syntax error). `punct_only()`: a query of
  ASCII punctuation only (no quote, bracket, `:`, `/`) returns no rows (it tokenizes to nothing).
- Behaviour change: a lone `-` no longer finds hyphenated names by substring; non-ASCII bare words are now token
  prefixes (accent-folded, case-folded) like ASCII ones, not case-sensitive substrings.

## Tests
- tests/m3/search_prefix.{tsv,golden}: new row k-eclair.mod; goldens `é`, `É`, `écl`, `café`, em dash, `-`;
  library-search-long-prefix expects library_searches=16.
- tests/m2/library_vamos.cmake: `é` and `-` over bigidx.db: no ERROR, db_heap_peak <= 524288 (built, not run here:
  amiga-library-vamos was not run, 68k half measured by hand above).
- Revert check: old search.c -> library-search-long-prefix FAILS (`écl` fts 0, `café` fts 0, `-` returns 5 rows);
  new search.c passes. `ctest -R 'library|query|search'`: 13/13. Both builds: 0 warnings outside third_party.

## Open
- amiga-library-vamos not run (4 searches over a 20000-row index; the machine was kept to one heavy job).
- Real UTF-8 argument under vamos untested (argv arrives as Latin-1); host path covers it.
- One-letter ASCII search on 516k rows: ~100 s under vamos; a faster path needs a one-letter prefix index (rebuild
  of every shipped index) or a per-row cap, a product decision.

## I198e: one-letter scan capped (timing)
Same synthetic 516000-row index as above (not the real Modland names), vamos `-C 68020 -m 8192 --machine a1200`,
`library search --db work:user.db --index work:idx.db -- a`. After the cap: src_count=256, truncated=1,
db_heap_peak=328672, query_ms=500 (before: ~100000 ms). The first run on a fresh user.db returned
`ERROR out_of_memory` once and did not reproduce on two reruns (a, s); unexplained, watch for it.
