# M2 Phase 4a report: I122, I123, I126 (paula14)

Design doc section 14. Plan: `2026-10-24-m2-amiga.md`, "## Phase 4".

## Rows

- I122 done: `plugins/outputs/paula14/common/p14_split.{h,c}`. Default linear table (D-M2-4), max error over all 65536 inputs <= 1 LSB of 14 bits. T52 `paula14-tables` green on desktop and vamos (hash equal).
- I123 done on desktop/vamos shadow, unverified on hardware: `p14_sink` (4 chip RAM channels, double buffer, `SEL_MEM_SAMPLE`), `p14_out` shared skeleton, `paula14-shadow` (counting bus), `paula14` (custom chip bus, Amiga only). T54 `no-float-interrupt` extended with `p14_split p14_sink p14_out` and green.
- I126 done: paula14 has no voice backend, so RENDER kind and VOICE kind of any voice count route to the mixer (`sel_engine_voice_route` with `has_sink=0`). Proven in `paula14-unit` (voice counts 0..32) and `paula14-routing` (+ `-vamos`).
- I124, I125 (AHI): skipped, rig-only. No `ahi` plugin exists; it would take the same no-voice-backend path, no engine change needed.

## Tests run (affected only)

paula14-tables, paula14-tables-vamos, paula14-routing, paula14-routing-vamos, paula14-unit, paula-out-unit, paula-voices-unit, no-float-interrupt, amiga-build, registry-tags: all pass. `registry-tags-negative` and `rp-config-taglist*` were red/planned-red not caused by this work (registry-tags-negative not re-run after the count change). `selecta-cli commands --unimplemented | wc -l` = 21.

## Deviations and open items

- Plugin-count expectation `SEL_EXPECT_PLUGINS` 9 to 10 (new host-only `paula14-shadow`).
- New tags `STAT_P14_FRAMES/MAX_ERR/HASH/BUFS` (0x2015..0x2018) in `tags.def`; `render` prints them.
- Hardware pacing of `sel_p14_submit` (audio task CIA tick) and the `paula14` hardware output are unverified: rig/T56.
- Hardware `paula14` fails to open above about 31.4 kHz (period 113 limit); shadow clamps.
- Calibration file loader (`ENVARC:Selecta/paula14.cal`) not implemented; only default table plus `lo[64]` hook.
- Buffer length constant 512 frames, `paula14.buffer` host config override; per-CPU-class `sel_prefs` is I125.
- Partial final buffer is not flushed at close.
