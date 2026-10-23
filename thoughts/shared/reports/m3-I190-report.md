# M3 I190: integer adaptive leveler with provisional gain write-back

Design: `thoughts/shared/plans/2026-10-22-architecture-design.md` section **6a. Loudness normalisation**
("When no gain is known: an optional adaptive leveler (slow integer AGC) converges within a few seconds and writes its
result back as a provisional gain"). Decisions D-M3-9 (-18 LUFS target, `mix.loudness` track|auto|off, -1 dBFS limiter)
and D-M3-12. Plan item T90 (`thoughts/shared/plans/2026-10-25-m3-library.md`).

## Changes

- `src/mixer/loudness_gain.[ch]`: `sel_leveler` (integer only). Measures the unscaled signal in 256-frame chunks,
  gates silence (mean L^2+R^2 >= 2000), keeps a running mean then an EMA (tau 2 s), slews the gain toward the target
  (600 cdB/s), clamps to the same +-24 dB table. `sel_leveler_estimate_cdb` returns the cumulative gated estimate
  (0 until 3 s of gated signal). `sel_leveler_ceiling_q16` bounds the gain over the next call for the limiter headroom
  reference. `sel_db10_cdb` integer 10*log10.
- `src/engine/engine.c`: per-slot leveler state (`lev_on`, `lev`). `loudness_slot` picks source `stored`,
  `provisional`, `leveler` (auto and no gain), `none`, `off`. `slot_gain` runs the leveler per chunk. `close_slot`
  calls `leveler_writeback` on the command thread through the library worker (never the audio thread).
  Stats: `leveler_frames`, `leveler_writes`.
- Library: schema v3 (`songs.loudness_prov`), `sel_library_loudness_set_ex/get_ex`. A provisional set only writes when
  the song has no gain or already a provisional one; a real set always wins and clears the flag. `SEL_LIB_SCHEMA_VERSION`
  is 3; `tests/m3/library_db_open.sh` updated.
- CLI render prints `leveler_frames= leveler_writes=`.
- Level convention is the analyser fallback (sum of both channels' squares / frames, dBFS), so a provisional gain
  equals what a real analysis would give.

## T90 assertion map (tests/m3/loudness_playback.sh, ctest `loudness-playback`)

- `auto` without stored gain: `loudness_source=leveler`, sentinel `leveler_frames >= 132300`, gain below unity (song
  above target), no limiter hits.
- Converges within 5 s: output level of seconds 5-6 within 1.5 dB of -18 dBFS (measured -21.0 per-sample, i.e. -18
  with both channels summed; second 1 already at -20.6).
- Provisional gain written: second `auto` render reports `loudness_source=provisional`, `leveler_frames=0`, gain
  within 15% of the leveler gain (DB holds -881 cdB in manual check).
- `track` on a provisional gain uses it; `track` on an un-analysed song: `none`, hash equals plain render.
- Real gain overrides provisional; later `auto` keeps it (`stored`, no leveler, gain equal to the stored q16).
- Existing checks unchanged: off hash == plain hash, track hash differs, limiter keeps samples <= -1 dBFS.

## Tests

Host build clean (-Werror). Amiga 68020 build clean. ctest pass: loudness (all 7 incl. loudness-playback,
mixer-loudness-gain-unit, library-db-open), mixer, stage-commands, render-golden-host, crossfade, loop-fade, analyse,
library, engine, history subsets (19/19 on the second group). Render golden unchanged with loudness off.

## Open items

- Provisional write needs >= 3 s of gated signal; shorter plays write nothing.
- `leveler_writes` in the render STATS line shows 0 because the write happens at slot close, after the stats line;
  the DB check proves the write.
- Leveler uses unweighted RMS (like the analyser fallback), not K-weighted LUFS.
- No dedicated unit test for `sel_db10_cdb`/leveler convergence in `tests/mixer/loudness_gain_test.c` (covered through
  the render entry point only).
- Amiga/a1200 and DEViLBOX parts of T90 are not covered here.
