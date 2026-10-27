# I532 silent playback on the Amiga

Cause: the default output on the Amiga is `paula` (plugins/outputs/paula/paula_out.c), a voice-only output
(SEL_OUT_VOICES). Its `out_pump` pulls the engine's PCM and discards it. The shell (apps/selecta/main.c) never calls
`sel_engine_set_voice_sink`, so every song routes to the mixer and plays into that discard. `sel_registry_default_output`
(src/engine/registry.c:173) picks the first entry of SEL_MACHINE_AUDIO ("paula" or "paula,ahi").

Fix (engine level, reusing the registry and output hooks):
- src/engine/engine.c: `pcm_output_for`, `switch_output`, `follow_route`; `start_subsong` calls `follow_route` after
  `handover_for`. A mixer-routed song on a voice-only output asks the app for a PCM sink: `ahi` when the machine
  AUDIO list names it, else `paula14`. A song routed DIRECT swaps back to the saved voice output.
- include/selecta/engine.h: `sel_engine_set_output_switch`.
- apps/selecta/main.c: `shell_switch_output` closes the open output, opens the new one at its own rate, sets the
  source; the loop and exit paths use `so.pump` / `so.close`.

Tests (tests/engine/pcm_route_test.c, wired in tests/CMakeLists.txt):
- engine-pcm-song-leaves-voice-only-output (AUDIO paula,ahi -> ahi)
- engine-pcm-song-falls-back-to-paula14 (AUDIO paula -> paula14)
- engine-voice-song-stays-on-paula (voice song with a sink: routed direct, no switch)

Revert-check: with the `follow_route` call disabled the first two fail:
`FAIL line 214: g_switches == 1u`, `FAIL line 219: g_switches == 1u` (plus the id and state.output checks);
restored: 3 of 3 pass. `selecta` host target builds.

Not verified: Amiga rig (none used). Also seen, out of scope: `selecta-cli play --output paula` froze rig 2;
`ram:aga.log` ends with an `amiga_start` stack overflow. True Paula-direct playback in the app shell stays unwired
(no voice sink); only selecta-cli wires it.
