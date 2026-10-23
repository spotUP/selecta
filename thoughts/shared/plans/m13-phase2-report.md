# M1.3 Phase 2 report (I88, I89)

Red evidence: `52/60 Test #52: pattern-cells ....***Failed 0.26 sec`, `0% tests passed, 1 tests failed out of 1` (script printed "unknown directive @cells 0 0 1 4"). Red commit: 2bd9668.

Green: `ctest -R pattern-cells` 100% passed. Related tests (position, nuade, openmpt) pass. Ratchet `commands --unimplemented | wc -l` = 27. `cmake --build build/amiga -j4` green. No new engine objects, so `cmake/no_float.cmake` count unchanged.

## What was added
- tags.def: RP_TRACKER_INFO 0x1011, RP_TRACKER_CELLS 0x1012. replayer.h: sel_rp_tracker_info, sel_rp_cell and the two function typedefs.
- nuade adapter: hooks via the player's tracker_info / tracker_cells vtable entries. openmpt adapter: hooks via openmpt_module_get_pattern_row_channel_command.
- engine: sel_engine_tracker_info / sel_engine_tracker_cells (current slot). CLI: `@cells order row channels rows`.

## Deviations
- T41 uses `selecta-test.mod` for nuade (nuade cannot open the plain M.K. file) and `selecta-test-mk.mod` for openmpt; the plan said mk for both.
- openmpt note numbers use libopenmpt's MOD convention: period 428 reads note=61 (C-5), nuade reads 25 (C-2). The two replayers differ by 36 semitones for MOD. Open question: normalise in the openmpt adapter (needs a cached "is MOD" flag, since the metadata call allocates).
- openmpt effect numbers come from a table covering the ProTracker set; other effects read as 0.
- nuade: patterns=0 (player reports none); orders found by probing up to 4096.
- No audio-thread request path: the engine calls are valid only on the thread that owns the replayer (script driver). Phase 5 must add the request path.
- No nuade patch needed. third_party/nuade and libopenmpt were copied into the worktree, not committed.
- tags.def may conflict with Phase 1 additions at merge.
