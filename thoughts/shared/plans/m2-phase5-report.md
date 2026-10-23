# M2 Phase 5 report (I127, I128; I129 not done)

Commit: one combined commit for I127 and I128 (the two stages share stage.c/h, the engine wiring, the render counters and T53; they could not be split cleanly).

## I127 LED filter
- src/mixer/stage.c/.h: 2-pole Butterworth low-pass, bilinear transform designed at init in Q28 integer maths (Taylor tan), so any render rate has its -3 dB point at 3275 Hz. Direct form I, int64 accumulate, no float. Counter `led_frames=`.
- Engine owns the stage, applied in sel_engine_render after volume/timer, before the DSP chain. API: sel_engine_set_stage(led, width), sel_engine_set_paula_output(on). The Paula output sets the latter to 1 later: both stages are then bypassed (flag `paula_output`, no Paula output exists yet).
- render flags `--led on|off`, `--stereo-width N`; render prints `led=.. led_frames= led_cutoff_hz=` and `width= width_frames= width_mono=` when either flag is given. `led_cutoff_hz` is the designed constant, not a measurement.
- -3 dB measured in tests/mixer/stage_test.c (test mixer-stage-unit) on a generated 5 Hz-step sine sweep: 3275 Hz at 44100 and 48000, 3280 Hz at 22050 (limit 3111..3439).
- Golden: tests/golden/m2/render-led.sha256 (selecta-test.mod, 2 s, --led on --stereo-width 50).

## I128 stereo width
- M/S in integer: side scaled by width/100; width 100 is a bypass (hash equals plain, counter stays 0), width 0 gives L == R. Counter `width_frames=`.

## Tests
mixer-led-stereo (desktop), mixer-led-stereo-vamos, mixer-stage-unit, mixer-unit, no-float-engine: all pass. Not run: the full suite.
no-float-engine expected object count 28 -> 29 (stage.c is a new mixer object; cmake/no_float.cmake updated).

## Deviations from the plan / T53 (tests/m2/mixer.cmake)
1. selecta-test.mod renders mono (L == R in every frame: the protracker4 replayer sets no pan), so width 0 and 50 cannot move its hash. The "each knob moves the hash" check for width now runs on the stereo selecta-test-mk.mod (libopenmpt pans it), desktop only (libopenmpt is not in the 68k build: open_failed). LED, both-knobs and the golden still run on selecta-test.mod in both trees; w0 on the mono fixture must equal plain.
2. m2_exec passes long absolute paths to vamos, which truncates them (open_failed). T53-vamos now runs with WORKING_DIRECTORY = repo root and relative paths (same fix as tests/amiga/run_render.cmake). Other m2 vamos tests that render files likely need the same; lib.cmake left untouched.

## Open questions
- Should the protracker4 replayer pan LRRL (as the other nuade players do)? It changes every render golden; then T53 could use the mono-free fixture in both trees.
- I129 (commands filter.led / mix.stereo_width, prefs, UI, T8 count) is open.
