# I517 phases 2 and 3 report

Design: thoughts/shared/plans/2026-10-26-source-plugins.md (D3, D4, D5, Phases 2 and 3, Reachability).

## Phase 2 (commit beafd85)
archive, json_scan, url_path and latin1 moved to plugins/shared (targets selecta_shared_source, selecta_shared_archive); plugins/shared/source/sync_text.h added.

## Phase 3
- zxart moved with git mv to plugins/sources/zxart/ (zxart.c, zxart_sync.c/.h, zxart_net.c/.h). It uses only the net and catalogue host services and exports SEL_PLUGIN_ENTRY. One `sel_add_plugin(zxart 7 ...)`; plugins/sources/CMakeLists.txt finds children by file(GLOB).
- New SEL_SRC_DESCRIPTOR tag (0x7017) in tags.def; plugins/shared/source/descriptor.h and sync_stat.h.
- `sel_source_register_builtin` registers local plus the six other built-ins, then walks the plugin enumeration for kind 7 (`register_plugins` in src/library/source.c). Duplicate or over-long ids are rejected with a log line.
- Link cycle: library -> registry -> core -> library produced Apple ld duplicate-library warnings (link-warnings test). Root fix: the library does not link the registry. The front end hands the enumeration in, as it already does for the engine: `sel_source_set_plugins(count, get, host)` is called by `sel_engine_create` from cfg->registry_count / registry_get. Frees the Amiga static link of a LINK_INTERFACE_MULTIPLICITY workaround. Consequence: a caller of sel_source_register_builtin outside an engine sees no plugin sources.
- Reachability test `src-plugin-reach` (tests/engine/src_plugin_reach_test.c, tests/dui/src_plugin_reach.cmake, test source tests/sources/testsrc under SEL_BUILD_TESTS): SOURCES lists testsrc with name and kind; LIBRARY_SYNC testsrc x returns ok, sentinel 1, SOURCES count 3; grep proves "testsrc" appears nowhere under src/, include/, plugins/, apps/; the SDL half checks the desktop Sources page dump row `testsrc|remote|Test Source|1|3`.
- Revert check (redone after the link fix): replacing the `register_plugins();` call with `(void)register_plugins;` makes src-plugin-reach, sources and sources-off fail; restored, all pass.
- Golden tests/golden/desktop/library-sources-rgba32-1280x800@1.hash updated (testsrc row, keygenmusic before zxart). tests/m3/sources*.expected updated likewise.
- tests/CMakeLists.txt: the registry-tags-negative expectation excludes testsrc (the nested bad-plugin tree has SEL_BUILD_TESTS=OFF).
- SEL_NOFLOAT_TARGETS now lists selecta_shared_source and selecta_shared_archive. sel_plugin_zxart is NOT listed: sel_zxart_millis uses double and strtod. The same code sat in selecta_library at HEAD (its object also carried __adddf3), so the float use is pre-existing.

## Results
Host: link-warnings, registry-tags, registry-tags-negative, src-plugin-reach, sources, sources-off, zxart-unit, source-zxart-net, sdk-isolation (+fixture), library-sync-dispatch, abi, amiga-build, no-float-engine pass; lint target OK.
SDL: `ctest -L dui -LE dui-red` 55 of 56 on the full run; dui-hover-nowplaying failed once under load and passed alone. dui-library-sources passes after the golden update. dui-keys passes. src-plugin-reach passes with the SDL half.

## Not green / unverified
- dui-footer: Timeout (120 s) when run alone twice; passed in the full dui run. Unverified whether pre-existing; not touched by this change.
- home-view (#193): home_rows=14 want 13. Cause is the Quick picks grid from I478 (e2e3346, 2026-10-26), after the test was written. Not caused by I517.
- pattern-cells (#106): tracker META_MESSAGE diff in tests/m13/pattern.expected; unrelated, unverified pre-existing.
- amiga-console-latin1-name-box-glyph-vamos (#144) fails; unverified as pre-existing. amiga-new-library-database-is-not-junk (vamos out of memory), amiga-machine (FAST_RAM 3751096 vs 8384512), rig-* tests: environmental, unverified.
- clang-format is not installed: the format target is unverified.
- Amiga is untestable here beyond the cross build (amiga-build passes).
