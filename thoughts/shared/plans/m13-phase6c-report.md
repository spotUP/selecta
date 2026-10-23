# M1.3 Phase 6c (I103) report

T39 `selecta-m13-reach` is green; the `viz_frames=` exit line is printed by `apps/selecta/main.c`.

## Changed
- `include/selecta/tags.def`: UI stat tags STAT_VIZ_FRAMES, STAT_SCOPE_READS, STAT_PATTERN_ROWS (0x5012 to 0x5014), same mechanism as the T28 sentinels.
- `plugins/ui/text/text_ui.c`: counters bumped in the real draw code (viz panel frame with voice data, scope voice read, pattern row drawn), exposed through `tui_stats`.
- `include/selecta/engine.h`, `src/engine/engine.c`: `sel_engine_stats` gains mutes (set_mute_mask), fades (both fade starts, in policy_chunk and policy_loop, now non-const engine), crossfeed_on (dsp chain query), sleep_armed (timer sleep_left_ms).
- `apps/selecta/main.c`: prints the line after the existing exit counters.

## Results
- T39 red before (`no viz_frames line`), green after. Host ctest of registry-tags, ratchet (T8, stays 21), m12-reach, ui-panel-*, sleep-*, m13: 11 of 11 pass. Amiga build completes, rc 0.
- No new object, so cmake/no_float.cmake is unchanged.

## Deviations / open
- SLEEP_TIMER `not_implemented`: not reproducible. SLEEP_TIMER and ALARM are in SEL_CMD_IMPLEMENTED and dispatch; after a fresh build in this worktree stderr is empty. The earlier error came from a stale build. T8 therefore did not drop (no verb changed state); it stays 21, which the plan names as the target.
- Default: `fades` counts end fades started (loop count or max length); T39 does not assert it (as in 6b).
- T39 was red in this worktree on the first run, so no separate red commit.
