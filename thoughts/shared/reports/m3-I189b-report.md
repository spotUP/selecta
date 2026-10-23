# M3 I189b: loudness headroom before the limiter

Design: architecture design section "6a. Loudness normalisation" (fixed-point gain, peak limiter in the mixer stage).

## Changes
- `src/engine/engine.c`: new `loud_reference()` picks a per-render-call reference gain (16.16) = the largest stored gain
  among the songs that can sound in the call (current and prepared slot), at least unity; if any such slot has no gain
  the reference is unity. `slot_gain()` now applies the slot gain relative to that reference (<= unity, so a slot or an
  equal-power crossfade sum can never pass 16 bits). `sel_engine_render()` sets `loud.gain_q16 = reference`, so the
  existing limiter (`sel_loudness_gain_process`, unchanged) multiplies the boost back in int64 and saturates only after
  its -1 dBFS ceiling. No second limiter, no float, no per-block allocation (one `uint32_t` in the engine struct).
- Deviation from the ledger wording: no int32 mix accumulator. The mix stage and DSP chain run in place on int16 between
  the mix and the limiter, so a wide accumulator cannot reach the limiter without changing both interfaces. Scaling the
  slots to the loudest one and applying that gain in the limiter keeps headroom through the same chain with the
  same cost. Side effect: with a boosted song the DSP chain sees the signal at its pre-boost level (linear stages are
  unaffected; the limiter restores the level).
- No-loudness path and any call whose reference is unity (attenuation, no boost): bit-identical to before
  (render-golden-host green; loudness-playback exact-sample checks unchanged).
- `tests/m3/loudness_playback.sh`: two new cases.
  - Boost: stored +6 dB on the test mod (plain peak 22857, boosted 45.7k), output must equal an awk model of the
    limiter on the unclipped plain * gain, exactly, limiter_hits > 0, no sample at 32767/-32768.
  - Crossfade gain at the switch (script with `OPTION crossfade`, song A -6 dB, song B -12 dB): before the overlap each
    sample is plain * gain A, after it plain * gain B (within 1 LSB for tie rounding), overlap length matches
    `xfade_frames`; with equal gains on both songs the whole run equals plain * gain within 2 LSB.

## Fail before
Old engine, new test (`build/host` built from main before the edit):
`boost mismatches=64135 first_frame=4872 rail=0` then `FAIL: boosted output is not the limiter applied to the unclipped plain * gain`.
(limiter_hits was > 0 on main too, because the limiter bounds the output; the clip shows in the waveform, hence the exact model.)
With the fix the hot case's hit count dropped from 66782 to 11336 (+24 dB) and the boost case passes exactly (491 hits).

## Tests (host, nice 19)
17 of 17 passed: loudness-playback, loudness-analysis, loudness-fetch, mixer-loudness-gain-unit, mixer-stage-unit,
mixer-led-stereo, stage-commands, render-golden-host (4 matched tests), crossfade, loop-fade, sleep-alarm.
Amiga 68020 build: rc 0, no errors.

## Open items
- The crossfade case does not fail on old main (it guards the shared `slot_gain` path; it was a coverage gap, not a bug).
- A reference gain is chosen per render call, so the DSP input level steps when a boosted song enters or leaves a call
  (only visible for nonlinear or stateful DSP plugins with a boosted song in a crossfade or gapless pair).
- Loudness-off mixing of a gain-less song next to a boosted one falls back to unity reference and 16-bit saturation of the
  boosted slot (rare: only when the preference changes mid-queue).
