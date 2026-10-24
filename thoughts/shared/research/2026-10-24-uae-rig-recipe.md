# FS-UAE rig recipe for Selecta (Amiga 68020) - verified 2026-10-24

Works: booted, ran `selecta-cli machine` and headless `selecta`, read text back. Boot to ALLDONE took well under 5 minutes.
Not UP-Term's rig.py/wasabid: an own FS-UAE config (rig3/rig5/rig6 untouched; UP-Term read only).

## Pieces
- Emulator: `~/Code/Emulators/fs-uae/fs-uae` (3.1.66 local build). Kickstart: `~/Code/UP-Term/roms/kick40068-a1200.rom`.
- System disk: `~/Code/UP-Term/vtcon/build/rig4/sys.hdf` mounted READ ONLY as DH0 (rig4 must not be running).
- Binaries: `build/amiga/apps/selecta/selecta`, `build/amiga/apps/selecta-cli/selecta-cli` (Amiga build, see RULES.md Commands).
- Songs: any host files, e.g. `tests/m3/lyrics/*.mod`, `t2.mp3`/`t2.ogg`.

## Layout (S = scratchpad dir, e.g. .../scratchpad/uae-rig)
- `S/cfg.fs-uae`: A1200, cpu=68020, fpu=68882, fast_memory=8192, jit_compiler=0, uae_cpu_speed=real, uae_cpu_cycle_exact=true,
  uae_cpu_compatible=true, hard_drive_0=sys.hdf (read_only=1), hard_drive_1=S/work label WORKX, hard_drive_2=S/boot label BOOTX
  priority 10, joystick_port_1=none, volume=0, initial_input_grab=0, logs_dir=S/logs. Template: scratchpad of session 2d1f083b `rig/a1200.fs-uae`.
- `S/boot/s/startup-sequence` (host folder is the boot drive; this is the only thing that makes it run a command):
  Assign SYS: DH0:, C: DH0:C, S: DH0:S, LIBS: DH0:Libs, DEVS: DH0:Devs, T: RAM:, WORK: WORKX:, ENVARC: WORKX:, ENV: RAM:, then `C:Execute WORKX:run`.
  Using `DH0:C/Assign >NIL: X: Y` lines. ENVARC: MUST be assigned or selecta/selecta-cli open an invisible requester and hang (src/core/config_dir.c).
  LIBS: must be assigned or the libs fail to load.
- Deploy = copy files into the host dir `S/work` (it IS WORKX:): selecta, selecta-cli, songs/, keys.txt, run.

## Run + output
`S/work/run` (AmigaDOS script) redirects everything to `WORKX:out.txt`, which appears on the host in `S/work/out.txt`; end with an `ALLDONE` marker.
Example run script:
    Echo >WORK:out.txt "START"
    WORK:selecta-cli machine >>WORK:out.txt
    WORK:selecta --size 80x24 --keys WORK:keys.txt --screen-dump WORK:dump.txt --start-dir WORK:songs >>WORK:out.txt
    Echo >>WORK:out.txt ALLDONE
keys.txt = `wait 500` then `q`. Screen text lands in `S/work/dump.txt` (host). No serial or screenshots needed.

## Commands
    cd S && (nohup ~/Code/Emulators/fs-uae/fs-uae cfg.fs-uae > fsuae.log 2>&1 & echo $! > pid)
    for i in $(seq 1 55); do grep -q ALLDONE S/work/out.txt 2>/dev/null && break; sleep 5; done; cat S/work/out.txt
    kill $(cat S/pid)
Delete/overwrite `out.txt` is done by the script itself on start; to re-run, edit `work/run` and restart the emulator (no live reload).

## Observed
Machine: CPU=68020 CPU_SPEED=185 FPU=1 FAST_RAM=8 MB CHIP_RAM=2096128 GFX=ECS AUDIO=paula,ahi TIER=planar. selecta headless printed
`ui_frames=1 ui_hash=8989f5a4`, browse listing of songs/ (.., ambient.mod, techno.mod), rc=0.

## Caveats / what failed
- `selecta --version` does not exist; use `selecta-cli machine` (help: `selecta-cli help`).
- Not testable here: AGA/RTG/68060/network/real Paula timing. 68060 via FS-UAE gave CPU=68040 FPU=0 (I194); real-board-only rows stay on hardware.
- Cycle exact 020 is about 1x host speed: renders take minutes (2 s of mp3 render about 2 min).
- Earlier I213 hang with -m68881 link was never explained; this recipe ran the current binary fine.
- Windows are opened on the host (FS-UAE window); no input grab needed.
