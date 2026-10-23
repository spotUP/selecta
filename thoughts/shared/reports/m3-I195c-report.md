# I195c: race-free cancel of in-flight URL fetches

Plan: thoughts/shared/plans/2026-10-25-m3-library.md, Phase 11 (I194 to I196). Follows I195a (796af02).

## Change
- The boolean `e->net_cancel` (set by send, cleared by the pump) is gone. Replaced by a generation pair in the engine:
  `cancel_gen` (send increments it for every STOP/OPEN before the push) and `cancel_served` (the pump increments it
  for each canceller it pops, pump-owned). A fetch is cancelled while `cancel_gen != cancel_served`. Nothing is cleared.
- `sel_net_set_cancel(const volatile unsigned* gen, unsigned start)`: the thread's net waits end once `*gen != start`.
  Callers changed: engine pump (registers `&cancel_gen, cancel_served` at pump start and after each canceller pop),
  `sel_engine_destroy` (`NULL, 0`, the commit 9419800 behaviour kept), library job thread (`&job_quit, 0`).
  posix net.c, amiga net.c (no-op, comment: jobs run inline, revisit with I199), net.h updated.
- Sentinel `net_cancel_lost` (stats field): counts dispatches of a canceller with another canceller queued behind it
  and no cancel pending. Must be 0.

## Test
- `net-cancel-race` (tests/engine/cancel_race_test.c): sender thread sends 300000 STOPs, main thread pumps, asserts
  `net_cancel_lost == 0`.
- Fail-before (commit 3ecd26c, boolean flag): `cancel_race lost=854`, `3852`, `766` over three runs, exit 1.
  After the fix: passes (0 lost).
- Also green: stream-url (I195a hang test, http://hang.test), net-fixture-stop, library-close-cancel.
- Host and Amiga (build/amiga) builds compile, no errors.

## Found
- The old flag lost a cancel whenever send landed between the pump's `count == 0` check and the clear.
