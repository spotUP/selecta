# I525 click latency and I523 stack measure (partial, no numbers)

## Done
- Instrumentation (kept): with `--stats`, desktop_ui.c writes one stderr line per non-move pointer event (`input t= kind= x= y= hit=`) and one per repaint (`frame end= ms= px=`). apps/common/amiga_start.c prints `stack used N of M bytes` after the swapped run when built with `-DSEL_STACKMEASURE`.
- AGA build with those flags: configure with `-DSEL_DESKTOP_AGA=ON` and `-DCMAKE_C_FLAGS="-m68020 -mcrt=nix20 -fomit-frame-pointer -DSEL_STACKMEASURE"` (the toolchain flags must be repeated; a bare define drops -mcrt=nix20 and net.c fails on ssize_t). The worktree has empty submodule dirs third_party/nuade and libopenmpt: symlink them to the main checkout.
- Rig 2 restarted with purpose "MusicPlayer AGA agent: click latency and stack measure"; selecta-aga copied to build/rig2/vtc and started with `--ui desktop --backend aga --log RAM:sel.log --stats`.

## Not done (blocked)
- About 3 minutes after launch wasabid on rig 2 stopped answering (connection refused on 1235) while fs-uae kept running. Same symptom as I524 (ledger). The log file is exclusively locked while Selecta runs (Error 202), so the log cannot be read until Selecta exits, and Selecta cannot be asked to exit once wasabid is dead. No click, latency or stack number was obtained; no fix and no regression test were written; RULES.md line 22 is NOT corrected.
- So the first-order finding for I525 is that the AGA process starves the system (wasabid dies) well before clicks can be judged. Suspect: task priority or a busy loop in desk_aga_poll/present (ChangeScreenBuffer loop with WaitTOF, safe_port wait), not input translation. Unverified.

## Next
1. Find what starves wasabid (run Selecta at priority -5 via `ChangeTaskPri`/`Run <NIL: nice`; check whether the first frame, 9.4 s, runs at priority 0 with the OS blocked in a busy WaitTOF loop).
2. Write the log to a file that is not locked: flush and close/reopen per line, or log through a second path (serial.log) so reading does not need Selecta to exit.
3. Then repeat the 5-click run and the STACKMEASURE exit.

## Addendum: button trace
With --stats on the AGA backend each MOUSEBUTTONS message now logs `button down|up x y stamped=<Intuition time> taken=<time the poll took it> downs= ups=`; the `input t=` line gives the handled time and `frame end= ms=` the frame time. Stamped to taken is the wait behind a frame; downs vs ups shows a lost half of a pair. Built, not run (rig 2 is down).
