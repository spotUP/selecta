# M3 I197 / I143 (T96): prebuilt read-only index, library on the 68020/2 MB tier

Branch worktree-agent-a6425917ced1a2b52. Design: plan 2026-10-25-m3-library.md Phase 12, D-M3-16.

## Delivered
- `selecta-cli library export-index --db <path> --out <file> <source>` writes a database file (full schema, FTS built,
  optimised) with the songs of one source. Prints `index_rows= index_bytes= index_file=`.
- `sel_library_attach_index` ATTACHes it read-only (`file:...?mode=ro`, schema `idx`) next to the writable user DB.
  LIBRARY_SEARCH (FTS and compiled SQL), metadata lookup and STATS cover both files. User data stays in the main DB.
- `library search|stats --index <file> [--machine <name>]` print `index_songs= db_heap_peak= query_ms=`.
- Tests: `library-index-attach` (desktop, 13 checks: export rows, golden keys across both files, stats, index file
  byte-identical, missing index error) and `amiga-library-vamos` (T96, 68k under vamos, `--machine a1200`).

## Real Amiga bugs found and fixed (src/library/db/amiga_vfs.c)
No file database could be opened on the 68k at all (only `:memory:` worked):
1. SQLite unix VFS prefixes relative names with getcwd(); AmigaDOS names ("work:user.db") never start with "/" and
   libnix getcwd fails (vamos: GetCurrentDirName stub) -> "unable to open database file".
2. After that, libnix fcntl byte-range locks fail -> SQLITE_IOERR_LOCK (3850).
Fix: register a default VFS "amiga" cloned from `unix-none` (no locking, one owner) with xFullPathname = identity.
Also: SQLITE_CONFIG_MEMSTATUS is now switched on (SQLite is built with MEMSTATUS=0), else the heap peak reads 0.

## Measured (vamos, 68020, --machine a1200, heap 524288 bytes)
- db_heap_peak = 457184 (87% of the 512 KB tier heap); stats run 452640.
- query_ms = 20 (search), 40 (stats, two calls); vamos clock, not hardware time.
- sqlite_text_bytes = 1045456 (`m68k-amigaos-size -t` of the SQLite static library, measured by the test script;
  the CLI cannot report its own text size).
- Fixture is 5 rows; the peak is dominated by schema migration and FTS, not row count. Real-size index on the A1200: open.

## Red before / green after
- Before the VFS fix T96 failed `RED[T96]: 'library search --index' exited 1 on 68k`.
- Before the FTS fix the desktop test failed 3 golden searches (alias in `f MATCH` is invalid in FTS5).

## Open
- Large-index timing and page-cache pressure on real hardware (rig reserved).
- `amiga-library-vamos` needs build/amiga built first (like the other vamos tests).
