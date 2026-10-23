# M3 I183 report: similar and also-by queries

## Done
- `src/library/db/similar.c`: worker ops `SEL_SIMILAR_NEXT` / `SEL_ALSOBY_NEXT` (added to the genre op enum in `db.h`, dispatched from `genre.c`, paged by row offset) and the caller halves `sel_library_similar` and `sel_library_also_by`.
- Similar: every other song ordered by genre match with the seed (case-insensitive; empty genre never matches), then rating (`ratings` table, unrated = 0), then library order; capped at 100 rows.
- Also-by: other songs sharing a canonical composer with the seed (`song_composers`, filled by the I182 songdb step); same row callback as the composer page.
- CLI: `selecta-cli library similar --db <path> <key>` prints `row=<key>|<name>|<genre>|<rating>|<match>` and `similar_rows=N`; `library also-by --db <path> <key>` prints `row=<key>|<name>` and `also_by_rows=N`.
- Test `similar-duplicates` (`tests/net/similar.cmake`, golden `tests/m3/similar.golden`): on the devilbox fixture, also-by of ambient.mod = techno + nogenre (2, not itself, not down.mod), also-by of down.mod = 0; similar order is nogenre (genre match, rating 2) before techno (rating 5) before down (rating 4). Passes; `devilbox-genre` and `library-db-open` still pass. The duplicates part (I184) is absent, not red.
- Amiga: `selecta_library` target builds for 68020.

## Open
- No rating writer exists yet (RATE is I173), so the test inserts ratings with the `sqlite3` shell found at configure time (`SEL_SQLITE3`); on a machine without it the test fails. Switch to the RATE verb once I173 lands.
- The test sets genres by `userdata --tag`; the fixture has no two songs with the same genre.
- Similar lists all other songs (genre match first), not only genre matches; the radio steering rule (I186) may want a filter.
- Not run: full ctest, lint target.
