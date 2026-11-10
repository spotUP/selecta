---
date: 2026-11-10
row: I427e (fixes I1305a)
status: phase 1 (loader + bbmp_ptk); phase 2 converts the other families
supersedes: architecture-design section 10 line 471 ("static registry ... default for 68000 builds") and the I427f clause "Amiga and web builds stay static", for the Amiga only. Web is out of scope.
---

# Amiga players as LoadSeg plugins, loaded on demand

Johan, 2026-11-10: "players should never be linked in they are plugins loaded on demand", "its been in the plans from
the very start all players are plugins loaded only when needed", "no wonder the amiga app takes ages to load fix it".

Measured cause (I1305a): every Amiga executable linked every replayer. ProTrekkr's static buffers alone put 22 MB of
the 24 MB BSS into selecta-cli, so vamos at `-m 8192` fails with `NO MEMORY for 16fc1a4 bytes` and the real machine
clears 24 MB at every start.

## Split per plugin

- **Manifest** (registry side, linked into the executable): one small C unit compiled with `SEL_PLUGIN_MANIFEST`.
  Its `sel_plugin_entry_<name>` returns the normal descriptor (KIND, ID, VERSION, TAGS) plus `SEL_PLUGIN_MODULE`
  (new tag 0x0104, the module file name). TAGS carries only the static facts: NAME, VERSION, KIND, EXTENSIONS,
  PROBE, PRIORITY, NEEDS_FLOAT / CPU tags. Probing, extension lists and CPU/FPU gating therefore never load a module.
  The probe code is shared with the module (one source file in both, e.g. `bbmp_ptk/probe.c`): no duplicate.
- **Module** (`Players/sel_<name>`, a plain AmigaOS hunk executable): the full plugin, its engine and its libraries,
  linked with `-nostartfiles` and `src/engine/seg_crt.c` FIRST.

## Hunk header (seg_crt.c, offset 0 of the first code hunk; host reads it at `BADDR(seg) + 4`)

| offset | content |
|---|---|
| +0 | `moveq #-1,d0; rts` (run from a shell it exits with failure) |
| +4 | `'SELP'` magic |
| +8 | header version 1 |
| +12 | `int init(void)`: sets SysBase, runs libnix INIT_LIST (libraries, C++ constructors); 0 if exit() was called |
| +16 | `void fini(void)`: runs EXIT_LIST |
| +20 | `sel_plugin_entry` |

ncrt0.o is not reused: it waits for a Workbench message when pr_CLI is 0 and calls main(). callfuncs is libnix's own
priority walk, copied.

## Host table

Nothing new: the module's `sel_plugin_entry(1)` returns the descriptor, and the engine passes the existing
`sel_host*` to RP_OPEN as on every platform. The module links no selecta_core of its own by rule (D6); phase 1's
bbmp_ptk still links selecta_archive (miniz) privately.

## When LoadSeg runs

`sel_plugin_resolve(hooks)` (src/core/plugin_resolve.c) is called right before a replayer is opened: in the engine
(before the RP_KIND lookup) and in the library scan (local.c, before open/metadata). On the Amiga the resolver is
`loader_seg.c`: the first call for a manifest loads the module, checks magic + version, runs init, calls the entry,
checks the real ID equals the manifest ID and caches the real hooks. A failed load is cached and never retried; it is
recorded with `sel_registry_note_rejected`. `sel_registry_stats.module_loads` counts loads (printed by
`selecta-cli plugins` and on stderr by `render`). Other platforms install no resolver: hooks pass through.

## Search path

`$SELECTA_PLUGIN_PATH` (one directory) first, then `PROGDIR:Players/`. LoadSeg runs inside
`sel_amiga_dos_quiet_begin/end` so a missing assign opens no requester.

## UnLoadSeg policy

Modules stay loaded until process exit (atexit: fini, then UnLoadSeg). Reason: hook pointers and song state may live in
the module; unloading on song change needs a refcount in the engine (phase 2 option, not needed for start time).

## Known gaps for phase 2

- `sysfile_owner_name` (engine.c ~2749) matches desc TAGS == hooks; it fails for resolved module hooks. Match the
  manifest instead.
- A module that calls core helpers (sel_tag_find, ...) must link them itself or get them through sel_host.
- exit() inside a hook after init is not safe (only init/fini unwind).
