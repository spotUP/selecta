# I186 RADIO from a seed (T88)

Command: `RADIO <seed|off>` (commands.def, group playlists).

Steering rule: `src/library/radio.c`, `sel_library_radio_next`. Candidates come from `sel_library_similar`
(genre match, or rating >= 4) and `sel_library_also_by` (same composer). Weighted draw, weight 1 + genre match
+ (rating - 3 when >= 4); composer rows weight 2. Skip-flagged songs are excluded (library key and queue form).
The previous pick is avoided unless it is the only candidate. The library takes the draw from the engine
(`sel_xorshift32` on a seed derived from the engine shuffle seed), so one seed gives one sequence.

Engine: `radio_fill` tops the queue up to 5 songs ahead, called from `queue_start` and `commit_next`.
Stats: `radio_appends` and `radio_min_ahead` in `sel_engine_stats`. Script directive `@radio` prints them and the queue.

Test T88 `radio` (tests/net/radio.cmake, tests/m3/radio.sel is a template with @SONGS@): 20 NEXT, appends >= 20,
min ahead >= 5, every appended song matches genre/composer/rating, skip-flagged down.mod is never appended,
two runs are identical. Needs the net fixture and sqlite3 (like T86).
Before the change: RADIO unknown, no `radio_appends=` output. After: pass.

Limits: radio assumes sequential play (ahead = rows after the playing one).
Unimplemented commands count stays 10. Host and Amiga 68020 builds are clean (0 warnings, 0 errors).
