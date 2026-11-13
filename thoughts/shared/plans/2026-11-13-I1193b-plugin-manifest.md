---
date: 2026-11-13
row: I1193b (Mac cold start; follows I1193, reuses I427e)
status: accepted 2026-11-13 (Johan picked all recommended options)
builds-on: thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md (manifest + resolver), thoughts/shared/plans/2026-10-30-plugin-loader-sdk.md (D6 every plugin a module)
measurement: thoughts/spot/agent-reports/I1193b.md
---

# Mac plugin manifest: open modules on demand

## Problem and invariant

`sel_registry_init_machine` (src/engine/registry.c:328) calls `sel_loader_scan` (src/engine/loader_dl.c), which
dlopens every `sel_*.dylib` (104 in the app) only to call `sel_plugin_entry` and read the descriptor. Measured
(I1193b report): 234-468 ms warm, 1973 ms cold, and 330-460 ms PER module for files the OS has not seen yet
(first run after a build or install: 34-48 s), serialised under dyld's global write lock; threads do not help.

Invariant: the UI is interactive within 1 s of launch, independent of how many plugins are installed. Restated
for the loader: the number of modules opened before the first frame is a constant (the UI and output that run),
never a function of the plugin count.

Root-cause level: the registry. It holds a live descriptor per plugin, and on the desktop the only way to get one
is to open the module. The Amiga already split the descriptor into a manifest (static facts + probe, linked in)
and a module (loaded on first resolve) in I427e. The Mac needs the same split; the resolver API, the
`SEL_PLUGIN_MODULE` tag, `sel_plugin_manifest`, `sel_registry_stats.module_loads` and the two resolve call sites
already exist. This design adds a desktop manifest source and a dlopen resolver, nothing parallel.

## What the start path needs vs what needs hooks

Read without opening anything (plain data, tag types INT / STR / LIST in include/selecta/tags.def):

| Consumer | Tags |
|---|---|
| `descriptor_ok`, `keep`, dedupe (registry.c) | PLUGIN_KIND, PLUGIN_ID, PLUGIN_VERSION |
| `fits` (registry.c:254) | RP_MIN_CPU, RP_NEEDS_FLOAT, RP_NEEDS_FPU, RP_MEMORY, OUT_MEM_CHIP |
| `sel_registry_format_owner`, `sel_replayer_outranks` | RP_FORMATS, RP_PRIORITY, RP_NEEDS_FLOAT |
| `sel_hooks_claim_name` (claims.c:29), file filters | RP_EXTENSIONS (doubles as Amiga prefixes: `mod.foo` matches on the first segment) |
| Settings / Formats pages, `plugins` listing, defaults (`sel_registry_default_ui/output/backend/integration`) | RP_NAME, version, kind, ui-listed flags |
| `sel_registry_sysfiles` (I710ga) | RP_SYSFILE (LIST of INT/STR) |

Needs the module (PTR tags): every hook function (open, render, seek, meta, ...), and SEL_RP_PROBE.

So the manifest carries every INT, STR and LIST tag of the descriptor verbatim, and for PTR tags only the fact
that they exist. The probe is the one PTR the start path would like; see "Probing".

## Manifest: format, where the build writes it, single source of truth

Format: generated C, compiled into ONE module `sel_manifest.dylib` (`.so` on Linux) in the same folder as the
built-ins (`${SEL_BUILTIN_MODULE_DIR}`, bundled into Contents/PlugIns). One dlopen at start, no runtime parser,
the compiler checks the tables. It exports `sel_manifest_entries[]` (NULL ended list of `sel_plugin_entry_fn`),
one per plugin, each returning a descriptor exactly like the Amiga manifest unit: KIND, ID, VERSION,
`SEL_PLUGIN_MODULE` = module file name, TAGS = static tag list. Present PTR tags are listed as a LIST under a new
tag `SEL_PLUGIN_HOOKS_PRESENT` (tag ids only, never a pointer), so "does this UI plugin have a settings page"
answers without a load (`sel_hooks_has(hooks, tag)`, new helper in claims.c, falls back to `sel_tag_find != 0`).

Two producers, both from the plugin's own source:
1. Generated (default, every desktop module): `selecta-cli plugins --manifest-c <out.c>` opens every built-in
   module (build machine only, files are warm right after link), walks each descriptor with the tag type table
   from tags.def (src/core/tag.c), and writes the C. Nobody edits a manifest; it is a dump of the descriptor the
   module returns.
2. Hand-split (`probe.c` built with `SEL_PLUGIN_MANIFEST`, the 12 I427e plugins today): compiled into
   `sel_manifest.dylib` as-is, so their real probe runs without loading the module, same file as on the Amiga.
   The dump skips a plugin that has such a unit. The CMake keyword `AMIGA_MANIFEST` becomes `MANIFEST` and
   applies to both platforms.

