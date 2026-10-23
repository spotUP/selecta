# M1.3 Phase 3a report (I90, I91)

## Blocked on me
- The mute golden `tests/golden/nuade/selecta-test-mute1.sha256` is the hash of a render of the zeroed-sample module (`gen_testmod --zero-sample 1`). No human listened to it. Listen to a muted-voice render once.

## Changed
- I90 `ab8699d`: test T42 `voice-control` (red). Red line: `Test #57: voice-control ....***Failed 0.05 sec` (cause: unknown option `--script`).
- I91 `f74a1b1`: MUTE and SOLO verbs, 32-bit mute mask in `sel_state.mute_mask`, VOICE event "ut" carries the mask, mixer skips muted voices (clock keeps running), `RP_MUTE` 0x1013, openmpt adapter via libopenmpt_ext `set_channel_mute_status`, `render --script`.
- Ratchet: `selecta-cli commands --unimplemented | wc -l` is 25 (was 27); `tests/ratchet/unimplemented.txt` set to 25.
- Host and amiga builds green. After I91: ratchet, viz, golden, cells, nuade, openmpt tests pass. `voice-control` passes mute, solo and toggle for nuade and openmpt, and fails at `pitch_factor_q16=131072` (I92 half, as intended).

## Found
- Deviation: T42 expected SOLO 2 = 0x0b, which is SOLO 3. Corrected to 0x0d in the I91 commit. SOLO limits the mask to the song's channel count (tracker geometry, since openmpt reports 0 voices).
- `render --script` did not exist; added. In script mode ERROR events print to stdout and do not fail the render.
- `pitch_factor_q16=65536` is a hard-coded placeholder in `apps/selecta-cli/render.c`; I92 must replace it with the engine value. `end_position` for nuade reads row 0 (nuade reports no position); the tempo check compares equal values.
- No Paula-direct backend with a volume path exists in code (only the mixer); nothing to do there.
- openmpt mute is tested as "differs from unmuted", not against a golden.
