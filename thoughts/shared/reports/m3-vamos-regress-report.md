# m3 vamos regression: libnix clock() constructor (report)

Culprit: commit 780f3a9 (I174 VOTE, merged in 118a205). Bisect: 006f6ac pass, 118a205 fail (same crash), 88798b1 fail.
Not I190. Every `*-vamos` case failed at 88798b1 (general crash at program start, before main).

Cause: src/engine/engine.c vote_token_make (`seed[0] = time(NULL)`, `seed[1] = clock()`, lines 2969-2970 at 118a205).
The Amiga build never referenced libnix clock() before; linking it pulls libnix `__initclock` into `__INIT_LIST__`.
`__initclock` opens timer.device through an exec/Process pointer chain that vamos cannot emulate, so it loads
A6=0x402 and `jsr a6@(-60)` jumps to PC=0x404. Trace: stack return address base+0x33e1ba = `__initclock+0x1a`,
called from the `__INIT_LIST__` runner in `_start`. src/core/amiga/clock.c already documents this ("libnix's clock()
opens timer.device at startup, which vamos cannot emulate"); time must come from the host (`host->clock_ms`,
HOSTQ_LOCAL_TIME), never libc clock().

Fix: vote_token_make now takes the engine and seeds from `local_ms(e)` and `e->host->clock_ms(e->host)`.
Evidence: `nm` of build/amiga selecta-cli lists `___initclock` before the fix and none after.

Tests (fail before, pass after; the existing top-level vamos cases are the regression, no new test):
all 14 `*-vamos` cases (mixer-led-stereo-vamos etc.) failed before. After the fix, with host and amiga rebuilt,
`ctest -R 'vamos|loudness-playback|vote-devilbox|rate-devilbox|library-db'`: 21 of 21 passed.
Reverting the fix brings `___initclock` back and the vamos cases crash again.