CMake (cmake/selecta_registry.cmake, next to the `sel_registry_builtin_modules` generation): an
`add_custom_command(OUTPUT manifest_gen.c DEPENDS <every SEL_BUILTIN_MODULES target> selecta-cli)` runs the dump
with `SELECTA_NO_MANIFEST=1 SELECTA_BUILTIN_MODULES_DIR=${SEL_BUILTIN_MODULE_DIR}`; `sel_manifest` is
`add_library(MODULE manifest_gen.c <MANIFEST units>)` linking selecta_core like every module (D6). No cycle:
modules and selecta-cli do not depend on `sel_manifest`. Any plugin change relinks its module, which reruns the
dump. scripts/install-mac-app.sh copies it with the rest of PlugIns (glob already `sel_*.dylib`).

Sync checks: (a) build-time dependency above; (b) on first resolve the loader compares the module's real ID and
VERSION with the manifest (loader_seg.c already does this) and rejects a mismatch via
`sel_registry_note_rejected`; (c) ctest `plugin_manifest_matches` re-dumps from the built modules and diffs
against the compiled manifest (catches an install that mixes an old manifest with new modules).

## Lazy open: who triggers dlopen

`sel_loader_scan` becomes: dlopen `sel_manifest.dylib` from the built-in search folders (env, app, build tree:
same order as `load_builtin_modules`), add each manifest entry with `sel_registry_add_builtin_entry`, mark every
`SEL_PLUGIN_MODULE` name seen (so the general scan of the app folder never opens it), install
`sel_plugin_set_resolver(resolve, manifest_of)`. `resolve` is the loader_seg.c logic with dlopen/dlsym: first call
per manifest opens the module, checks ID/VERSION, caches real hooks, calls `sel_registry_note_module_load`; a
failure is cached and never retried. Modules stay loaded for the process lifetime (no dlclose: C++ statics,
I1314b's Amiga unload does not apply on the desktop).

Call sites that must resolve before touching a PTR tag:
- already done: engine replayer open (engine.c:2904), library scan open/metadata (local.c:240), probe fallback
  (`sel_plugin_probe`, plugin_resolve.c:122);
- to add (row M1 audits them): output open, UI plugin start, viz start, DSP chain add, source plugin
  calls (library sidebar, sync), integration start, converter and scripting use, settings pages that call a
  plugin hook, `sysfile_owner_name` (engine.c, the known I427e gap: match the manifest, not the hooks),
  `selecta-cli plugins --lint` (must resolve everything; it is the one path allowed to open all modules).

Expected start set for `--ui desktop --backend sdl`: `sel_manifest`, `sel_desktop_ui`, `sel_coreaudio`, plus any
integration that auto-starts (nowplaying). 3-4 opens instead of 105.

## Probing

Today `find_replayer` (local.c:125) and the engine's open walk every replayer and call `sel_plugin_probe`. With a
manifest: a hand-split plugin probes in the manifest (no load); a generated manifest has no SEL_RP_PROBE, so
`sel_plugin_probe` takes the existing nuade path: the module loads only when `sel_hooks_claim_name` says the file
name (extension or Amiga prefix) is one it claims, then its real probe runs. Scanning a folder of mp3s therefore
opens minimp3 only.

Behaviour change: a file whose NAME no plugin claims but whose CONTENT one recognises (magic only, renamed files)
is no longer found by a plugin without a hand-split probe. Magic cannot be dumped as data in general (probes are
code: header walks, checksums). The fix is per plugin, the Amiga pattern: move the probe into `probe.c`
(`MANIFEST probe.c`), which also serves Amiga phase 2. Row M6 lists which plugins rely on content-only detection.

A cold library scan of a mixed collection will still open most replayers on the scan thread (dyld lock
serialised). That is after the first frame and off the UI thread; whether a UI-thread dlsym waits on that lock is
unverified (measure in M5).

## Amiga and web parity

Amiga: same mechanism already (manifest descriptors + `SEL_PLUGIN_MODULE` + resolver + `module_loads`); this
design only renames `AMIGA_MANIFEST` to `MANIFEST`. The Amiga keeps hand-split manifests linked into the
executable, because the cross build cannot dlopen 68k modules to dump them. Possible later: the Amiga build reuses
the host dump for C-only players (needs a host build step like cmake/host_bake_tools.cmake) instead of splitting
each probe.c by hand; not in this design.

Web: STATIC by rule (selecta_plugin.cmake:37), one wasm, no dlopen cost: nothing to do. If wasm side modules ever
land, the same generated manifest is the registry source.

## Pin test and revert proof

- Count: `load_module` in loader_dl.c calls `sel_registry_note_module_load` (today only loader_seg.c does), so
  `sel_registry_stats.module_loads` counts every dlopen after the manifest. Add `manifest_entries` to the stats.
- Test 1 (ctest `registry_opens_nothing`): `selecta-cli plugins --stats` with the built modules prints
  `module_loads=0` and `manifest_entries>=90` after registry init.
- Test 2 (ctest `desktop_first_frame_opens`): desktop UI under `SELECTA_SDL_TEST_READY` (I1164a hook,
  desktop_ui.c:801) writes `start_module_loads=N` to the trace at the first presented frame; assert N <= 3
  (desktop_ui, the output, one integration). Dummy video driver, no audio (null output).
- Revert proof: run both tests with `sel_manifest.dylib` removed from the module folder: the loader falls back to
  open-all, `module_loads` reads ~104 and both tests fail. Record the failing output in the row.
- Timing (manual row, not ctest, flaky on a busy machine): copy the app (new inodes, the 330-460 ms per-file case)
  and measure launch to first frame with `SELECTA_NULL_PACED=1` on a lib.db copy; target < 1 s.

## Rows (one sonnet agent each, in order)

- **M1 Resolve audit (no behaviour change).** List every read of a PTR tag from a registry descriptor in src/,
  apps/, plugins/ui/*; route each through `sel_plugin_resolve` (generalised to every kind) and presence checks
  through new `sel_hooks_has`; fix `sysfile_owner_name` to match the manifest. Mac resolver still passes through,
  so the full existing suite must stay green.
- **M2 Dump.** `selecta-cli plugins --manifest-c <out>` writing the generated C from tags.def types, plus
  `SEL_PLUGIN_HOOKS_PRESENT` in tags.def. Unit test: dump of 2 plugins compiles and round-trips their tags.
- **M3 Build.** CMake custom command + `sel_manifest` module; `AMIGA_MANIFEST` -> `MANIFEST` on both platforms;
  bundle in install-mac-app.sh; ctest `plugin_manifest_matches`. Amiga configure must still build (rule 4a).
- **M4 Lazy loader.** loader_dl.c: load manifest, mark names seen, dlopen resolver with ID/VERSION check and
  cached failure, `module_loads` count, fall back to open-all with one stderr line when no manifest is found.
- **M5 Pin tests + revert proof + timing.** Tests 1 and 2 above, revert proof output in the row, fresh-inode
  launch timing on the Mac; Johan check of a cold app launch (Mac).
- **M6 Content-only formats.** List replayers whose files are found by magic only (scan a corpus where names do
  not match, compare owners with and without manifest); give each a `probe.c` split (shared with Amiga I427e
  phase 2).

## Second cost: desk_open (SDL_Init + window + renderer, 452-3525 ms)

Not this design. It is the SDL/Metal first touch (or load from the concurrent build during the measurement), not
plugin related, and nothing above changes it. After M4 it is the largest start item and can alone break the 1 s
budget, so it gets its own row (proposed I1193c): measure SDL_Init, window, renderer separately on an idle
machine, cold and warm; candidates are showing the window before renderer creation finishes, and avoiding SDL
subsystems the desktop does not use at start (SDL_INIT_AUDIO/JOYSTICK).

## Decisions for Johan

1. **Which kinds go lazy.** (Recommended) all kinds, keeping D6 "everything a module"; the start set is a constant
   3-4 opens. Alternative: link UI, output and integration plugins into the app (Amiga style) for 1 open at start;
   saves ~1 s on the very first run after install, breaks D6.
2. **Probe behaviour.** (Recommended) a module loads only for files whose name it claims (the existing nuade
   rule), plus probe.c splits for magic-only formats (M6). Alternative: keep content-probing every replayer, which
   opens all modules on the first scan and keeps the cold cost there.
3. **User / $SELECTA_PLUGIN_PATH modules.** (Recommended) still opened at start like today: few, third party,
   not in the build manifest. Alternative: a per-user manifest cache keyed on size+mtime (more code, later row).
4. **Missing or stale manifest.** (Recommended) fall back to open-all with one stderr line, so tests, old installs
   and dev folders keep working. Alternative: refuse to start, which makes a broken install obvious.
5. **Unload.** (Recommended) modules stay loaded until exit on the Mac. Alternative: Amiga I1314b unload after a
   scan; risky with C++ statics and saves only memory the Mac has.
6. **desk_open cost.** (Recommended) separate row I1193c after M4. Alternative: fold into I1193b.

## Johan's answers (2026-11-13, AskUserQuestion)

1. All kinds lazy (D6 kept).
2. Probe by claimed name; magic-only replayers get probe.c splits (M6).
3. Missing or stale manifest: open all, one stderr warning.
4. desk_open cost: own row I1193c after M4.
Decisions 3 (user plugin folder opened at start) and 5 (no unload on Mac) taken as recommended defaults.
