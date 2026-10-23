# I170: no-float-engine object set derived from the targets (D-M3-3)

## What was wrong
cmake/no_float.cmake globbed the objects and pinned a hand count of 29. The build now has 50 (20 engine,
protocol, mixer, wav, null plus 30 selecta_library objects), so T18 failed on the count, not on float.

## What uses float
`m68k-amigaos-nm -u` over every selecta_library object (30) and over third_party/sqlite:
- selecta_library: no soft-float references in any of its 30 objects (db, worker, search, scan, fetch, query,
  regex, net, cache, sources, playlist, export). No real bug found.
- selecta_sqlite (`sqlite3.c.obj`): references ___adddf3, ___divdf3, ___muldf3, ___subdf3, ___floatsidf,
  ___fixdfsi, ___extendsfdf2 and compare helpers. Allowed by D-M3-3 (library worker, double allowed).
- The R128 analyser is desktop-only and not in the 68k build.

## What was excluded, and why
Only whole targets that D-M3-3 allows: `selecta_sqlite` (never listed, and it was never inside the
`selecta_library.dir` glob either). selecta_archive and mbedtls are separate targets that were never in the
list and are untouched. Nothing inside selecta_library is excluded.

## Change
- src/CMakeLists.txt (SEL_AMIGA branch): `file(GENERATE)` writes `build/amiga/no-float-targets.txt`, one line per
  checked target (selecta_core filtered to engine/ and protocol/, selecta_mixer, sel_plugin_wav,
  sel_plugin_null, selecta_library): name, source dir, binary dir, filter, source list. Same pattern as
  m2-interrupt-objects.txt.
- cmake/no_float.cmake: maps each listed .c to the object CMake produces, globs what is on disk per target, and
  fails on any difference in either direction (expected but not built, built but not in a target's sources),
  then runs the nm check on the set. The hand count is gone.

## Guard proof
- Stray `stray.c.obj` in selecta_library.dir: fails ("built but not in a target's sources").
- `md5.c.obj` moved away: fails ("expected but not built").
- `volatile double` multiply appended to src/engine/engine.c, amiga rebuilt: fails with
  `selecta_core.dir/engine/engine.c.obj: __muldf3`. Reverted, rebuilt.
- `ctest -R no-float`: amiga-build, no-float-engine, no-float-interrupt pass (50 objects).
