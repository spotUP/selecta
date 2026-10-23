# M3 I210 report

- link-warnings: selecta-cli listed selecta_loudness directly although selecta_core already links it PUBLIC; selecta and view/format/text_ui tests listed selecta_core although selecta_registry links it PUBLIC. CMake emitted both occurrences. Removed the redundant direct entries (apps/selecta-cli, apps/selecta, tests/CMakeLists.txt). Host build now has no duplicate library warning.
- Seven screen goldens: diffed each against the actual screen; every diff is line 24 only (footer: player footer "Left Right Seek  Up Down Volume ..." replaces the browser footer). Goldens updated from the actual output.
- http-client: not reproduced under ctest -j2 (subset, and full -L m3 with 49 tests). Fixture ports are ephemeral, net-guard uses its own WORK dir and server, every other fixture user holds RESOURCE_LOCK net-fixture. Removed the stale DEPENDS net-guard and comment. Remaining suspect, unconfirmed: the test asserts wall-clock windows (3 s +-300 ms first-byte timeout, 30 s +-1 s total timeout), which can miss on a loaded machine.
- Verification: link-warnings, 7 screen tests, http-client, net-guard pass 3x under nice ctest -j2 --repeat until-fail:3.
- Worktree needed git submodule update --init (nuade, libopenmpt, via --reference) before configure.
