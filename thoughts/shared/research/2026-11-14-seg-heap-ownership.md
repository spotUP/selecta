---
date: 2026-11-14
row: I1378g
design: thoughts/shared/research/2026-11-13-amiga-plugins-on-demand.md (Design step 1)
status: fix on branch amiga-i1378g
---

# Amiga LoadSeg modules: heap ownership across the import table (I1378g)

## Question

Each LoadSeg module (seg_crt.c startup, -nostartfiles) links its own libnix: malloc pool (`___memorylist` in
libnix `malloc.o`, freed by `___exitmalloc` in the module's EXIT_LIST, i.e. at `fini`), stdio (`__sF`), errno.
UI modules call core through import thunks (I1378a). Does heap memory, or a FILE*, cross that boundary?

## libnix facts (m68k-amigaos-nm on ~/opt/amiga/m68k-amigaos/libnix/lib/libm020/libnix.a)

- `malloc.o` defines `_malloc`, `_free`, `___initmalloc`, `___exitmalloc`, `___memorylist`, `___memsema`. No other
  member references `___memorylist`, `___memsema`, `___initmalloc` or `___exitmalloc`.
- `realloc.o`, `calloc.o`, `strdup.o`, `strndup.o`, `asprintf.o` and the stdio/IO members (`fopen.o`, `setvbuf.o`,
  `tmpfile.o`, `open.o`, `dup2.o`, `lseek.o`, `atexit.o`, `__amigapath.o`, `dirent_scandir.o`, `concat.o`,
  `wcstol.o`) only reference `_malloc` / `_free` (realloc also reads the libnix block header at -4).
- So replacing `_malloc`, `_free`, `_realloc` replaces the whole pool; libstdc++ `operator new` also ends in `_malloc`.

## Imports: ownership crossing

Union of both blocks (amiga-i1378c desktop_ui list, amiga-i1378b text_ui list). Prototypes in include/selecta/*.h;
first call site in the module. "caller buf" = module passes its own buffer, core copies into it; "core-owned" = core
returns a pointer into its own static/engine memory the module only reads.

| Symbol | Prototype | Module call site | Crossing |
|---|---|---|---|
| sel_tag_find | tags.h:44 | desktop_ui.c:1481, text_ui.c:770 | none (reads module tag list during call) |
| sel_tag_name | tags.h:50 | desktop_ui.c:6937, text_ui.c:1774 | core-owned const string |
| sel_tag_type_of | tags.h:51 | desktop_ui.c:6940, text_ui.c:1779 | none (out struct) |
| sel_app_plugin_dir | engine.h:375 | uimodel/about.c:88 | caller buf |
| sel_config_dir | engine.h:372 | text_ui.c:2587 | caller buf |
| sel_cmd_area / _args / _name | protocol.h:61-63 | uimodel/palette.c:106,114,189 | core-owned const string |
| sel_cmd_by_name | protocol.h:66 | uimodel/palette.c:186 | none |
| sel_engine_lib_deferred | engine.h:740 | desktop_ui.c:8561 | none |
| sel_engine_lib_nowait | engine.h:737 | desktop_ui.c:4995 | none |
| sel_engine_lib_site | engine.h:739 | desktop_ui.c:4996-4998 | core KEEPS the module's tag pointer (string literal in the module's data hunk) until the next call; the module restores the previous tag at :4998, so no pointer survives the call. Static data, not heap: a module unloaded between the two calls would leave a dangling pointer. |
| sel_engine_library_items | engine.h:692 | uimodel/libfilter.c:43, text_ui.c:955 | caller buf (fixed-size struct, char arrays, no pointers) |
| sel_engine_library_items_post/_busy/_take | engine.h:699-701 | desktop_ui.c:12090-12112 | caller buf; the job buffer is core heap, core frees it in _take |
| sel_engine_pattern_request | engine.h:775 | desktop_ui.c:7694, text_ui.c:2182 | none |
| sel_engine_plugin_host | engine.h:442 | desktop_ui.c:754, text_ui.c:454 | core-owned sel_host table; its mem_alloc/mem_free are the host allocator (text_ui.c:244,695,2563-2569 already free host memory through it) |
| sel_engine_queue_groups | engine.h:717 | desktop_ui.c:4649 | core-owned arrays ("belong to the engine until the next call"), module only reads |
| sel_engine_reads_pending / sources_pending | engine.h:421,424 | desktop_ui.c:10298, 13520 | none |
| sel_engine_sample_get | engine.h:773 | desktop_ui.c:7588 | out struct |
| sel_engine_scan_info / scan_progress | engine.h:326,340 | desktop_ui.c:8081, 15629 | out struct |
| sel_engine_send_line | engine.h:414 | desktop_ui.c:13804, text_ui.c:147 | module stack pool; the engine copies string args on send (I1082b, comment at both sites), no pointer kept |
| sel_engine_song_briefs_post/_busy/_take | engine.h:732-734 | desktop_ui.c:5012-5040 | post copies keys into core; take copies into caller buf |
| sel_engine_song_text / _generation | engine.h:718-719 | desktop_ui.c:3292, 4626 | caller buf |
| sel_engine_state | engine.h:429 | desktop_ui.c:1151, text_ui.c | core-owned state, read only |
| sel_engine_stats_get | engine.h:658 | desktop_ui.c:4712 | out struct |
| sel_engine_sync_cancel / sync_progress | engine.h:350-351 | desktop_ui.c:15074, 14001 | none / out struct |
| sel_engine_tracker_info | engine.h:760 | desktop_ui.c:7119 | out struct |
| sel_engine_view_cursor_key | engine.h:669 | text_ui.c:265 | core-owned const string |
| sel_engine_view_row_path | engine.h:671 | desktop_ui.c:8863 | caller buf |
| sel_format_label | format_label.h:15 | desktop_ui.c:1904, text_ui.c:1833 | caller buf |
| sel_plugin_resolve | registry.h:92 | desktop_ui.c:1481 | core-owned const tag list |
| sel_uim_lfilter_match / lsort_item_cmp | uimodel/browse.h:55-57 | desktop_ui.c:5660, 12224 | none |
| sel_view_style_label | engine.h:663 | desktop_ui.c:7959 | core-owned const string |

No import returns heap the module frees, takes ownership of module heap, or passes a FILE*. Nothing crosses today;
the risk is latent: the next import that returns a malloc'd block (or a core function that frees a caller block), any
C++ object or `strdup` handed to core, or a module allocation core keeps past a UI switch would use the wrong pool,
and the module's fini (EXIT_LIST `___exitmalloc`) would free memory core still points at.

errno: core functions set core's errno; no module code reads errno after an import call (not exhaustively checked
beyond the call sites above, unverified). stdio: each module's `__sF` is its own and no FILE* crosses.

## Fix (ABI, once)

`malloc`, `free`, `realloc` are appended to cmake/amiga_seg_imports.txt, so every module built with imports gets
thunks to the host's libnix allocator. `sel_add_seg ... IMPORTS` links with `-Wl,-u,_malloc -Wl,-u,_free
-Wl,-u,_realloc`: the symbols are undefined from the first input, so the thunk archive (linked after the module's
libraries, before the driver's -lnix) supplies them even when only libnix members or libstdc++ call malloc.
libnix `malloc.o` is then never linked into a module: no private pool, no `___exitmalloc` in fini. If something still
pulled malloc.o in, its `_malloc` would clash with the thunk and fail the link. calloc/strdup/stdio buffers/new all
end in the host pool; libnix's `___memsema` in the host serialises tasks.

The generated host table names each symbol through an asm-label alias (`extern char sel_seg_imp_N
__asm__("_malloc")`) so C library names never clash with GCC's built-in declarations.

Scope: modules built with IMPORTS (UI modules, the seg-imports test variant). Replayer modules (no IMPORTS, count 0)
keep their own pool: their allocations stay local by design, and the pool freed at fini is their leak net. Giving
them imports would make every replayer unloadable on a host older than the new list (SELP v2 count rule).

Trade-off: a UI module's leaks now stay in the host pool until the process exits instead of being reclaimed at fini.

## Other findings (not heap, same class)

- amiga-i1378b and amiga-i1378c each appended different symbols to the "append only" list from the same base
  (slot 1 is sel_config_dir in b and sel_app_plugin_dir in c, ...). Whichever merges second must re-append its own
  symbols after the other's, or modules from one branch call the wrong core function.
- The desktop seg links its own copies of selecta_platform, selecta_archive, selecta_uimodel, selecta_gui
  (plugins/ui/desktop/CMakeLists.txt): their globals are separate from core's copies.
