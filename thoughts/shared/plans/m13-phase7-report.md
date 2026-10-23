# M1.3 Phase 7 report (rows I104a to I104d)

Design doc: section 14 M1.3 and section 8. Plan: `2026-10-23-m1-3-viz-voices-playback.md`. All tests green in `build/host` (19 of 19 in the affected set: ui-*, selecta-m13-*, no-alloc*, sample-export*). `build/amiga` builds (no engine object added, no_float count stays 27).

## I104a, T26 no-alloc with crossfeed on (commit 2c54036)
- Change: `tests/m0/no_alloc.sh` render branch passes extra args to `render` and, with `--dsp`, requires `dsp=... blocks=N>0` so the window provably covered the plugin. New test `no-alloc-render-dsp`.
- Red: temporary `malloc(16); free()` in `xf_process` (crossfeed.c): "173 allocations on the render path". Code restored.
- Green: all 5 no-alloc tests pass.

## I104b, T46 nuade sample scan (commit 81f46f8)
- Change: `tests/m13/nuade_samples.sel`, `nuade_samples_check.cmake`, test `sample-export-nuade`. Expected lengths and bytes are parsed from the module itself (IFF MODL wrapper, PTDT chunk), not from output.
- Red: SEL_RP_SAMPLE_GET hook disabled in the nuade adapter: "expected 4 SAMPLE lines, got 0". Restored.
- Green: sample-export and sample-export-nuade pass.

## I104c, key-level engine state (commit cf887b7)
- Change: exit line `engine_state pitch_cents= tempo_percent= mute_mask= mutes=` in `apps/selecta/main.c`; keys files and `keys_engine_check.cmake`; tests `selecta-m13-keys-pitch_tempo_solo` and `-unsolo`.
- Red: bound `S` to @info and `>` to `TEMPO +0` in keybind.c: both tests failed. Restored.
- Green: both pass (100 cents, 10 percent, mute_mask 14; unsolo gives mask 0).

## I104d, note dots and phase view (commit d3606ef)
- Change: V cycle now has 6 states (off, levels, scope, spectrum, note dots, phase). New UI stat tags `STAT_DOT_ROWS` 0x5015 and `STAT_PHASE_POINTS` 0x5016, exit line `viz_marks`, tests `ui-viz-dots` and `ui-viz-phase` (counter above 0 plus golden screen).
- Red: tests written first with empty goldens; both failed with no `viz_marks` line.
- Green: both pass, 4 repeats stable. The goldens were generated from the first green screen and inspected by eye (dots spread over the scale, Lissajous diagonal with correlation 99 percent).

## Deviations
- Q27: nuade slot numbers are catalogue positions (1,2,4,3 for selecta-test.mod), not module sample numbers. The test pins this.
- No ASCII fallback for the phase plot (braille) or dots (U+25CF), same as the existing scope and spectrum panels, which also write UTF-8 unconditionally. Plan asked for `.:|` fallback.
- Note dots use the Paula sample rate as pitch (the engine reports rate, not note). Index 0 = 2072 Hz = tracker C-0; C-2 (8287 Hz) is index 24. Real note pitch would need a replayer note field.
- Phase panel in the non-mixed path prints `unsupported_path`; no test covers that branch (null output is always mixed).
- `phase_points` counts plotted samples (256 per frame drawn), not distinct cells.
- No golden for the spectrum panel (Q30) was added; out of scope of I104.

## Open questions (default)
- Map nuade slots to module sample numbers (default: keep Q27).
- Add ASCII fallback to all viz panels together (default: no, UTF-8 only).
- Add a test for `unsupported_path` via a non-mixed output (default: skip until such an output exists).
