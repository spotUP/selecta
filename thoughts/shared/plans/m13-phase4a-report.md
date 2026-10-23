# M1.3 Phase 4a report (I93, I94)

Design: architecture-design sections 3 and 4. Plan: 2026-10-23-m1-3-viz-voices-playback.md.

## Commits
- faae4ad  Red tests T43, T44, T45 (I93)
- 108bbed  REPEAT, loops, fade_ms, max_song_ms, silence_ms, SONG_ENDED reason (I94)

## Red lines (before product code)
- Test #58: loop-fade ........................***Failed    0.41 sec  (SONG_ENDED had no reason)
- Test #59: crossfade ........................***Failed    0.44 sec  (unknown directive @stats)
- Test #60: sleep-alarm ......................***Failed    0.16 sec  (ERROR unknown_command ALARM, unknown directive @clock)

After I94: loop-fade passes; crossfade fails on `unknown directive @stats`; sleep-alarm fails on the unknown ALARM/@clock. Both stay red for I95/I96.
Affected suites run green (35 tests: queue, ratchet, abi, m0-script, integration, no-alloc, voice, viz, codec, seek, nuade, openmpt, render).

## Ratchet
`selecta-cli commands --unimplemented | wc -l`: 23 -> 22 (REPEAT). tests/ratchet/unimplemented.txt updated.

## Deviations
- No duration used (Q20): the replayer LOOPED and ENDED tick flags and the libopenmpt short read drive the end.
- nuade Protracker never reports ENDED, only LOOPED. T43 loop_end therefore uses selecta-test-mk.mod (libopenmpt, repeat count 0 -> short read -> reason end). short.mod now ends with reason loop; tests/m1/queue.expected updated.
- The CLI prints SONG_ENDED through the codec, so the line is `SONG_ENDED <reason>` (no `reason=` key); T43 expectations changed to match.
- Reason `stopped` is not emitted (STOP emits no SONG_ENDED today).
- REPEAT one ignores `loops`; it restarts a song that ends with reason end without emitting SONG_ENDED. max_length and silence still end the song under REPEAT one (T43 uses it as the never-looping song).
- stop-after-current is one-shot (reset to off after it fires); stop-after-queue is persistent.
- Fade is linear Q16 gain applied in 64-frame steps; `loops` counts LOOPED ticks and the fade starts on the tick that reaches the count. New accessor sel_engine_fade and CLI directive @fade_mid.
- `SEL_OUT_VOICES` / `voices-test` output does not exist in the tree; crossfade_unsupported.sel only issues `OUTPUT voices-test` (belongs to I95).
- T45 uses `script sleep.sel --clock fake=07:29:59` (my syntax; the plan says `--clock fake`). Volume ramp and restore assertions left to I96.
- gen_testmod usage string lacks `[--silent]` (minor).

## Questions for the user
None.
