# M2 Phase 2 report (rows I112 to I117): Amiga host core/pal

Plan: thoughts/shared/plans/2026-10-24-m2-amiga.md, Phase 2; decisions D-M2-1 (priority 30), D-M2-3.

| Row | Commit | Test | Result |
|-----|--------|------|--------|
| I112 CIA-B tick interrupt | 2987e49 | T54 no-float-interrupt (core objects) | green (objects=2) |
| I113 audio task at prefs priority | 2987e49 | T54 same | green; sentinel audio_task_ticks= UNVERIFIED (rig) |
| I114 SPSC under Disable/Enable, main loop | c971ab2 | spsc-queue (desktop), spsc-queue-vamos | green (1000 pushed, 1000 popped, 2000 publishes) |
| I115 dir.c Examine/ExNext, library browse | 8ee0f79 | T57 amiga-library-browse-vamos | green |
| I116 one option parser | 9780bda, d8b689b | T58 amiga-tooltypes-cli-vamos (options) | green |
| I117 priority from option | d77d32d | T58 priority read-back | green (see deviation 5) |

Tests run (affected only): no-float*, spsc*, amiga-tooltypes-cli, amiga-library-browse. No full run.

## Deviations

1. nofloat.cmake want-list reduced to `audio_int audio_task`; Phases 3 and 4 must re-add `paula paula14 ahi` when those objects join target `selecta_audio_irq` (src/CMakeLists.txt). The object list is generated from the target (file(GENERATE)).
2. T57 desktop twin not registered: the desktop dir.c is not part of this row; only the vamos twin runs.
3. PRI, BUFFER, LED have no commands.def verbs, so they are new prefs rows (audio.priority default 30, output.buffer, output.led, output.id). OUTPUT is validated against commands.def (sel_cmd_by_name). Parser: src/engine/options.c.
4. `--dry-run` is handled as a first-argument command in apps/selecta-cli/main.c; `--tooltypes <file>` reads text lines. The icon.library GetDiskObject ToolTypes reader is NOT written; the .info wrapper is open.
5. vamos stubs SetTaskPri (returns 0, changes nothing). src/core/amiga/pal.c stores ln_Pri under Forbid when the read-back differs. A real exec never takes that branch. This is an emulator compensation; remove it if the rig is the only gate you trust.
6. I112 and I113 share one commit (one CMake object library covers both files). Commit 2987e49 alone does not build (it lists core/amiga/loop.c, added in c971ab2).
7. Commit d8b689b: no-float-engine counted 27 objects; options.c made it 28 (14 engine). Count read from the target's object list.

## Open questions and unverified

- Rig sentinels `cia_ints=` and `audio_task_ticks=` UNVERIFIED: the selecta app is not built for Amiga and there is no shell that prints sel_audio_int_count / sel_audio_task_ticks. audio_int.c and audio_task.c only compile-check; never executed (CIA-B vector, timer A/B choice, AddTask stack, Signal handshake untested; vamos does not emulate CIA interrupts).
- loop.c (sel_loop_turn) and the audio task are not wired into any app.
- Flake: first vamos run after a rebuild, or amiga-library-browse alongside another vamos test under -j2, can fail with "path setup failed". A rerun alone passes. Probably the shared vamos fixture; not investigated.
