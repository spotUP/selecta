# I161 report: modland source sync (T72)

## Changed

- `LIBRARY_SYNC <source> <dir> [now]` engine command (commands.def, engine.c `cmd_LIBRARY_SYNC`, `sel_engine_sync_info`).
- `src/library/sources/modland_sync.{c,h}`: reads `<dir>/allmods_md5.txt` (`<md5><ws><path>` or `<size>\t<path>`; `pub/modules/` is
  prepended when missing), parses rows with the modland source `parse`, writes them in batches of 32 through
  `SEL_LIB_REQ_SCAN_BATCH`, then applies every `<dir>/changelog_YYYY-MM-DD.txt` in name order.
- New library request `SEL_LIB_REQ_REMOVE` (db.h, db.c, scan.c) deletes files and songs rows for deleted and moved paths.
- `selecta-cli library sync [--db p] [--now epoch] [--dump] modland <dir>` prints `listing_rows= new_this_week= ...  db_songs=`;
  `--dump` prints `row=path|format|artist|md5` from the database file (host build only).
- Fixtures: `tests/m3/modland_listing/allmods_md5.txt` (100 rows: 40 real paths + 60 synthetic, md5 of the path string),
  `tests/m3/modland_changelogs/` (5 files), `tests/m3/modland_parse.golden`. Test `source-modland` (T72) `tests/m3/source_modland.sh`.
- Fixed clock 1792756800 (2026-10-23 12:00 UTC); week window = changelog dates 2026-10-17 .. 2026-10-23 (today and the 6 days before).
  Expected values (added, deleted, in-week added) are counted from the fixture files inside the script.

## Verified

- Host and Amiga (`cmake/amiga-68020.cmake`) `selecta-cli` build clean; no float used.
- `ctest -R 'source|modland|library|sync|commands|ratchet|protocol|engine'`: 18 of 18 pass, includes `source-modland`.
- `selecta-cli commands --unimplemented | wc -l` = 17 before and after (LIBRARY_SYNC was never in the list); ratchet file unchanged.

## Found / open

- Real `allmods_md5.zip` returns 404 upstream (phase 0 report); the md5 line format is therefore an assumption. The zip is not read, only the plain text listing.
- The `added:` / `new:` changelog keyword is inferred; the real changelog fixture only shows moved/to/updated/deleted.
- `updated:` is only counted. A `moved:` row loses its md5 (the listing is the only md5 source).
- Network fetch of listing and changelogs is not wired: `sel_source.listing` stays NULL; sync reads files in a directory.
- `new_this_week` lives in the engine counters only, not in a DB table (T91 may need persistence).
