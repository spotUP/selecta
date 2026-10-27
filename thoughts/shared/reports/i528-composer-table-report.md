# I528 composer table report

Decision (Johan): a maintained composer table so the composer list is a plain read.

## Design (schema v7, src/library/db/schema.c)
- composers(canon PK, display, songs) WITHOUT ROWID + index composers_by_name(display, canon, songs).
- composer_spellings(canon, display, n): rows per spelling, so MIN(display) after a delete is a seek among few rows.
- composer_unknown(n): songs without any song_composers row (the Unknown composer row, last).
- Six triggers: song_composers insert/delete/update(canon, display); songs insert/delete/update(key).
- op_composer_list (songdb.c) streams SELECT from composers ORDER BY display, canon, then the Unknown row, through op->row
  (compatible with the buffered replay in sel_genre_call). user.c "Your composers" reads composers too.
- songdb apply uses INSERT ... ON CONFLICT DO UPDATE instead of INSERT OR REPLACE (REPLACE does not fire delete triggers).
- library_clear empties the composer tables first so the per-row triggers touch nothing.

## Write paths
song_composers: songdb.c tag_composer and op_apply; folders.c song_remove and library_clear. songs deletes: folders.c song_remove;
scan.c:121 and :204 delete songs without deleting song_composers rows (pre-existing orphans; the old GROUP BY counted them, triggers
keep that identical). scan.c:216 renames songs.key (songs_composers_au). Nothing updates song_composers.song_key.

## Migration constraint found
SQLITE_TEMP_STORE=3 and a fixed memsys5 heap (32 MB Mac, 8 MB a060): CREATE INDEX on song_composers(canon, display) and a
GROUP BY canon, display fill both fail with "out of memory" on 516k rows. The fill is a streamed upsert per row
(INSERT ... SELECT ... WHERE 1 ON CONFLICT DO UPDATE), which fits.

## Measurements (live copy lib-copy.db, 516,675 songs, 21,818 composers)
- Migration (open+migrate): 2.65 s host tag; 2.65 s a060 tag (reset to v6 and re-migrated).
- List: 6.4 ms host, 6.5 ms a060, 21,818 rows. Target 100 ms met.

## Tests
- ctest library-scan-composers (compares list with old GROUP BY after inserts, composer change, deletes), composer-unit,
  library-scan-default-db, dui-library-unknown-composer, similar-duplicates, devilbox-genre, radio, schema, library-sync, user-data: 11 of 11 pass.
- Revert-check: with the song_composers insert trigger body emptied, library-scan-composers fails (FAIL tag names the composer, ...).
  Restored: passes.
