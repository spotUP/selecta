---
row: I219
date: 2026-10-24
status: built, browse tests green, playback unproven (vamos has no CIA resource)
---

# I219: KPlayer v33 for ProTracker .mod on Amiga

## What was built

- `third_party/kplayer/`: kpl14.s, kpl_offsets.S, mucro.i and LICENSE, copied unmodified from HippoPlayer-68k
  `code/`, plus a README that names the origin.
- `plugins/replayers/kplayer/`: an Amiga-only replayer plugin (`kplayer`, kind `SEL_RP_KIND_HW`, extension `mod`,
  priority 10, MIN_CPU 68000).
  - CMake assembles kpl14.s with `vasmm68k_mot -m68000 -no-opt -Fbin`, the same flags as HippoPlayer's Makefile.
    The 6080-byte output is byte-identical to HippoPlayer's `kpl`.
  - `bin2c.cmake` embeds the binary as a writable, word-aligned C array (`sel_kpl_code`).
  - `asm-include/` provides `exec/exec_lib.i` and `resources/cia_lib.i`, which forward to the bebbo NDK `lvo/`
    files.
  - If vasm is missing, configure warns and skips the plugin. The repo builds without ~/Code/HippoPlayer-68k.
- `adapter.c`:
  - Probe checks M.K., M!K! or FLT4 at offset 1080.
  - Open copies the module and the 2000-byte work area into `SEL_MEM_SAMPLE`.
  - Start calls `kp_init` with CIA timing (d1=1) and tempo on.
  - The new `SEL_RP_HW_PAUSE` hook calls `kp_playstop`. `SEL_RP_HW_RELEASE` and close call `kp_end`.
  - A global owner pointer makes a stale release do nothing.
  - Calls go through GCC inline asm: register variables for d0-d2/a0/a1, jsr via a2, d3-d7/a3-a6 saved.
- New API:
  - Tags `SEL_RP_HW_PAUSE` (0x101D) and `SEL_RP_HW_RELEASE` (0x101E).
  - `SEL_ROUTE_HANDOVER` and `sel_engine_route(kind, voices, has_sink, voice_max)`.
  - `sel_engine_set_handover` and `sel_engine_handed_over`.
  - Stats `hw_handovers` and `hw_starts`.
- Engine (`src/engine/engine.c`):
  - The HW route table is at `sel_engine_route`, engine.c:4877. `sel_engine_voice_route` now wraps it.
  - The HW start branch is at engine.c:1230. It hands the output over before `rp_start(..., NULL)`. If the start
    fails, it gives the output back.
  - `hw_clock`, engine.c:4723, runs `sel_engine_render` by wall clock from `sel_engine_pump` while handed over. This
    keeps song time, the end policy and the queue moving with no output pulling.
  - Slots of the HW kind render silence and never tick.
- Shell (`apps/selecta/main.c`):
  - `shell_handover` closes the output on release and reopens it, with a new source, on return.
  - The stats line now prints `hw_handovers= hw_starts= replayer=`.

## Tests

- `paula-out-unit` (tests/paula/paula_out_test.c:72) has new rows:
  - HW kind gives HANDOVER, with and without a sink.
  - VOICE gives DIRECT.
  - RENDER gives MIXER.
  - An unknown kind gives MIXER.
- `amiga-browse-nav-vamos`, `amiga-library-browse-vamos` and `amiga-console-latin1-name-box-glyph-vamos` pass
  again: the Amiga registry now claims `.mod`.
- The `root_file` case in tests/m2/browse_nav.cmake now uses a `work:root.mod` fixture.
- Affected run: 22 of 22 passed. The run covered paula-out-unit, engine, gapless, selecta-* and browse tests,
  the three vamos browse tests, and paula voice/routing vamos.
- Host and Amiga builds have 0 warnings. `commands --unimplemented` is still 8.

## What vamos proved and what it did not

- A run of the Amiga `selecta` on a synthetic M.K. module under vamos printed `hw_handovers=1`. This proves:
  - The registry picked kplayer.
  - The route returned HANDOVER and the output closed.
  - `kp_init` executed, with CIA and custom chip register writes in the vamos log.
- The run also printed `hw_starts=0` and `start_failed`. `kp_init` returned -1, most likely because vamos has no
  `ciab.resource` for the CIA timer. Unverified: the vamos log shows no OpenResource trace.
- Not proven: audible playback, the CIA interrupt cadence, pause, and end of song. A rig run on a real Amiga is
  needed.

## Known gaps

- There is no song-end detection from KPlayer. `k_songover` is not read, so the default duration policy ends the
  song.
- HW songs cannot seek. skip_frames returns 0.
- A gapless prepare starts the next song early, and that start releases the playing HW slot. The HW song can
  therefore be cut short before a gapless swap. Not fixed.
- The `replayer=` stat stays `none` when the start fails.

## License

KPlayer is MIT, Copyright (c) 2017 Kari-Pekka Koljonen (HippoPlayer `code/LICENSE.md`, copied to
third_party/kplayer/LICENSE). It is compatible with Selecta.
