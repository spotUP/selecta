# M1.3 Phase 5b report: crossfeed (I99, I100)

Commits: 3a3e6da (T47 red), b14e63f (I99), bcab7cd (I100).

Red record: `crossfeed-render` failed; `crossfeed-unit` Not Run (executable EXCLUDE_FROM_ALL because dsp.h did not exist yet).
Green: host ctest all pass (crossfeed-unit, crossfeed-render, no-alloc, palette-unit, registry-tags). Host and Amiga builds green. Unimplemented-verb ratchet stays 27.

## Deviations and decisions
- Coefficients are handed to the plugin by the engine through DSP_PARAM, because plugins compile against include/ only and the table is generated into the engine.
- Delay is 14 frames at 44100 Hz scaled by rate and rounded (the test expects (14*r+22050)/44100).
- `mixed_path` lives in the engine, set through the new public `sel_engine_set_output(e, out_hooks)`; the DSP verb answers `ERROR unsupported_path DSP` when the output provides voices. Not stored in sel_state.
- The DSP verb is placed in palette area "view" (palette-unit requires one of five known areas, in order).
- The DSP verb emits no ack event. Slots reopen on rate change only through set_mix.
- int64 intermediates in the one-pole filter and the mix (no float).
- SEL_EXPECT_PLUGINS bumped 6 to 7; cmake/no_float.cmake expected object count 23 to 24 (dsp_chain.c).
- Golden hash tests/golden/dsp/crossfeed.sha256 is NOT pinned: needs a listening check first; the render test prints the computed hash.
- `selecta-cli render --dsp <name>` added; prints `dsp=<name> blocks=N`.
