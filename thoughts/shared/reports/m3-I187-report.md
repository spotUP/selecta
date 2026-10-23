# I187: desktop loudness analysis job (T89)

## What was built

- `src/analyse/loudness.{h,c}`: EBU R128 / ITU-R BS.1770-4 integrated loudness in double precision (K-weighting biquads, 400 ms blocks with 100 ms step, absolute gate -70 LUFS, relative gate -10 LU). It also keeps an RMS (unweighted, all channels) and sample-peak fallback. The gain is the -18 LUFS target minus the measured loudness, clamped to +-24 dB, in centi-dB.
- Desktop-only static library `selecta_loudness`, linked into `selecta_core`. It is not part of the `no-float-engine` target list. Amiga builds define `SEL_NO_ANALYSE`; the `ANALYSE` command there reports error `no_analyser`.
- Engine command `ANALYSE <path|local:key>` (`include/selecta/commands.def`, `src/engine/engine.c`): a private nested engine opens and plays the song with a 44100 Hz linear resampler. It renders 4096-frame chunks without real-time pacing until SONG_ENDED or a 1800 s cap, and feeds the analyser. The gain is stored per song key `local:<path>`. Stats fields `analyse_jobs`, `analyse_speed`, `analyse_lufs_cdb`, `analyse_gain_cdb`, `analyse_peak_cdb`, `analyse_gated`.
- Library DB: `sel_library_loudness_set/get` (`src/library/db/{db.h,db.c,user.c}`) use the existing `songs.loudness_gain` column (centi-dB); the schema is unchanged.
- CLI: `selecta-cli analyse <song>`, `analyse --sine <hz>:<dbfs> [--seconds n]`, `library analyse [--db p] 'source:local'`.
- Test T89 `loudness-analysis` (label m3): `tests/m3/loudness_analysis.sh`.
- RULES.md Commands table has rows for both new commands.

## Results

- 1 kHz sine at -23 dBFS: -22.991 LUFS. At -33 dBFS: -32.991 LUFS. Both within +-0.1. Fallback RMS reads exactly -23.0 and -33.0 dB.
- `analyse_speed` is about 760 to 840 for the sines and about 180 for the test module (needs >= 10).
- Test module: the gain is stored and read back equal (`stored_gain_cdb == gain_cdb`).
- ctest run on host: `loudness-analysis`, `ratchet`, `library-db-open`, `user-data`, `cues`, `protocol-reflection-commands`, `playlist-codecs`, `codec-optional-arg`, `stage-commands`: 9 of 9 pass. `selecta-cli commands --unimplemented` still lists 10.

## Deviations and open points

- The reference signals are synthesized by the CLI (`analyse --sine`) because no WAV replayer exists. "-23 dBFS stereo" is read as peak amplitude -23 dBFS on both channels, which gives -23 LUFS under BS.1770.
- The ANALYSE argument is a path or a `local:<path>` key. `analyse_jobs` and `analyse_speed` live in `sel_engine_stats` of the calling engine.
- Amiga build was not run (no `build/amiga` in this worktree). The analyser is excluded by `SEL_NO_ANALYSE`; verify on the Amiga build.
- The `no-float-engine` test is not registered on the host build, so it was not run.
- Not in scope and untouched: gain fetch/put to DEViLBOX, mixer gain, limiter, leveler (I188 to I190).
