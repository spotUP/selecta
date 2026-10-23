# M2 Phase 3 report (I118, I119, I120, T54)

Branch worktree-agent-a228452034abc5b69. I121 (routing) not done.

## I118 Paula voice sink - commit 48d8e4e
- plugins/outputs/paula/paula_voices.{c,h}: integer sink behind `include/selecta/voices.h`, bus abstraction (`write`, `set_lc`).
- Chip copy through `host->mem_alloc(..., SEL_MEM_SAMPLE)`; S16 converted by >>8; start and loop word aligned; freed on stop.
- Engine hookup: `sel_engine_set_voice_sink()`; new OUT tags VOICE_BACKEND 0x2010, STAT_REG_WRITES 0x2011, STAT_LOG_DIGEST 0x2012.
- Conformance test `paula-voices-unit` (tests/paula/paula_voices_test.c): green.

## I119 period, volume, loop - same commit
- AUDxPER = round(3546895*256 / hz_q8), clamped 113..65535, written once per note (dedupe on last value).
- AUDxVOL = volume_q16 >> 10, 0..64. Loop part written as second LC/LEN after DMACON set.
- Deviation: AUDxLEN clamped to 65535 words. nuade samples of 131072 bytes wrapped to LEN=0.

## I120 shadow twin - commit 3c1dd9d
- plugins/outputs/paula/shadow.c, output id `paula-shadow`. Log lines `W <reg> <val>` and `LC <reg> <serial> <offset>` (serial, not address, so logs are host independent). Own SHA-256.
- `selecta-cli render --output paula-shadow selecta-test.mod --seconds 2` prints `aud_writes=53 reg_log_sha256=7d84a091514122afdd33e88056dace3ff9b8dc03cad8b9965b530c6d1afce2dc`.
- Golden `tests/golden/m2/paula-voice-registers.sha256` created from that run. Two desktop runs equal. Manual vamos run (build/amiga CLI) gives the identical hash.
- Hand check: selecta-test.mod channel 0 row 0 is sample 1 at ProTracker period 428 (C-2, tools/gen_testmod.c g_scale). Log has `W 00a6 000001ac` = AUD0PER 428 = round(3546895 / 8287.26 Hz). D-2 (381, 0x17d) also appears on channel 0.
- T51 desktop: green.
- Plugin-count expectation in tests/CMakeLists.txt 8 -> 9 (new plugin). `render` parser now accepts options in any order (T51 argument order needs it); `--output` is a WAV path if it has '/' or ends in .wav, else a plugin id.

## T54 - commit 76c5b29
- OBJECT library `selecta_paula_irq` (plugins/CMakeLists.txt, Amiga only) joins `m2-interrupt-objects.txt` via `$<TARGET_OBJECTS>`. Want-list now `audio_int audio_task paula_voices`. no-float-interrupt green.

## Open / not done
- T51 vamos under ctest FAILS in this worktree: the amiga CLI gets `open_failed` for the long absolute path (also fails mixer-led-stereo-vamos, so environmental, not my change). Same command with a relative file name under vamos passes with the equal hash. Re-run in the main checkout.
- Hardware plugin `paula.c` ($DFF000 writes) not written, not rig verified.
- Amiga `src/host/mem.c` does not map SEL_MEM_SAMPLE to MEMF_CHIP (plain malloc).
- shadow.c is not in the T54 list (only the sink); the shadow runs under vamos with nuade float shim, outside the interrupt.
- paula14-tables (T52) and rp-config-taglist (T61) are red: later phases, not touched.
- I121 routing (config-driven selection of the sink) not done; the CLI `--output <id>` is minimal wiring only.
- Over the tool-call budget.
