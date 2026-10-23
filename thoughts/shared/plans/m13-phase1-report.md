# M1.3 Phase 1 report: viz taps (I85, I86, I87)

Branch: worktree-agent-ae23ccc3fbc089dee

## Rows

- I85: viz.h, VIZ tags 0x4000 to 0x4003, failing tests first (commit 2550bd6). Red evidence: phase-tap, viz-vu, viz-spectrum, viz-taps all failed against the stub; protocol-reflection-* and registry-tags stayed green.
- I86: mixer per-voice scope and peak, engine block closing, VU, triple buffer, sel_state.viz, render --viz-dump (commit 4ad74ac). T40 voice half, phase-tap, viz-vu, mixer-unit, golden renders and the four no-alloc tests green.
- I87: integer 256-point FFT, 16 bands, tools/gen_viz_table.c, tests/m13/viz.expected (generated twice, byte identical). viz-spectrum and viz-taps green.

## Deviations

- render --viz-dump still needs --output <wav>; the plan's T40 command omits it, the test passes it.
- VU release is level -= max(1, level >> 3) so small levels reach 0.
- Extra ctest entries viz-vu and viz-spectrum beyond T40 and T48. The phase, VU and spectrum host tests are skipped on the Amiga build (they use math.h).
- tools/gen_viz_table.c is new (Hann window, twiddles, band edges); it follows the gen_sinc_table pattern.
- Blocks close at the end of each sel_engine_render call over the output buffer, not by splitting the render at block edges. This keeps RENDER-kind call counts and golden renders unchanged. The dump is exact when the caller pulls rate/50 frames per call, as the CLI does in dump mode; with unaligned calls the voice taps can run up to one call ahead of the block edge.
- Spectrum bands take the maximum over FFT bins (bin = round(f * 256 / rate)). The 256-point resolution is 172 Hz at 44100 Hz, so the lowest bands share bins. Log edges give 1000 Hz in band 8 and 4000 Hz in band 12, as the test expects.
- Triple buffer swap is a plain volatile exchange on m68k (single core), __atomic_exchange_n elsewhere.
- Memory: the mixer grows by about 16 KB per slot, the engine by about 70 KB (three viz frames).
- phase_test uses rate 51200 so a block is exactly 1024 frames.
- Spectrum is skipped when SEL_MACHINE_CPU is below a 68030.
- viz_table.h is generated into the build dir and added as a selecta_core source.
- no_float object count is 23; tests/ratchet/unimplemented.txt is unchanged at 27.
