## Implementation M2 (plan: thoughts/shared/plans/2026-10-24-m2-amiga.md, status: draft, starts after I104)

Tests T49 to T61 are defined in the plan's finish-line table.

| ID | Status | Phase | Task | Proof |
|----|--------|-------|------|-------|
| I105 | [ ] | 0 | Add T49 to T55, T57, T58, T61 red (desktop and vamos) with fixtures | ctest lists them, all fail |
| I106 | [ ] | 0 | Add rig driver `tests/amiga/run_rig.cmake`, label `rig`, skip line; T56, T59, T60 red | ctest -L rig fails with rig up, skips visibly with rig down |
| I107 | [ ] | 1 | Declared-need tags in `tags.def` (MIN_CPU, NEEDS_FLOAT, MEMORY, PRIORITY, CONFIG, OUT_MEM_CHIP) | build both trees; tag table test |
| I108 | [ ] | 1 | Registry machine filter and priority owner choice, fake `sel_machine` injection | T49 green desktop |
| I109 | [ ] | 1 | GFX and AUDIO probes, planar tier, `--tier` override | T50 green |
| I110 | [ ] | 1 | timer.device ReadEClock clock, DateStamp fallback under vamos | clock test green on rig and vamos |
| I111 | [ ] | 1 | T49 green under vamos; tag every existing plugin with its needs | T49 green both trees |
| I112 | [ ] | 2 | CIA-B interrupt server (signal only) | T54 core objects green; rig sentinel `cia_ints=` |
| I113 | [ ] | 2 | Audio task at prefs priority, render loop | rig sentinel `audio_task_ticks=` |
| I114 | [ ] | 2 | SPSC publication with Disable/Enable around head/tail; main loop Wait plus `sel_engine_pump()` | queue test under vamos |
| I115 | [ ] | 2 | `dir.c` Examine/ExNext | T57 green |
| I116 | [ ] | 2 | One option parser for ToolTypes and CLI over `commands.def` | T58 green (options) |
| I117 | [ ] | 2 | Task priority from option | T58 green (priority read back) |
| I118 | [ ] | 3 | Paula voice sink, chip RAM samples via `host->mem_alloc` | build; voice API conformance test |
| I119 | [ ] | 3 | Period once per note, volume 0..64, loop registers | T51 register hash |
| I120 | [ ] | 3 | Shadow register twin for vamos and desktop | T51 green both trees |
| I121 | [ ] | 3 | Routing: VOICE with voice_count <= 4 to `paula` | routing table test; T56 precondition |
| I122 | [ ] | 4 | Portable paula14 split tables, default calibration (D-M2-4) | T52 green desktop |
| I123 | [ ] | 4 | paula14 Amiga sink, chip RAM double buffer | T52 green vamos; T54 extended |
| I124 | [ ] | 4 | AHI sink over ahi.device | rig sentinel `ahi_bufs=` |
| I125 | [ ] | 4 | Buffer length per CPU class in `sel_prefs`, AHI mode and rate defaults (D-M2-5) | T59 green |
| I126 | [ ] | 4 | Routing: RENDER and VOICE over 4 voices to mixer then paula14 or ahi | routing table test |
| I127 | [ ] | 5 | Integer LED filter stage | T53 LED part green |
| I128 | [ ] | 5 | Integer stereo width stage | T53 width part green |
| I129 | [ ] | 5 | Commands, prefs keys, text UI controls for LED and width | T53 green both trees; T8 count moves |
| I130 | [ ] | 6 | `plugins/ui/text/amiga/term.c` raw mode, WaitForChar | T55 partial |
| I131 | [ ] | 6 | `plugins/ui/text/amiga/spawn.c` | spawn test under vamos |
| I132 | [ ] | 6 | Build `plugins/ui/text` and `apps/selecta` on Amiga | T55 green |
| I133 | [ ] | 6 | Rig run in UP-Term | T56 green |
| I134 | [ ] | 7 | `SEL_RP_CONFIG` tag list, prefs, per-song override (D-M2-8) | T61 green |
| I135 | [ ] | 7 | T60 batch on `--a1200 --nofpu`, verdicts recorded, NEEDS_FLOAT tags set | T60 green |
| I136 | [ ] | 8 | Fix T8 ratchet number for M2 command set | T8 green |
| I137 | [ ] | 8 | Full finish-line run (reason: landing M2) | all T49 to T61 and T8 green, none skipped |
| I138 | [ ] | 8 | Manual checks 1 to 5 confirmed by Johan | Johan's confirmation recorded in ledger |
