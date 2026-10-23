# M2 Phase 3b report (I118 remainder, I121)

## Done
- I118: SEL_MEM_SAMPLE maps to AllocVec(MEMF_CHIP) on Amiga (src/core/amiga/mem.c via src/host/mem_class.c); desktop unchanged.
- I118: hardware output `paula` (plugins/outputs/paula/paula.c, paula_hw.c). Shares paula_out.c skeleton and paula_voices.c sink with `paula-shadow`; only the bus differs. Raises SEL_OUT_PAULA_HW, which sel_engine_set_output turns into sel_engine_set_paula_output(e, 1).
- I121: sel_engine_voice_route(voice_count, has_sink, voice_max); VOICE replayer with <= 4 voices goes direct, else mixer.

## Tests
- mem-class-unit, paula-out-unit (shared hook pointers identical, hw register writes, routing table), paula-voices-unit, paula-voice-registers (desktop golden hash unchanged), no-float-interrupt (now includes paula_hw): pass.
- Host and Amiga builds green.
- Red, not from this change: paula14-tables(-vamos) (Phase 4), amiga-text-ui-vamos (T55), amiga-roundtrip (stale vamos ram volume), paula-voice-registers-vamos (open_failed on a host absolute song path in the worktree; same as the previous report).

## Deviations
- Plugins cannot reach the engine, so the set_paula_output call is done through new tags SEL_OUT_VOICE_MAX (0x2013) and SEL_OUT_PAULA_HW (0x2014).
- Non-sample classes on Amiga now use AllocVec/FreeVec (MEMF_PUBLIC) instead of libnix malloc.

## Open
- No vamos run of AllocVec; chip proof is the counter test plus the Amiga build.
- Rig down: hardware plugin does not AllocAudio or take over the system audio interrupt; real-hardware ownership unverified.
- A >4-voice song on `paula` goes to the mixer but has no PCM output stage until Phase 4 (paula14/AHI).
- Amiga plugin-count expectations not checked for the new `paula` plugin.
