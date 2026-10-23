# M1.3 Phase 3b report (I92: pitch and tempo)

## Changed
- Red line before the change: `Test #57: voice-control ....***Failed 0.53 sec` (at `pitch_factor_q16=131072`).
- `86b9689`: tools/gen_pitch.c (host tool, float allowed) writes src/mixer pitch_table.h at build time (2401 Q16 entries), wired in src/CMakeLists.txt like gen_viz_table.
- `51a8b7a`: PITCH (-1200..1200, `ERROR range PITCH`) and TEMPO (-50..100, `ERROR range TEMPO`) verbs. VOICE kind: mixer resample step scaled by the Q16 factor (`sel_mixer.pitch_q16`), TEMPO scales the scheduler tick length. RENDER kind: optional hook RP_SET_SPEED 0x1014 (pitch_q16, tempo_q16); openmpt adapter uses libopenmpt_ext set_pitch_factor / set_tempo_factor. render.c prints the engine value.
- Ratchet 25 -> 23; tests/ratchet/unimplemented.txt updated.
- After: voice-control, ratchet, golden, nuade, openmpt, viz: 16 of 16 pass. Host and amiga builds green.

## Found / deviations
- Both verbs accept relative arguments like VOLUME.
- Pitch/tempo state lives in sel_state (pitch_cents, tempo_percent, pitch_q16); no event is emitted on change.
- TEMPO on VOICE kind rounds the tick length down (min 1 frame); the T42 check compares positions, not audio.
- Not audibly verified: pitch/tempo sound by ear.
