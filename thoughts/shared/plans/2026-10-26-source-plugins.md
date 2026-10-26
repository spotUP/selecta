---
date: 2026-10-26
author: spot
status: approved
ledger: I517
design: thoughts/shared/plans/2026-10-22-architecture-design.md section 9 (F33) and 9.1
---

# Sources as plugins (I517)

## Problem

Johan, 2026-10-26: "sources should be modular like plugins it should be easy to add new sources".

Today a source is not modular. Adding one touches five places outside the source itself:

1. `src/library/source.h` declares a `sel_source_<id>()` getter per source.
2. `sel_source_register_builtin` (`src/library/source.c:48`) registers each source by name.
3. `cmd_LIBRARY_SYNC` (`src/engine/engine.c:4553`) is a hard-coded if-chain: one `is_<id>` flag, one stats
   struct and one `sel_<id>_sync` / `sel_<id>_net_sync` call per source, and it copies source-specific fields
   (hippo_lists, prg_*, songlengths, stil_entries, subtunes, net_zip_bytes, changelogs) into `sel_sync_info`.
4. `sel_sync_info` (engine) carries those per-source fields.
5. `src/library/CMakeLists.txt` lists every source file.

`SEL_PLUGIN_KIND_SOURCE = 7` (`include/selecta/plugin.h:20`) exists but nothing uses it. The architecture doc
contradicts itself: section 9 (F33) says "each source is its own plugin", section 9.1 says "one C file per
source under `src/library/sources/<id>.c`". This plan settles it for the plugin form; 9.1 gets corrected.

## Invariant

A new source is one directory, `plugins/sources/<id>/`, holding its C files and a `CMakeLists.txt` with one
`sel_add_plugin(<id> 7 ...)` call. No file outside that directory changes. It then appears in the SOURCES verb,
on the desktop Sources page (I265), syncs through `LIBRARY_SYNC <id> <arg>`, and plays through the cache.

## What sources depend on today

Measured from the calls in `src/library/sources/*_sync.c`, `*_net.c` and `sync_net.c`:

| Need | Today | Plugin form |
|------|-------|-------------|
| HTTP GET, Range, HEAD, polite pause | `sel_http_request`, `sel_sync_net_*` (`src/library/net/http.h`, `sync_net.h`) | host service `net` |
| Write songs, albums, lists, art to the library | `sel_library_request`, `sel_sync_flush_scan` (`scfg.lib`) | host service `catalogue` |
| Cache and config directories | `sel_cache_init`, `sel_config_mkdir` | host service `catalogue` (dir fields) |
| Archive unpack (zip, LZX, PP20) | `sel_archive_extract_file`, `archive_*.c` | `plugins/shared/archive/` |
| JSON scan, URL path, text helpers | `json_scan.c`, `url_path.c`, `sync_util.h`, `sel_latin1_to_utf8`, `sel_url_unquote` | `plugins/shared/source/` |
| Local folder listing (offline sync from a mirror copy) | `sel_browse_*` (`src/library/local.h`) | host service `catalogue` (`list_dir`) |
| Plugin registry for format lookup | `scfg.registry_get` | existing `sel_host.query` |

## Design

### D1. Public descriptor: `include/selecta/source.h`

Move `sel_source`, `sel_listing_sink`, `sel_progress`, `sel_transport`, `sel_list_sink`, `sel_playlist` out of
`src/library/source.h` into the SDK unchanged. Add three members at the end (the struct is static, built per
plugin, so appending is safe):

```c
    /* Fetch or refresh this source's catalogue. `arg` is a URL or a local mirror path; the source decides which
     * (the engine no longer calls sel_modland_is_url). Stats go out as tags (SEL_SYNC_*), so the engine needs no
     * per-source field. 1 on success. */
    int (*sync)(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t stats_cap);
    uint32_t flags; /* SEL_SOURCE_REMOTE, SEL_SOURCE_SYNCS, SEL_SOURCE_NEW_THIS_WEEK */
    const char* sync_default; /* default URL for the Sources page Sync button; NULL when none */
```

`sel_source_is_remote` reads `SEL_SOURCE_REMOTE` instead of comparing ids (removes the last id literal, I211b).

### D2. Host services, appended to `sel_host`

Follow the existing `voices` accessor pattern: one function pointer per service returning a const vtable, NULL
when the build has none. Append below the "append only" line, bump `SEL_API_VERSION`.

```c
    const struct sel_net* (*net)(const sel_host*);             /* include/selecta/net.h */
    const struct sel_catalogue* (*catalogue)(const sel_host*); /* include/selecta/catalogue.h */
```

- `sel_net`: `get_file`, `get_range`, `head_length`, `pause`, plus a raw `request` for sources that page through
  an API. Same signatures as `sync_net.h` today, with `const sel_host*` first. The engine implements it over
  `src/library/net/http.c`; the Amiga build gets the same table over its bsdsocket stack (I199).
- `sel_catalogue`: `begin(source_id)`, `song(path, size, md5, format, artist, title, ...)`, `album(...)`,
  `art(...)`, `list(id, name, paths, count)`, `commit()` / `abort()`, `cache_dir(source_id)`, `list_dir(path, sink)`.
  The engine implements it over `sel_library_request` and `sel_sync_flush_scan`, so batching and transactions
  stay where they are. Plugins never see the DB.

The engine passes a `sel_host` whose `ctx` points at the engine's library; services are bound per sync call.

