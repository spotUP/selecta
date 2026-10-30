---
date: 2026-10-30
topic: I728 MP3 micro drops in MUI Selecta on rig 2
status: parked (Johan: test on a real A1200 first); one run measured, cause not proven
---

# I728: MP3 micro drops in MUI Selecta (rig 2)

Johan: "an mp3 played in mui selecta ... sometimes i got rapid micro audio drops".

## Measured (one run, then parked)

Temporary instrumentation in `apps/selecta/main.c` (per-turn timing around `so.pump`, plus the output's stats at exit), now reverted.
Rig 2, 68060, 256 MB, Synchronize.mp3, MUI window shown, ARexx QUEUE_ADD + PLAY, 60 s, QUIT.

| Run | Condition | Output | Underruns | Decode ms | Wait ms | Turns | Turn gap max / >10 ms / >20 ms | Pump max | Mac fs-uae load |
|-----|-----------|--------|-----------|-----------|---------|-------|------|------|------|
| r1 | shown, default | **paula14, not AHI** (no AHI stat tags) | n/a (paula14 has no counter) | n/a | n/a | 10799 | 1554 ms (startup) / 9 / 1 | 75 ms | 4 rigs (1, 2, 5, 7) at 98-102% each |

Other lines from r1: `replayer=minimp3`, `shell_wait_ms_min=0 shell_wait_ms_max=20`, `ui_open_mui=1`.
The runs planned with iconified and `output.buffer 4096` were not done (hunt parked; rig 2 also went down once in between).

## What the code shows (not yet proven on the rig)

1. **The default 060 output in this run is paula14, not AHI.** The brief assumed AHI 1024 frames; the AHI stats (underruns, decode, wait) do not apply to what Johan heard if he used the default.
2. **paula14 is treated as a paced output but never blocks.** `plugins/outputs/paula14/common/p14_out.c` does not set `SEL_OUT_UNPACED` (only `plugins/outputs/paula/paula_out.c:128` does), so `sel_shell_wait_ms` (`apps/common/shell_pace.h:11`) returns 0 and the MUI wait does not pace the loop. `out_pump` comments that "the audio task calls this once per CIA tick, which paces the swaps", but `sel_audio_task_start` (`src/core/amiga/audio_task.c`) has no caller anywhere.
3. **`sel_p14_submit_n` (`plugins/outputs/paula14/common/p14_sink.c:101`) has no link to the hardware.** It writes the "idle" half by its own toggle and re-points AUDxLC; it never checks which half Paula is playing. Halves are 512 frames at 31388 Hz (16.3 ms); each shell turn pumps 627 frames (20 ms). r1 ran 10799 turns in about 70 s of loop time (about 6.5 ms per turn, so about 3x faster than real time if every turn pumped a full 627 frames). Production that is not locked to playback overwrites or skips halves, which would give drops.
4. Unclear: 3x over-production would sound like heavy chopping, not "micro drops". Before any fix, measure pumped frames against wall time (instrumentation was written for that: run r2, not finished).
5. The shell's host `config_get` (`src/host/host.c:3`) always returns the default, so `output.buffer` and `paula14.buffer` cannot be set in Selecta (only in selecta-cli render).

## Next (when unparked)

- Repeat r1 with pumped frames / play ms (does production match 31388 Hz?), then with `--output ahi` shown, iconified and 4096-frame buffers.
- Fix to propose if (3) is confirmed: pace paula14 from the hardware, either by starting the CIA audio task for the Amiga shell or by an AUDx interrupt/position read in `p14_sink.c` that blocks submit until the queued half has been picked up; plus an underrun counter in paula14. Also wire the shell's `config_get` to the prefs (`src/host/host.c:3`).
