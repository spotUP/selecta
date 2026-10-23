# M1.3 Phase 4c report (row I96)

Branch: worktree-agent-a784dea085ceb4298 (base 248716b)

## Commits
- d5dcb45 Add HOSTQ_LOCAL_TIME, fake clock, SLEEP_TIMER and ALARM (I96)

## Results
- T45 `sleep-alarm`: green (was red at start). New `sleep-ramp` (unit test of ramp shapes, midnight wrap, daily re-arm): green. `sleep_cancel.sel` (SLEEP_TIMER 0 and ALARM off cancel) runs inside T45.
- Ratchet: `commands --unimplemented` 22 to 21; `tests/ratchet/unimplemented.txt` updated.
- no-alloc tests: green (first run failed with 39 allocations from localtime_r on the render path; fixed by priming the time zone state in sel_engine_create).
- Amiga build (`build/amiga`): compiles.
- Full host ctest: only `registry-tags` and `registry-tags-negative` fail. Cause predates this work: `tests/CMakeLists.txt` expects 9 plugins, the tree builds 10 (the nowplaying plugin from I79-I81 is not counted). Not touched here.

## Deviations from the plan
- Clock code lives in `src/host/clock.c` and `src/core/amiga/clock.c` (existing layout), not `src/core/{macos,linux,amiga}/clock.c`.
- HOSTQ_LOCAL_TIME (tag 0xA001) returns milliseconds since local midnight.
- Ramps are a separate Q16 gain in `src/engine/timer.c`; the VOLUME state is not changed or re-emitted.
- The audio thread polls the clock once per render call.
- Red test not committed separately: T45 was already red at the start; `timer_test.c` and `sleep_cancel.sel` were written together with the code and not seen red on their own.

## Open questions (default chosen)
- Alarm is minute-granular: setting it inside its own minute fires it (needed by T45, which sets 07:30 at 07:30:59). Default kept.
- Sleep expiry while not playing is dropped. An alarm while already playing does nothing. A manual pause or stop cancels a running down ramp. Defaults kept.
- Alarm ramp starts from 0 even when resuming a paused song.
