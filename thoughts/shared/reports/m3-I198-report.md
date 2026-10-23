# M3 I198: full Modland index on the 68020/2 MB tier (D-M3-16)

Result: FAILS the tier as built. The index opens and counts after a code fix, but every multi-hit search errors
(`ERROR bad_value`) and `index_songs` reads 0 after a search. The fallback row (flat sorted table, section 15) is
warranted; not opened by this agent (todos.md is not mine to edit).

## Input (no allmods_md5 with md5 exists; real data used)
- `https://ftp.modland.com/allmods_md5.zip` does not exist (404, see 2026-10-25-m3-external-endpoints.md). The
  existing fetch path (src/library/sources/modland_net.c) downloads `allmods.zip` -> `allmods.txt` (`<size>\t<path>`,
  no md5). Real copy used: /Users/spot/Code/DEViLBOX/server/data/modland-cache/allmods.zip (5.76 MB, 2026-10-23),
  30,757,261 byte listing, 516,199 lines. The md5 column (32 bytes/row) is absent, so a build with md5 would be
  somewhat larger than the numbers below.

## Commands (desktop build/host Debug, amiga build/amiga; S = session scratchpad)
    unzip -p allmods.zip allmods.txt > $S/ml/allmods_md5.txt
    selecta-cli library sync --db $S/full.db modland $S/ml
        -> listing_rows=516189 skipped_rows=10 db_songs=516189 (full.db 334,168,064 bytes; 104 s wall)
    selecta-cli library export-index --db $S/full.db --out $S/w/idx.db modland
        -> index_rows=516189 index_bytes=271130624 (271 MB; 22 s)
    selecta-cli library stats --db $S/w/user.db --machine a1200      (creates the 2-row user DB)
    vamos -C 68020 -m 8192 -V work:$S/w -- build/amiga/apps/selecta-cli/selecta-cli library stats|search \
        --db work:user.db --index work:idx.db --machine a1200 [-- <query>]

## Real bugs found
1. `export-index` failed on the full source: `ERROR export_failed`, SQLite "out of memory" (desktop 32 MB heap): one
   transaction INSERT...SELECT for 516k rows with the FTS triggers. Fixed: windows of 20000 ids, one transaction each
   (src/library/db/index.c, EXPORT_WINDOW).
2. On the 68k, `library stats --index` over the 271 MB index: `rc=7 out of memory` from `SELECT count(*) FROM
   idx.songs` (the default 2 MB page cache of the ATTACHed file is carved from the 512 KB heap). Fixed:
   `PRAGMA idx.cache_size=-64` after ATTACH. Regression test: T96 (`amiga-library-vamos`) now builds a 20000-row
   Modland index and runs stats on it; verified red with the pragma removed, green with it.

## Measured under vamos (68020, --machine a1200, heap_bytes=524288; vamos clock, not hardware time)
| run | result | db_heap_peak | query_ms |
|---|---|---|---|
| stats (two STATS calls) | songs=516191 index_songs=516189 | 452640 | 2420 |
| search `bekiffte` (rare word, FTS, 1 hit) | src_count=1 fts_queries=1 | 457440 | 20 |
| search `jester` (FTS word, 186 hits in listing) | `ERROR bad_value` | n/a | n/a |
| search `purple` (FTS word, 319 hits) | `ERROR bad_value` | n/a | n/a |
| search `artist:jester` (compiled, field) | `ERROR bad_value` | n/a | n/a |
| search `format:xm` (compiled, field) | `ERROR bad_value` | n/a | n/a |
| search `year:1995..1997` (compiled) | `ERROR bad_value` | n/a | n/a |

- After the 1-hit search the CLI printed `index_songs=0`: the post-search `count(*) FROM idx.songs` fails; the
  peak 457440 is 87% of the heap, the same NOMEM class (unconfirmed).
- Unconfirmed: the cause of `bad_value` on multi-hit searches (no sqlite error printed; src/library/db/search.c sets
  bad_value on a failed prepare/step). Likely heap exhaustion with a real-size index; needs the sqlite errcode
  surfaced in the search error path.
- Heap was not changed. sqlite_text_bytes unchanged from I197 (1,045,456).

## Open
- Decide the fallback: flat sorted table (section 15) as its own row; the SQLite index costs 271 MB on disk and the
  512 KB heap is at 87% on the first query.
- Surface sqlite3_errcode in LIBRARY_SEARCH errors so bad_value on multi-hit searches is diagnosable.
- Real-hardware timing not measured (rig reserved).
