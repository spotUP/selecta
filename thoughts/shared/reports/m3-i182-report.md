# M3 I182 report: songdb lookup, composer alias merge, composer page

## Done
- `src/library/db/songdb.c`: songdb lookup `GET <devilbox>/api/songdb/lookup?hash=<12 hex UADE key>`, cached in `songdb_cache` (NULL authors = not in songdb). A failed request (503, 404, transport) caches nothing and is not an error. Runs inside `sel_library_genre_sync` (the UADE key is already computed there). Worker ops SEL_SONGDB_GET/PUT/APPLY and SEL_COMPOSER_NEXT; `sel_library_composer_page` pages by cursor.
- `src/library/composer.[ch]`: `sel_composer_canon` / `sel_composer_display`: drop "(handle)" groups, "Last, First" to "First Last", ASCII case and punctuation fold, alias table.
- Schema version 2: `songdb_cache`, `song_composers`, index on canon. `library_db_open.sh` expects 2.
- genre.c: `sel_json_string`, `sel_library_http_json`, `sel_genre_call` exported and reused (no second HTTP/JSON path).
- CLI: `library genre` prints `songdb_lookups= songdb_cache_hits= composers_set=`; new `library composer --db <path> <name>` prints `composer=`, `row=<key>|<name>`, `composer_rows=N`.
- T85 (`devilbox-genre`, `tests/net/devilbox_genre.cmake`): run 1 songdb_lookups=4, 4 `/api/songdb/` requests, composers_set=3; run 2 songdb_cache_hits=3, songdb_lookups=1; composer page for four spellings ("Jogeir Liljedahl", "virgill", "liljedahl, jogeir", "JOGEIR LILJEDAHL (Virgill)") lists the same 3 songs; "Dr Awesome" lists 1.

## Open question (needs a decision)
The songdb source format and the alias table are only partly defined.
- Source format: taken from the endpoint shape in `thoughts/shared/research/2026-10-25-m3-external-endpoints.md` line 15 (`{found,format,channels,authors[],publishers[],album,year,subsongs[]}`). Never verified against a live DEViLBOX server; fixtures follow that shape.
- Alias table: the design only says "author normalisation via DEViLBOX songdb". No alias data source is specified. `composer.c` has a built-in seed (virgill, jogge -> Jogeir Liljedahl; purple motion -> Jonne Valtonen; skaven, skaven252 -> Peter Hajba). Where the real alias data comes from (songdb field, shipped table, user edit) is undecided.
- Only ASCII is case-folded; non-ASCII names compare byte-exact.

## Leftovers
- No golden file `tests/m3/composer.golden`; the test compares the four spellings to each other and asserts counts.
- No separate unit test for `sel_composer_canon`; covered through the CLI test.
- Amiga: `selecta_library` target compiles; full Amiga link not run.
- Tests run: `-L m3` (34 tests, all pass) with vamos/rig excluded.
