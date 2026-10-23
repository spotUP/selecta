# M3 Phase 2 report: query parser and SQL compiler (I147, I148)

Plan: `thoughts/shared/plans/2026-10-25-m3-library.md` Phase 2; design doc section 9.2. I149 to I151 (LIBRARY_SEARCH, PLAY_QUERY, QUEUE_ADD_QUERY, VIEW_SAVE) not started.

## Rows
- I147: `src/library/query.c` / `query.h`: one recursive-descent parser (OR, implicit/explicit AND, NOT, `-term`, parentheses, quoted strings, bare words, `key:v`, `key:=v`, `key:/re/`, `>n`, `<n`, `=n`, `n..m`). Errors: `unknown_key`, `view_cycle`, `unknown_view`, `syntax`, `bad_value`, `too_complex`.
- I148: SQL emitted while parsing against schema v1, every value a bound `?` parameter (fixed-size struct, no allocation). Durations (s/m/h, default s), sizes (b/k/m/g), relative time (s/m/h/d/w/y; `<7d` = newer than), `lastplayed:never`, `in:` (playlist or folder name), `view:` (nested, cycle detected through a name stack, depth 8), `liked/wrong/skip:yes|no`, `plays`, `rating`.
- Keys: format artist album genre source name path md5 year rating plays length size played lastplayed added liked wrong skip in view.

## Tests
- T63 `query-compile` red commit "Add T63 query-compile red test (I147, I148)": `100 - query-compile (Failed)`. Green after implementation.
- Entry point: `selecta-cli library query --explain [--now N] [--view name=query]... [--] <q>` prints `sql=`, `params=`, `query_terms=` (sentinel, must be >= 1). Script `tests/m3/query_compile.sh` over `tests/m3/query.golden` (29 rows incl. 5 error rows) prints `query_compiles=<n> rows=<n>` and fails unless equal.
- Extra manual check (not in ctest): every golden SQL prepared and executed against the schema v1 text with Python sqlite3: 0 failures.
- `selecta-cli commands --unimplemented | wc -l` = 21. build/host and build/amiga both build.

## Deviations and open questions
- Plan defines no golden format; chosen: TSV `query, sql, params`, header `#now`, `#view`.
- `key:/re/` compiles to `REGEXP ?`; SQLite has no built-in REGEXP, so the worker must register a regexp function before I149 runs such a query (not done; needs a small matcher usable on 68k).
- `in:<name>` matches a playlist name or a folder name one level deep, not nested subfolders. Veto if wrong.
- `played` and `lastplayed` are aliases (last non-skipped play). `plays` counts non-skipped history rows.
- `-term` / `NOT` on nullable numeric columns excludes NULL rows (SQL three-valued logic); text columns use COALESCE so they are included.
- Compile-only CLI: running a query against a DB waits for LIBRARY_SEARCH (I149).
