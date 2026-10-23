# I173e and I190b report

## I173e: net-guard under ctest -j2
- Cause: net-guard asserts the whole fixture request log has 1 line, but shared the one net-fixture server and log
  with every other net test. RESOURCE_LOCK serialises tests, not order: under -j2 another fixture test can run
  first and add lines. net-guard was only ordered first by number (plain -j2 happened to pass).
- Before: `ctest -R 'net-fixture|net-guard|modland-net|radio|similar|smart|rate-devilbox|loudness-fetch' -j2 --schedule-random`
  failed net-guard in 3 of 6 runs.
- Fix (c655d40): net-guard starts its own fixture server with its own WORK dir (`tests/net-guard`) and log, the
  pattern vote-devilbox already uses; no FIXTURES_REQUIRED or lock needed.
- After: 8 of 8 randomised runs green; `ctest -R net -j2` green twice.
- Open: http-client keeps a now-redundant `DEPENDS net-guard` and comment; harmless.

## I190b: no libnix startup constructor in the Amiga selecta-cli
- New `cmake/no_startup_ctor.cmake` (mirrors no_float.cmake: NM over the build tree) and ctest `no-startup-ctor`
  in tests/CMakeLists.txt next to no-float-engine (FIXTURES_REQUIRED amiga_cli, so amiga is rebuilt first).
  It scans the linked executable (a constructor is pulled in at link time, objects cannot show it) for `initclock`.
- Passes on current code. With `long f(void){(void)clock(); return (long)clock();}` appended to
  src/engine/engine.c it failed ("libnix startup constructor linked ... initclock"); reverted, amiga rebuilt, green.
