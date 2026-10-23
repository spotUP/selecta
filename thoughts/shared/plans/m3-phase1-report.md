# M3 Phase 1 report: SQLite, library worker, schema v1 (I144, I145, I146)

Plan: `thoughts/shared/plans/2026-10-25-m3-library.md`, Phase 1, D-M3-1/2/3. Test: T62 `library-db-open`.

## Rows
- I144: SQLite 3.54.1 amalgamation vendored in `third_party/sqlite/` (SHA3-256 matches sqlite.org), target `selecta_sqlite` with the D-M3-1 options. Host and 68k trees build; `library stats` prints `sqlite_version=3.54.1`.
- I145: worker thread with request and result SPSC queues (`src/library/db/worker.c`), memsys5 heap allocated once at open through `host->mem_alloc` (`db.c`).
- I146: schema v1, `user_version` migrations, FTS5 `song_fts`, account handle on every entry point (`sel_library_open(host, machine, account, path)`).

## Tests
- T62 red: commit "Add T62 library-db-open red test (I144-I146)": `95 - library-db-open (Failed)`.
- T62 green after implementation. Also green: `link-warnings`, `no-alloc-render-queue`, spsc tests. `selecta-cli commands --unimplemented | wc -l` is 21.
- T62 checks: `db_open=1`, `schema_version=1`, `fts5=1` (insert, prefix MATCH, rollback), heap bytes per machine (a500 524288, a1200 524288, a1200-fpu 2097152, a060 8388608, desktop 33554432), one heap allocation, 2+ worker requests, and on macOS zero `malloc` calls between `SEL_ALLOC_FREEZE` and `SEL_ALLOC_THAW` (count_malloc shim).
- Amiga: `build/amiga` links `selecta` and `selecta-cli` with SQLite. Not run on the rig.

## Deviations and decisions
- The plan gives no heap table; I chose it over FAST_RAM + CHIP_RAM: <=4 MiB 512 KiB, <=16 MiB 2 MiB, <=128 MiB 8 MiB, above 32 MiB. Veto if wrong.
- The Amiga build runs requests on the caller's task (`SEL_LIBRARY_NO_THREAD`) until the library subtask in Phase 12.
- The heap is process global (SQLite requirement) and shared by all account handles; freed on the last close.
- `SEL_LIB_REQ_STOP` is declared but unused (the thread stops via a flag).
- Requests are `sel_message` with private codes, not `commands.def` verbs; engine forwarding of LIBRARY_* is not part of Phase 1.
- `third_party/sqlite/amiga_stubs.c` stubs `fchmod` and `fchown`, which libnix lacks and the unix VFS references. SQLite stays on the unix VFS on Amiga; file DB behaviour there is untested.
- `selecta_library` no longer links `selecta_core` privately: that caused the `link-warnings` duplicate-library failure. Core symbols resolve through the consumers' link lines.

## Open questions
- Ledger rows I144-I146 not ticked (launcher owns `thoughts/spot/todos.md`).
- Amiga file-backed DB (Lock/Open VFS behaviour, fast RAM heap placement) needs the rig, Phase 12.
- Other targets print pre-existing `ignoring duplicate libraries: libselecta_core.a` ld warnings; only the `selecta-cli` link is gated by the test.
