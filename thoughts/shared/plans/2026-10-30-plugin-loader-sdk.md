---
date: 2026-10-30
author: Opus (musicplayer-55)
status: decided (Johan 2026-10-30: D6 everything a module, D9 loader, SNDH, then SDK)
rows: I427a-I427e
source: architecture design section 10 (2026-10-22-architecture-design.md:439-460), M0 skeleton "Loaders" (2026-10-22-m0-skeleton.md:42)
---

# Runtime plugin loader and SDK

## Why now

I427 (SNDH via sc68, GPL-3) must ship outside the MIT core (Johan 2026-10-30: "separate GPL plugin";
"the original plans say ui's and replayers should be plugins and we should offer sdk/docs").
Today every plugin is a static library (`cmake/selecta_plugin.cmake:33`, `sel_add_plugin`) and the registry is a
compile-time array (`src/engine/registry.c:76`, `sel_registry_entries[]`). The design always planned more:
section 10 names `dlopen`, `LoadLibrary` and Amiga `LoadSeg` loaders that "all feed the same registry", and
section 1 promises "a published C SDK". The ABI side exists already: one entry symbol
`sel_plugin_entry(uint32_t host_api)` returning a tag list, plain name when `SEL_PLUGIN_ENTRY` is not defined
(`include/selecta/plugin.h:30-35`), SDK isolation checked by `sdk-isolation` and `sel_incheck_*`.

## Decisions (defaults; the ones marked [Johan] wait on him)

D1. Loader. New `src/engine/loader_dl.c`: POSIX `dlopen`/`dlsym` now, `LoadLibrary` under `#ifdef _WIN32` in the
same file when Windows lands. It calls a new `sel_registry_add_entry(sel_plugin_entry_fn, const char* origin)`
before `sel_registry_init_machine`; init walks the static array first, then the dynamic entries, through the
same `fits()` filter. Handles stay open for the process lifetime (no unload in v1).

D2. Module build. `sel_add_plugin(<name> <kind> MODULE <src>...)` builds `add_library(... MODULE)` without the
`SEL_PLUGIN_ENTRY` rename, so the module exports plain `sel_plugin_entry`; `-fvisibility=hidden` with only the
entry exported. A MODULE plugin is not added to `SEL_PLUGINS`, so it never enters the static registry or the
core binary. The same source can still build static (tests, 68000 builds).

D3. Search path, first hit per file name wins: `$SELECTA_PLUGIN_PATH` (colon list), then the app's own plugin
folder (`Contents/PlugIns` in the macOS bundle, `<exe dir>/plugins` elsewhere), then the user plugin folder next
to the existing config dir (reuse the config path helper, do not add a new one). Files: `*.selplug` suffix?
No: platform suffix (`.dylib`/`.so`/`.dll`) with a `sel_` prefix, so tools recognise them.

D4. Validation. `dlsym` miss, `entry(SEL_API_VERSION)` returning NULL, or a malformed descriptor: skip, log one
line with path and reason, count in `sel_registry_stats` (new fields `dyn_loaded`, `dyn_rejected`). Same
`SEL_PLUGIN_ID` as a plugin already registered: the first one wins (static before dynamic), log the duplicate.
`selecta-cli plugins` lists every plugin with origin (`static` or the module path).

D5. Licence boundary. A GPL module includes only `include/selecta/` (MIT) and links only its own vendored code.
The core build never links it; `SELECTA_GPL_PLUGINS` (default OFF) only adds the module targets. Proof: `nm` of
the core binary shows no sc68 symbol with the option ON. The SNDH module ships as its own archive with GPL-3
COPYING and the sc68 source (or a written offer), never inside the MIT release archive.

D6. Every plugin is a module (Johan 2026-10-30: "Everything a module"). Desktop builds (macOS, Linux, Windows)
build every replayer, output, DSP, viz, converter, source and UI plugin as a MODULE and load them through D1; the
core binary holds engine, registry and loaders only. The static registry stays for builds that cannot load
modules (68000 Amiga until I427e, web port) and for tests that link a plugin directly: CMake option
`SELECTA_PLUGIN_LINK` = `MODULE` (desktop default) or `STATIC` (Amiga, web, test builds). Missing built-in
modules at start are an error the UI reports, not a silent empty registry.

D7. SDK. `install(DIRECTORY include/selecta)` plus an exported CMake package `SelectaSDK` with an INTERFACE
target `Selecta::sdk` (headers + the module compile flags). `examples/plugin-sine/` is an out-of-tree replayer
built only through `find_package(SelectaSDK)`. Docs in `docs/sdk/`: README (ABI, entry, tags, host table,
versioning rules from section 10), `replayer.md` (section 4 contract), `building.md` (module build, search path,
licence notes). Every SDK claim cites the header it documents; the header stays the source of truth.

D8. Amiga. `LoadSeg` loader with the entry at the first hunk is M4 players plan work (2026-10-24-m4-amiga-ui.md:16)
and gets its own row (I427e). SNDH on Amiga is not in scope.

D9. Order (Johan 2026-10-30): I427a loader, I427b SNDH plays, I427f built-ins to modules, I427c SDK + docs,
I427d packaging, I427e Amiga LoadSeg.

## Rows

- I427a Loader + MODULE build + search path + stats + `selecta-cli plugins`. Done: a test fixture module
  (tiny replayer under `tests/fixtures/plugin_dyn/`) loads through `$SELECTA_PLUGIN_PATH` and
  `selecta-cli render` produces its known output; sentinel proves the dynamic entry ran; core binary does not
  contain the fixture's symbol; rejected-module cases (no symbol, API mismatch, duplicate ID) each counted.
- I427b sc68 SNDH module under `SELECTA_GPL_PLUGINS`: vendor `third_party/sc68` (COPYING GPL-3, VERSION.txt,
  PATCHES.txt), `plugins/replayers/sndh` as MODULE. Done: a real ICE!-packed SNDH renders non-silent through
  the loader from the desktop open path; `nm` core shows no sc68 symbol; option OFF keeps
  "No player for SNDH yet".
- I427c SDK: install + `SelectaSDK` package, `examples/plugin-sine`, `docs/sdk/`. Done: CI builds the example out
  of tree against the installed SDK and renders it through the loader.
- I427d Packaging: macOS bundle `PlugIns`, separate GPL archive for the SNDH module.
- I427f Built-ins to modules (D6): `SELECTA_PLUGIN_LINK`, every `sel_add_plugin` honours it, bundle copies the
  modules, startup reports a missing built-in. Done: desktop `selecta` binary links no `sel_plugin_*` (nm), the
  full dui and engine test set passes with modules loaded from the build tree, Amiga and web builds stay static.
- I427e Amiga `LoadSeg` loader (M4 players plan).
