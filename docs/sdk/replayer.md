# Writing a replayer

Header: include/selecta/replayer.h. Worked example: examples/plugin-sine/sine.c (a complete RENDER replayer, about 150 lines).

## Descriptor

`SEL_PLUGIN_KIND` is `SEL_PLUGIN_KIND_REPLAYER` (plugin.h). `SEL_PLUGIN_TAGS` lists the `SEL_RP_*` hooks from tags.def:
`SEL_RP_NAME`, `SEL_RP_VERSION`, `SEL_RP_KIND`, `SEL_RP_EXTENSIONS`, `SEL_RP_PROBE`, `SEL_RP_OPEN`, `SEL_RP_CLOSE`,
`SEL_RP_START`, and then `SEL_RP_TICK` or `SEL_RP_RENDER` by kind. Optional hooks (seek, position, metadata, tracker cells,
mute, speed) are declared only when implemented (replayer.h).

## Kinds (enum sel_rp_kind, replayer.h)

- `SEL_RP_KIND_VOICE`: the replayer owns no clock. The engine calls `RP_TICK` and the replayer writes the voice table
  (voices.h); the engine mixes.
- `SEL_RP_KIND_RENDER`: the replayer produces audio itself. `RP_START` receives `voices = NULL`. `RP_RENDER` writes
  interleaved stereo int16 at the rate given to `RP_START` and returns the frames written; fewer than asked means song end.
- `SEL_RP_KIND_HW`: the replayer drives sound hardware itself (replayer.h, HW kind notes).

## Lifecycle

1. `RP_PROBE(data, size, name)` returns a `SEL_PROBE_*` verdict. It must not allocate.
2. `RP_OPEN(src, host, info)` gets the whole song in memory; `src->data` stays valid until `RP_CLOSE` returns. Fill `info`
   (voice count, subsong count, default subsong, timebase). Return the state pointer or `NULL`.
3. `RP_START(state, subsong, rate, voices)` returns 1 on success.
4. `RP_TICK` or `RP_RENDER` run on the audio side. No allocation here: allocate in OPEN or START through `host->mem_alloc`.
5. `RP_CLOSE(state)` frees the state through `host->mem_free`.

## Rules

- No floating point in a path that runs under interrupt on 68k, no VLAs, no alloca (architecture design section 12).
- The module links nothing from Selecta. It uses `sel_host` for memory, logging and configuration.
- The registry hides a replayer the machine cannot run when it declares needs by tag (architecture design section 12).
