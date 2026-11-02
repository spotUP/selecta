# Vendored SQLite (plan 2026-10-25-m3-library.md, D-M3-1)

- Version: 3.54.1 (`sqlite-amalgamation-3540100.zip`, source `https://sqlite.org/2026/sqlite-amalgamation-3540100.zip`)
- Files kept: `sqlite3.c`, `sqlite3.h` (unmodified), plus `shell.c` from the same zip (unmodified; built only for the host as `sel_sqlite3_shell`, the sqlite3 CLI the tests use because the system one may lack FTS5; never on Amiga). `sqlite3ext.h` is not used
- SHA3-256 of the zip (matches the hash column of https://sqlite.org/download.html): `92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30`
- SHA-256 of the zip: `0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9`
- Build options live in `CMakeLists.txt` here (target `selecta_sqlite`).
- A fix goes upstream or into `third_party/patches/` (RULES.md rule 5); the sources here are never edited.
