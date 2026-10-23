# M1.3 Phase 5a report: sample export (I97, I98)

## Blocked on me

- Nuade scan is untested: no test drives RP_SAMPLE_GET on a nuade-played module (first-use order differs from module slot order for selecta-test.mod: 1,2,4,3), so T46 asserts only the openmpt path. Decide whether T46 needs a nuade row and what slot numbering nuade should use.
- Question: sample_table.cpp copies the layout of `struct openmpt_module` from libopenmpt 0.8.4 and reaches `m_sndFile` through a derived class. A libopenmpt upgrade needs a re-check. Accept, or ask for a libopenmpt patch instead (plan forbade it)?

## Changed

- Red line (cb2b65c): `Test #60: sample-export ....................***Failed    0.22 sec`
- 5b93144 (I97): hook `RP_SAMPLE_GET` (0x1015), `sel_rp_sample`, openmpt adapter (sample_table.cpp), nuade adapter scan catalogue (second player instance, recording voice backend, stops at end/loop/10 minutes, names "sample NN").
- cb95a44 (I98): verbs `SAMPLE_LIST`, `SAMPLE_EXPORT "us"`, event `SAMPLE`, writers src/library/export/{wav.c,svx.c}, goldens in tests/golden/samples/. T46 green. `commands --unimplemented` stays 23; ratchet file unchanged. Host and amiga builds green; ratchet, nuade, openmpt, no-float, no-alloc, protocol, registry tests pass.

## Found

- Deviation: verbs reply with events, there is no OK line; SAMPLE_LIST answers one `SAMPLE n length loop_start loop_length rate "name"` event per non-empty slot.
- Deviation: `sel_rp_sample.name` is a `const char*` owned by the replayer, because events queue the pointer (a stack buffer printed empty).
- Deviation: T46 writes to relative paths in the songs directory instead of tmp paths.
- Files are written with fopen, as PLAYLIST_SAVE does; no separate host file API exists.
- Hook runs on the command thread. Both new verbs sit in group "playlists" in commands.def (no better fit).
- 16-bit data: WAV writes 16-bit PCM; 8SVX keeps the high byte only.
