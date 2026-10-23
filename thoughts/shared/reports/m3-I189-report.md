# I189 report: loudness gain and limiter (T90 gain/limiter part, D-M3-9)

Code commit: 2b7dd4e (branch worktree-agent-a259b740b76e82133)

## Changed
- src/mixer/loudness_gain.[ch]: integer centi-dB to 16.16 gain table (clamped +-2400 cdb), per-song gain, instant-attack -1 dBFS limiter (ceiling 29203), counters hits and frames (sentinel).
- prefs.def: mix.loudness = track|auto|off (default track).
- engine.c: loudness_begin() at song open/advance looks up local:<path> via sel_library_loudness_get (I188 store, reused); stage runs after the DSP chain. off = source "off"; no stored gain = "none", untouched. Stats gain four loudness fields.
- selecta-cli render: --loudness, --db, --gain-cdb; prints loudness_source= gain_q16= limiter_hits= leveler_frames=0 loudness_frames= only when one is given.

## Tests (host build/host)
- mixer-loudness-gain-unit: pass (table within 1 LSB of libm over all cdb, ceiling, bypass, exact scaling)
- loudness-playback: pass (off hash == plain, track differs, per-sample plain*gain, stored gain read back from --db, hot +24 dB peak <= 29203, hits > 0)
- mixer-stage-unit, stage-commands, render-golden-host-*, loudness-analysis, loudness-fetch: pass
- Amiga build (build/amiga): builds, rc 0.

## Open
- auto behaves like track (adaptive leveler is I190); T90 convergence and DEViLBOX/a1200 parts not covered.
- Limiter runs only when a stored gain is applied.
- Pref change mid-song applies at the next song open; gain switch at a crossfade is abrupt.
- render usage line in main.c not updated.
