---
date: 2026-10-27
topic: I525 AGA desktop UI busy loop (clicks late), I524b, wasabid death
status: fixed on host; unverified on AGA rig
branch: worktree-agent-adaf9a1d40cc70940
commits: d06c302, ce7f541, f305474
---

# I525: what keeps the AGA desktop UI loop from idling

## Measured (Mac, live loop on SDL dummy driver, SELECTA_SDL_TEST_LIVE=1, 3 s)

Counters added to the stats line: `turns idle= present= scan= input= viz=` (where desk_wait_turn returned)
and `bits song= queue= transport= progress= volume= status=` (dirty bits seen at present).

| run | idle | present | viz | progress bits |
|---|---|---|---|---|
| stopped, home view | 4 | 1 | 0 | 1 |
| playing selecta-test.mod, before fix (Debug) | 0 | 32 | 5153 | 31 |
| playing, earlier build (first measurement) | 0 | 3 | 135 | 3 |
| playing, after fix | 144 | 3 | 0 | 3 |

During playback every turn returned through the visualizer branch; the idle poll was never reached.
Present/progress counts are low, so progress repaint is not the driver.

## Root cause

plugins/ui/desktop/desktop_ui.c, desk_wait_turn: after `vz = viz_tick(d)` the turn did `if (vz) return 1;`.
viz_tick draws a frame whenever the engine block sequence moved (about every 20 ms) and the visualizer
strip is visible (default prefs: visible). The turn then returned without waiting, and the live loop
started the next turn at once. The wait in desk_sdl_poll (which already caps at 20 ms when viz_visible)
was skipped every time.

On AGA (plugins/ui/desktop/backend_aga.c) each such turn is a c2p of the strip plus
ChangeScreenBuffer/WaitTOF. The UI task never Wait()s on its IDCMP port in desk_aga_poll, so it holds the
CPU flat out: IDCMP clicks are only taken between frames (late or lost, the I525 symptom; the pointer
stays smooth because it is a hardware sprite), and other pri-0 tasks get little CPU.

## Fix (f305474)

`if (vz && !d->live) return 1;` A live window goes on to the bounded poll after a visualizer frame, so the
task waits for input up to the next engine block. The visualizer still gets one frame per block (the poll's
20 ms cap). Non-live runs (golden/test frames) keep the early return.

## Test

dui-idle-turns (tests/dui/idle_turns.cmake, registered in tests/dui/CMakeLists.txt): real live loop, 3 s,
stopped (present <= 5) and playing (present <= 30, idle >= 50, present + viz <= idle). Own control socket
(SELECTA_SOCKET) so a selecta already running on the machine does not make it exit "already running".

Revert check: fix removed, rebuilt: test FAILS (idle=0 present=32 viz=5153). Fix restored: passes
(idle=144). Related dui tests (hover/viz/live/tween, 9) and viz tests (8) pass.

## I524b (AGA screen ~3 min after launch) and wasabid death on rig 2

Not proven. Plausible link for wasabid: the busy loop starves it (same priority, CPU never released while
playing with the strip visible). The 3-minute delay before the screen appears happens before playback, so
this fix does not obviously explain I524b. Both unverified on the rig: rig 2 not run in this pass (budget).

## Open

- Confirm on rig 2 AGA: build m68k, start Selecta, play, click; read the `turns` line under --stats;
  check wasabid survives past 3 min.
- I524b needs its own trace of launch time to first present.