### D3. Shared helper code

Move pure helpers to `plugins/shared` (rule 3 allows it): `archive/` (archive.c, archive_lzx.c, archive_pp20.c),
`source/` (json_scan.c, url_path.c, the pure parts of sync_util.h, latin1, url_unquote). The core links the same
library where it still needs them (`local` source, URL OPEN). No copy stays behind.

### D4. One directory per source

```
plugins/sources/CMakeLists.txt         file(GLOB) each child with a CMakeLists.txt, add_subdirectory it
plugins/sources/zxart/CMakeLists.txt   sel_add_plugin(zxart 7 zxart.c zxart_sync.c zxart_net.c)
plugins/sources/zxart/zxart.c          descriptor + sel_plugin_entry returning it
```

The glob is what makes "no edit outside its directory" true; the static registry already comes from the
`SEL_PLUGINS` property (`cmake/selecta_registry.cmake`), so a new `sel_add_plugin` call joins it with no list edit.
Per-platform opt-out: the child's CMakeLists may `return()` on a build that should not ship it (the Amiga tier
decides by its own option, not the parent).

`local` stays in core: it is the filesystem scanner, shared with drag-and-drop and folder watching, and every
build has it. Radio (I410) lands as a plugin.

### D5. Registry fill and engine dispatch

- `sel_source_register_builtin` registers `local`, then walks the plugin registry for kind 7 and registers each
  descriptor. Duplicate or over-long ids are rejected with a log line naming the plugin (today they fail silently).
- `cmd_LIBRARY_SYNC` becomes: find the source, `unknown_source` if missing or without `sync`, call
  `src->sync(host, arg, now, stats, cap)`, copy generic stats (`SEL_SYNC_LISTING_ROWS`, `SKIPPED_ROWS`,
  `NET_REQUESTS`, `NET_BYTES`, `NEW_THIS_WEEK`, ...) into `sel_sync_info`, and keep source-specific ones as a tag
  list that `sel_engine_sync_info` and the SYNC_RESULT event pass through unchanged. `persist_new_this_week` runs
  when the source sets `SEL_SOURCE_NEW_THIS_WEEK` (today: modland only, by negated id test).
- The per-source fields in `sel_sync_info` go. Tests that read them (`hippo_lists`, `prg_*`, `songlengths`,
  `stil_entries`, `subtunes`, `net_zip_bytes`, changelog counts) read the tag list instead.

## Phases

Each phase leaves every existing source test green and is one commit or a short series.

1. **SDK and services.** `include/selecta/source.h`, `net.h`, `catalogue.h`; append `net` and `catalogue` to
   `sel_host`; engine implementations; generic stats tags. Sources still compiled in core, but `cmd_LIBRARY_SYNC`
   already dispatches through `src->sync` (core sources get thin `sync` wrappers). The if-chain is gone here.
2. **Shared helpers** to `plugins/shared/archive` and `plugins/shared/source`.
3. **Pilot: zxart** to `plugins/sources/zxart/`, off the services only. Proves sdk-isolation and the incheck
   (`sel_incheck_zxart`) accept a real source.
4. **The rest**, one source per commit: sndh, asma, keygenmusic, hvsc, hippo, modland (largest last).
5. **Cleanup.** Remove the `sel_source_<id>()` getters, the per-source CMake lines, the per-source
   `sel_sync_info` fields; correct design doc 9.1 ("one directory per source under `plugins/sources/<id>/`").
6. **New sources** I405 aminet, I406 modulespl, I407 amp, I408 amigaremix, I409 kwed, I410 radio, I411 others,
   each written straight as a plugin.

## Reachability (definition of done)

One test, `src-plugin-reach`, through the real entry point:

- A test-only source `tests/sources/testsrc/` (directory with its own CMakeLists, added only when
  `SEL_BUILD_TESTS`) whose `sync` writes 3 songs through `catalogue` and bumps a sentinel counter.
- Drive the engine over its command channel: `SOURCES` lists `testsrc` with name and kind; `LIBRARY_SYNC testsrc
  x` returns ok, the sentinel is 1, `SOURCES` now reports count 3; the desktop Sources page dump (I265 harness)
  shows a `testsrc` row with 3 songs.
- A CMake check asserts the test source's directory is the only place its id appears in the tree (proves the
  "no edit outside" invariant from the structure, not from a grep of one spelling).
- Revert check: drop the plugin-registry walk from `sel_source_register_builtin`; the test must fail.

Also green after every phase: `sdk-isolation`, every `sel_incheck_*`, the source sync tests, `ctest -L dui -LE dui-red`.

## Decisions (Johan, 2026-10-26: all three as recommended)

- `local` stays in core (recommended: shared with folder scanning, every build needs it).
- Discovery by `file(GLOB)` over `plugins/sources/*/CMakeLists.txt` (recommended: the only way a new source needs
  no edit outside its directory; cost is a re-run of cmake after adding a directory, which a new source needs anyway).
- Per-source sync stats leave `sel_sync_info` and travel as tags (recommended: otherwise every source edits the
  engine struct again).

## Constraints

- Amiga: the services must exist in the Amiga build before a network source is useful there; until I199 works on
  the rig, `net` may return NULL and sources then report `no_network` from `sync`.
- Do not land while I265 (Sources page) is open in `desktop_ui.c` and `engine.c` `cmd_SOURCES`; this plan starts
  after I265 is committed.
