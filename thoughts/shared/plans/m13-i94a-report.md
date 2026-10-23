# I94a report: first loop of a non-looping song ends the song

Rule chosen (adapter level, plugins/replayers/nuade/adapter.c rp_tick): nuade's Protracker reports LOOPED on the tick
that plays the last row, before it is known where the song lands (the player's position hook names the row just
played). The adapter holds that LOOPED back until the position changes (the first tick of the landing row, at most
one row later) and reports ENDED when the landing order is 0 (the song wrapped or jumped back to its start), LOOPED
when it lands on a later order (Bxx back to order n > 0, a loop by design). Players without a position hook, or not
in Rows mode, are unchanged. Engine untouched: it already maps ENDED to reason `end`.
Quirk found: nuade pt4 position_jump compares against a byte read from the IFF header, so a B-jump to the current
order is not reliably detected as a loop; only a jump to an EARLIER order is.

Changes: adapter.c; tools/gen_testmod.c `--loop` (3 orders, B01 at order 2 row 63 -> order 1) producing
selecta-loop.mod; tests/CMakeLists.txt (fixture + new test loop-first-end at the END); tests/m13/first_{end,loop}.sel
and first_check.cmake; loop_loop.sel, loop_fade.sel, crossfade.sel moved from test/short.mod to selecta-loop.mod
(those songs now end with `end`, so they no longer exercise a loop or fade).

Repinned tests/m1/queue.expected: line 51 `SONG_ENDED loop` -> `end`; line 55 `SONG_ENDED loop` -> `end`;
line 53 `POSITION 0 3 0 1925` -> `POSITION 0 0 0 0` (song 2 opens with no fade tail rendered before the pump).

Verified: ctest (33 tests: queue, loop, crossfade, sleep, golden, nuade, position, seek, render, stop, repeat) pass.
Revert check not run; without the adapter change selecta-short.mod reports `loop`, so loop-first-end fails.
