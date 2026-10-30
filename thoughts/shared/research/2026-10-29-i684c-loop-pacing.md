# I684c: does the MUI main loop busy-spin? (Opus measure, 2026-10-29)

Method: code reading only (no rig run; rig measure left for the fix's test). Verdict per output below.

## One shell turn (apps/selecta/main.c:732-771)
1. ui.wait(state, clockless ? SHELL_TURN_MS : 0)
2. sel_engine_pump (handed over: hw_clock renders by wall clock, engine.c:6769-6787, never blocks)
3. so.pump(so.out, rate*SHELL_TURN_MS/1000) only if so.out != NULL
4. control socket: compiled out on Amiga (apps/selecta/CMakeLists.txt:7, SEL_NO_CONTROL_SOCKET)
5. arexx_poll: GetMsg loop, never blocks (plugins/scripting/arexx/arexx_amiga.c:88)

MUI wait(0) with a timer: mui_io_wait (plugins/ui/mui/mui_ui.c:1948-1968) only does SetSignal; Delay(1) only when
treq == NULL. mui_wait_step (plugins/ui/mui/map/mui_wait.c) adds nothing that blocks.
Text UI is the same: txt_term_read(0) -> WaitForChar(Input(), 0) (plugins/ui/text/amiga/term.c:128). So the spin is
not MUI-specific.

## Per output
| output | where a turn blocks | spins? |
|---|---|---|
| null / wav (clockless) | UI wait(SHELL_TURN_MS) | no |
| AHI | ahi_out.c:62-66 submit() -> hw_wait -> WaitIO (ahi.c:103) once per full buffer; sel_engine_render always returns `frames` (silence when stopped/paused, engine.c:7547-7563), so it paces idle, paused and playing | no |
| paula (Amiga default: machine.c:148 puts "paula" first, registry.c:178) voice/mixer route | nowhere: paula_out.c:69-88 out_pump only pulls; paula.c bus is register writes; plugins/shared/paula has no Wait/Delay | YES, idle and playing |
| any output while a HW-kind replayer plays (KPlayer, plugins/replayers/kplayer/adapter.c; route HANDOVER engine.c:6935) | nowhere: so.out == NULL so step 3 is skipped; hw_clock is wall-clock and non-blocking | YES, even with AHI selected |

So: Selecta on Amiga with the default paula output spins at 100% CPU always; with AHI it spins whenever a native
HW player (the PT .mod path) owns the chip. The comment "A real output paces the loop" is false for paula and for
handover.

Side effect worth checking (unverified): paula voice-route songs are pulled as fast as the loop turns, so their
tempo is tied to loop speed (hw_clock protects only the HW route).

## Violated invariant
Every shell turn blocks for about one turn in exactly one place: the output's pump when that output paces, else the
UI wait with SHELL_TURN_MS. The shell passes 0 on "not clockless", which conflates "has a real clock" with "its
pump blocks".

## Level of the fix: shell loop (main.c), plus one output capability tag
Not the UI wait (rejected 16dbb2b1 was right to be rejected: it adds a 20 ms block on top of AHI's WaitIO).
Not paula out_pump alone: the handover case has no output at all, so only the shell knows nothing paces.

## Recommended fix
- New output tag SEL_OUT_UNPACED (pump never blocks), set in sel_paula_out_describe (covers paula and paula-shadow).
- Shell keeps a `paced` flag in shell_output (updated on open, switch and handover) and computes one value:
  `turn_ms = (clockless || so.out == NULL || !so.paced) ? SHELL_TURN_MS : 0`, used for ui.wait (and ctl serve).
  AHI and coreaudio keep wait(0): no underrun risk, no behaviour change on macOS.
- Test (host, reachability): run the real shell entry with `--output paula-shadow --ui text` (or a stub UI that
  records its timeout argument) and a song; assert every wait got SHELL_TURN_MS and the sentinel count > 0; same run
  with ahi-shadow asserts 0. Unit: handover (so.out == NULL) -> SHELL_TURN_MS. Revert proof: old code gives 0.
- Rig 2 check (no probe needed): main.c already prints `ui_waits=` at exit. Run Selecta --ui mui with
  virgill-that_meme_again.mod for 30 s on paula, quit via ARexx, compute ui_waits/30: expect ~50/s after the fix,
  thousands/s before. Repeat with --output ahi (KPlayer handover) for the second case.
