# I133 rig run (emulated, NOT the real A1200)

Date 2026-10-23.

## Target
- Real A1200: NOT reached. `ami.py` defaults to 127.0.0.1:1234 with key `rigwasabi`: `ami.py ping` -> "auth failed: bad key"; `rig.py status` -> "down".
  A real Amiga needs AMI_HOST and AMI_TOKEN in the environment (not set, no host in ~/.config/wasabi/config, none found in the repos). No network scan done.
- Reachable: the UP-Term FS-UAE rig3 (`UPTERM_RIG=3`, port 1236, wasabid 0.11b2, `amiga_model=A1200`, `cpu=68060-NOMMU`, 8 MB fast, 1 GB reported). All results below are EMULATED on rig3.
  (The I212b report's "real 68060" numbers were also taken on this FS-UAE config, not hardware.)

## ctest -L rig (registered in build/host, none in build/amiga), UPTERM_RIG=3
Log: scratchpad rig.log.
- #121 rig-selecta-upterm (T56): RED. `ami.py get T:selecta-stats` -> Error 205 (stats file never written).
- #122 rig-task-priority (T58): PASS (task_pri_requested==5, task_pri==5).
- #123 rig-ahi-render (T59): RED. Same: T:selecta-stats not found. No ahi_bufs= sentinel produced.
- #124 rig-nofpu-voice-budget (T60): RED by design: needs the no-FPU rig (`rig.py start --a1200 --nofpu`, SEL_RIG_NOFPU=1). Not run. SKIPPED in substance.

## Cause of the T56/T59 reds (evidence)
1. First failure source: the Amiga RAM: disk. The tests `put` RAM:Selecta (12.3 MB) and RAM:selecta-cli (12.6 MB) and never delete; `ami.py info` showed
   "Ram Disk: 24 MB total, 0 MB free, 100%+ used" and from then on `ami.py exec` fails for ANY command ("could not start the command", even `Echo hi`).
   It stayed failing after I deleted my files (RAM: used then 12 MB / 0 free); `rexx` + `address command` still works, `exec` does not. RAM: appears to have no growth room
   (Ram Disk "total" = used), so the second 12 MB binary exhausts it. Unverified why the RAM: handler is capped; not a Selecta code bug.
2. Independent of RAM: `RAM:Selecta` run via rexx (OUTPUT=AHI and --output paula, and even `--help`) exits rc 1 with nothing on stdout and no stats file.
   stderr was not capturable (AmigaDOS has no 2>). main.c has `return 1` paths after sel_cli_open_catalogue, engine allocation failure, control-socket "already running", and
   "return 2" for unregistered output/ui. Candidate (unverified): engine allocation failure ("selecta: engine allocation failed") or the catalogue/library.db open with
   the 24 MB RAM: full. Needs exec working (stderr visible) to confirm. NOT fixed; no fix branch created.
3. exec being dead also blocks the ledger I214a check.

## Manual steps (via rexx, output redirected to BOOTX: and fetched)
- `selecta-cli machine`: probe_calls=1 clock.source=eclock CPU=68060 CPU_SPEED=23800 FPU=1 FPU_SPEED=23800 FAST_RAM=1082130432 CHIP_RAM=2093056 GFX=AGA+RTG AUDIO=paula NET=unknown DISK=unknown
  (note: FPU=1 here; I212b report saw FPU=0 on this rig earlier).
- `selecta-cli plugins`: null, voices-test, paula-shadow, paula, paula14-shadow, paula14, wav (kind=2); crossfeed (3); nuade, minimp3, stb_vorbis (1); text (5); all version=1. No hang.
- `selecta-cli render RAM:selecta-test.mod --output RAM:r.wav --seconds 2`: rc 0, `ticks=100 voice_ops=818 mixed_frames=88200`. No hang (I214a not reproduced).
- Sentinels `paula_ints=`, `ahi_bufs=`: not obtained (Selecta did not run).

## Next steps
- Make exec work again (reboot the rig: `rig.py` / `ami.py reboot`, I did not, UP-Term session owns it), then run T56/T59 with RAM: kept under capacity (delete selecta-cli before putting Selecta, or put on BOOTX:/VTCX:).
- Make run_rig.cmake delete its put binaries per test, or copy to a disk volume.
- Capture Selecta stderr (exec) to find the rc 1 cause.
- Real hardware needs AMI_HOST/AMI_TOKEN from Johan.

## Cleanup
Deleted from RAM: Selecta, selecta-cli, selecta-test.mod, st, r.wav, plus RAM:y, RAM:out, RAM:cc1, RAM:screen.txt, RAM:library.db (left by earlier sessions, deleted to free space; library.db 24 bytes), RAM:T/#? contents. BOOTX: scratch text files deleted.
No shell windows opened. RAM:selecta-i212b no longer present. UP-Term files untouched. Nothing committed, no worktree.
