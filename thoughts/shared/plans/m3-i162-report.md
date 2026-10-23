# I162 report: hvsc source (T73 source-hvsc)

## Changed
- `src/library/sources/hvsc.c`: `hvsc` source descriptor (parse, url, path_in); registered in `sel_source_register_builtin`. Song key `hvsc:<path>`, path relative to the C64Music root. Source tag `SEL_SRC_HOSTED_BY` = "modland".
- `src/library/sources/hvsc_sync.{c,h}`: second source of the existing `LIBRARY_SYNC` command (no second mechanism). Phases: `hvsc_listing.txt` via `SCAN_BATCH`, `Songlengths.md5`, `STIL.txt`.
  - Time parse `m:ss` and `m:ss.mmm` (trailing flags such as `(G)` ignored), integer ms, no float.
  - Songlengths set `songs.duration_ms` (first subtune), md5, stickers `subtunes` and `lengths_ms`.
  - STIL sets stickers `stil.<field>` and `stil.<n>.<field>` (comment, title, artist, author, name); continuation lines joined.
- New DB request `SEL_LIB_REQ_ANNOTATE = 8` (`db.h`, `db.c`, `scan.c`): one transaction, updates existing song rows and stickers only. No schema change (user_version stays 1; stickers table reused).
- Engine `cmd_LIBRARY_SYNC` dispatches "modland" or "hvsc"; `sel_sync_info` gained `songlengths`, `stil_entries`, `subtunes`.
- CLI `library sync hvsc <dir>` prints `listing_rows= skipped_rows= songlengths= subtunes= stil_entries= db_songs=`; `--dump` prints `hvsc_row=` and `hvsc_sticker=` lines.
- Fixtures `tests/m3/hvsc/`, test `tests/m3/source_hvsc.sh`, CMake test `source-hvsc` at the end of `tests/CMakeLists.txt`.

## Verified
- Host Debug and Amiga 68020 builds link. `ctest -R 'source|hvsc|modland|library|sync'`: 12 of 12 pass, including `source-hvsc` and `source-modland`.
- `selecta-cli commands --unimplemented | wc -l` = 17.
- T73 counts rows, subtunes, songlengths and STIL entries from the fixtures, and checks Monty on the Run to the ms (225000,30500,62125), a STIL title, a STIL continuation, and the url of two rows.

## Found
- UNVERIFIED ASSUMPTION: the Modland path layout for HVSC is `pub/modules/HVSC/<path>`. The design doc gives no concrete layout. Change `SEL_HVSC_HOST_DIR` in `hvsc_sync.h` if the real host differs.
- `SEL_LIB_REQ_ANNOTATE = 8` may collide with a request code another agent adds concurrently; renumber on merge.
- Network fetch of Songlengths.md5 and STIL.txt is not wired (fixture directory model, same as I161).
- `chomp` and batch flush helpers duplicate `modland_sync.c`; candidates for a shared helper.
- STIL notes for a path with no catalogue row are counted in `stil_entries` but not stored.
- Cosmetic: the comment at `source.h` line 72 still names only `local` and `modland`.
