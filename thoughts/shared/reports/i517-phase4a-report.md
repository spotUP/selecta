# I517 phase 4a: sndh and asma as source plugins

Commits: sndh bba2abb, asma a86384c. One commit per source, pattern of zxart (c8e67f8).

## Conversion pattern
- git mv to plugins/sources/<id>/, new CMakeLists with sel_add_plugin(<id> 7 ...) linking selecta_shared_source (sndh also selecta_shared_archive).
- sel_local_scan_config becomes const sel_host*. Network via host->net, catalogue rows via host->catalogue (begin, song, commit/abort, cache_dir). No batch arrays; the engine batches.
- Descriptor via SEL_PLUGIN_ENTRY returning sel_source_plugin_descriptor.
- sndh: parse is exported as sel_sndh_parse because the sync used src->parse. Stat "batches" stays 0 in both.
- Core: registration, getter, engine.c includes and library CMake source lists removed. sel_plugin_sndh and sel_plugin_asma added to SEL_NOFLOAT_TARGETS.
- Unit tests fetch the descriptor via sel_plugin_entry(1) as the zxart test does.

## Order change
Plugin sources register after builtins. Registry order is now local, modland, hvsc, hippo, keygenmusic, asma, sndh, zxart, testsrc. Updated tests/m3/sources.expected, sources_off.expected, want_keys in tests/dui/library_sources.cmake. Golden tests/golden/desktop/library-sources-rgba32-1280x800@1.hash regenerated (row order changed).

## Gotcha
Stale build/amiga selecta_library obj files for moved sources make no-float-engine fail; delete sources/<id>*.obj.

## Results (build/host)
sndh-unit, asma-unit, source-sndh-net, source-asma-net, src-plugin-reach, sources, sources-off, sdk-isolation(+fixture), link-warnings, registry-tags-negative, no-float-engine, amiga-build, sel_incheck_sndh and sel_incheck_asma green. build/sdl: dui-library-sources green after golden update.

## Unverified
clang-format not installed. Amiga only via cross build / amiga-build, no rig run.
