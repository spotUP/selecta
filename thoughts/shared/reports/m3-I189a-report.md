# I189a: per-slot loudness gain

Design: thoughts/shared/plans/2026-10-22-architecture-design.md, section 6a "Loudness normalisation".

## Defect
I189 kept one per-engine struct `e->loud`, set by `loudness_begin()` (via `hist_begin()`, command thread) when a song
switch finished. The incoming song played its first frames at the outgoing song's gain, and the audio thread read a
struct the command thread wrote without ordering.

## Changes
- `src/engine/engine.c`
  - `sel_slot` carries `loud_active`, `loud_gain_q16`, `loud_source`. `loudness_slot()` fills them in `prepare_slot()`
    before `busy = 1`, so the swap ring publishes them with the slot. One helper `song_key()` builds the library key
    ("local:" prefix) and is shared by the loudness lookup and ANALYSE.
  - `slot_gain()` applies each slot's gain to that slot's samples right after `render_slot`, before the crossfade
    mix (outgoing, incoming and plain path). Integer math: `(s*g+32768)>>16`, int16 saturation.
  - The -1 dBFS limiter runs once on the mixed output in `sel_engine_render` (unity gain, pure limiter), active when
    any mixed slot carried a stored gain. `limiter_hits`, `loudness_frames`, `loudness_source`, `gain_q16` keep their
    meaning; source and gain come from the current slot.
  - `hist_begin` no longer touches loudness.
- `apps/selecta-cli/main.c`: script directive `@dump <file>|off` writes every rendered block as raw s16 stereo.
- `tests/m3/loudness_playback.sh`: gapless two-song case, songs seeded with -6 dB and -12 dB; every sample of A and B,
  B's first frames included, must equal plain * own gain. Rendered in 8192-frame blocks because the next song is
  prepared in the pump between renders and B's first non-silent frame is 4450.

## Fail before (old engine.c, new test)
    switch mismatches=1204 first_sample=97101
    FAIL: song B is not scaled by its own gain from its first frame, or song A by its own
(ctest rc 8). With the fix: Passed.

## Tests (host build/host, nice 19)
Passed 11 of 11: loudness-playback, mixer-loudness-gain-unit, mixer-stage-unit, stage-commands, render-golden-host,
crossfade, loop-fade, sleep-alarm and the others matched by the regex. Amiga 68020 build: rc 0.

## Open items
- Per-slot gain saturates at int16 before the limiter, so a boost past full scale clips before limiting.
- The limiter runs only when a mixed slot carried a stored gain.
- A loudness pref change mid-song applies at the next `prepare_slot`.
- Test covers the gapless switch; the crossfade path shares `slot_gain` but has no own gain-at-switch test.
