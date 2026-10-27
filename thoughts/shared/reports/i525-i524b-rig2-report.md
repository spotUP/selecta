---
date: 2026-10-27
topic: I525 AGA click check and I524b launch trace on rig 2 (UAE)
status: measured, no fix needed for I524b
branch: worktree-agent-a7d88ee58c5b56d09
---

# I525 / I524b rig 2 report

Build: AGA 68020, main 5201118 merged (fix `if (vz && !d->live) return 1;` in `desk_wait_turn`). Rig 2 only, purpose "MusicPlayer AGA agent: I525 rig check and I524b launch trace".
Launch: `Selecta --ui desktop --backend aga --format index8 --view library --stats --log RAM:aga.log --exec "QUEUE_ADD ...techno.mod" --exec PLAY`.

## Q1: clicks during playback

- techno.mod played about 220 s. 12 sidebar clicks, 12 registered (`input kind=2 hit=1`), heading region changed 12 of 12. Button stamped-to-taken latency about 0.1 to 0.2 ms.
- wasabid alive at 3 min 36 s (ping "wasabid 0.11b2" before and after).
- Playing: `turns idle=65498 present=38 scan=0 input=0 viz=0`, frames=75, turn_ms_max=84, frame_ms_max=62.
- Stopped (OPEN only, about 4 min): `turns idle=218 present=1 viz=0`.

## Q2: launch trace (new `trace t_ms=... <phase>` lines under `--stats`)

| Phase | Time |
|---|---|
| main begin to engine created | ~40 ms |
| desk_open begin to icons built | ~3 ms |
| icons built to canvas made | ~2.66 s |
| screen open | ~0.12 s |
| scale applied | ~45 ms |
| first present | ~40 ms |

Screen visible about 4 s after launch (two runs agree). The reported 3 minutes did not reproduce on a build with baked fonts and icons. Largest phase: canvas creation, `desk_open` between "icons built" and "canvas made" (canvas and pixel alloc, `sel_gui_canvas_init`, 32 UI colour setup). Not drilled further.

## Caveats and open items

- viz=0: no visualizer strip in this layout, so the I525 viz branch was not exercised. Only the outcome is confirmed, not the mechanism.
- During playback idle turns run about 300/s (uim_updates=65589) versus about 1/s stopped. CPU cost not examined.
- Unverified: the old 3-minute report came from a build without baked font/icon tables (I463).
- No fix, so no regression test. Trace lines have no ctest.
