# M1.3 Phase 4b report (row I95: early prepare, gapless, crossfade, mixed_path)

## Red line (before)

    Test #59: crossfade ...***Failed 0.44 sec (unknown directive @stats)

After: crossfade (T44) passes. Host and Amiga builds green. Affected ctest subset
(crossfade, loop-fade, queue, gapless, ratchet, no-float, no-alloc, golden, sdk,
repeat, fade, silence, max, unimpl): 17 of 17 pass. Unimplemented-command ratchet: 22 (limit 22).

## Commits

See `git log` on branch worktree-agent-a458ca0826b366204 (single commit "Add crossfade, early prepare and mixed_path (I95)").

## What was built

- OPTION crossfade 0..10000 ms, default 0 (gapless).
- Next song prepared into the idle slot before the expected end; audio thread switches at the song end (gapless) or mixes with an equal-power Q16 table (crossfade).
- tools/gen_xfade.c generates xfade_table.h (256 steps, Q16 cos table, incoming gain is the table read backwards).
- Snapshot field mixed_path; set by OUTPUT from the SEL_OUT_VOICES capability.
- voices-test output plugin (cmake name voices_test): crossfade on it answers `ERROR unsupported_path OPTION`.
- `@stats` CLI directive: `STATS xfade_frames=N slots_mixed=N mixed_path=N`.

## Deviations

1. Prepare lead is crossfade_ms + 2000 ms, not max(crossfade, 2000).
2. Early prepare only when the end is known (fade running, or max_song_ms set). Songs with unknown end (ENDED, silence) get no early prepare, since no duration is used (Q20). Eager prepare broke script tests that do not render (cancel stays unconsumed, both slots busy, OPEN/NEXT defers).
3. Table is generated into the build gen dir (xfade_table.h), not committed under src/mixer/.
4. The outgoing song's own fade gain still multiplies during the crossfade (continuous gain, double attenuation).
5. SONG_ENDED and SONG_OPENED fire at the switch (end of the crossfade).
6. Cancel race: if the audio thread already switched, the new song plays on.
7. tests/m1/queue.expected line 53: `POSITION 0 0 0 0` became `POSITION 0 3 0 1925`. The gapless switch renders about 1925 frames of the new song in the same render call before the pump emits its first POSITION.
8. viz_feed uses the outgoing slot pointer during the switch block.

## Questions for the user (default chosen)

- Should songs with unknown end get early prepare via a duration estimate? Default: no (Q20 stands).
- Should the outgoing fade gain be skipped during a crossfade? Default: no, keep it multiplying.
- Accept the queue.expected change as correct gapless behaviour? Default: yes.
