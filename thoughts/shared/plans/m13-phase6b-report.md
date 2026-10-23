# M1.3 Phase 6b report (row I102: T39 written and red)

Commit: see `git log` on branch worktree-agent-ae20932f30b0b647a ("Add T39 selecta-m13-reach ...").

## Red output
`ctest -R selecta-m13-reach`: `no viz_frames line` (stdout has only the ui_waits, songs_opened and cmdline_runs lines; stderr `error not_implemented SLEEP_TIMER`). Fails for the expected reason: the exit counter line (I103) and SLEEP_TIMER (other agent) are missing.

## Module choice
No change needed for T39. The player is chosen by format: `selecta-test.mod` (gen_testmod default, signature "Protracker4") is played by nuade (voice kind, 4 voices, header shows `Protracker4`); `selecta-test-mk.mod` (M.K.) goes to libopenmpt (render kind, no voice data). The existing `ui-panel-viz` golden used the mk module, hence "No per voice data". T39 and the new panel goldens use `selecta-test.mod` (`SEL_TESTMOD`).

## Files
- `tests/m13/reach.keys`, `reach.cmake`, `reach.screen`; ctest `selecta-m13-reach` in `tests/CMakeLists.txt`.
- Q30: `panel_levels_voices.{keys,screen}` and `panel_viz_voices.{keys,screen}` (ctest `ui-panel-levels_voices`, `ui-panel-viz_voices`, nuade module); the original mk goldens stay (they cover the "no per voice data" notice). All screens are real dumps from the current shell, byte-identical across two runs; both new panel tests and the old ones pass.

## Deviations
- Pattern panel and viz panel are exclusive (`P` clears viz, `V` clears pattern), so one screen cannot show both scope rows and `C-2` pattern rows. Key order is `P`, wait 300, `:DSP crossfeed on`, `:SLEEP_TIMER 1`, `V`, `V` (levels, then scope), `1`. The last screen is the scope view with voice 1 blank (the muted voice's scope is drawn blank; the `Muted` label exists only in levels mode, covered by `panel_levels_voices.screen`). `pattern_rows>0` is proven by the counter; the pattern screen with notes by `ui-panel-pattern`. The golden comes from a real run, not by hand.
- `reach.screen` was produced while SLEEP_TIMER answers `not_implemented`; the screen shows no message line, so it should not change when I103 lands. If it does, regenerate it and say why.
- Test checks V, S, P > 0 and `mutes=1 dsp=crossfeed:on sleep_armed=1`; `fades=F` is parsed but unchecked (plan gives no value).
- Shell scripted-key time is simulated (fast), so `wait 1000` costs nothing.

## Open questions (default)
- Should `Muted` also be drawn in scope mode so the scope view marks the muted voice? Default: no, keep the UI as is.
- Should the T39 key plan be amended to the order above? Default: yes, plan row T39 text to match.
