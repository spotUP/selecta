---
date: 2026-10-25
topic: I287 rtg backend first light (M4-P5-1)
status: built, not yet shown on the Replay
---
# I287 rtg first light

## Built
- `plugins/ui/desktop/amiga/backend_rtg.c/.h`: Picasso96 screen (mode via p96BestModeIDTagList, OpenScreenTags with the P96 mode id), backdrop window for IDCMP, present = p96WritePixelArray per dirty rect from the A8R8G8B8 canvas, input through sel_gui_input_map, timer.device timed wait plus Ctrl-C.
- `amiga/art_thread.c` stub (runs fn inline). CMake: desktop UI now builds for Amiga (`-DSELECTA_ICONBAKE_EXE=<host gui_iconbake>` needed). Fixed clang-only pragmas in atlas_build.c and stb_impl.c.
- New stats `SEL_UI_STAT_BACKEND`, `SEL_UI_STAT_SCREEN_DEPTH`, printed as `ui_backend id=... backend=... depth=...` by apps/selecta/main.c.
- `selecta` for 68020 links (15.4 MB). Host dui tests not rerun (see below).

## Replay results
- Replay has Picasso96API.library only (no cybergraphics). Workbench screen is 680x270x8.
- `Selecta --ui desktop --backend rtg --canvas 1280x800 --stats` run from a script: no RTG screen appeared (screens list showed Workbench only), exit code 1, no stdout.
- Same with `--backend mem`: exit 1 and no output, so the failure is before the rtg backend, in the desktop plugin or its Amiga start (not yet found).
- An earlier run with `--ticks 3` printed `desktop_ui closed frames=0`, with exit 0.
- Debug blocker: wasabi/amiga_run does not capture stderr (libnix stderr), AmigaDOS has no `2>`. All error messages of desk_open are lost. Next step: a `--log <file>` or stdout copy of errors, or wasabi `debug` (KPrintF).
- Soft float on 68020 not measured (nm of undefined libgcc symbols still to do).

## Update 2026-10-25 (exit 1 root cause, rig 2 run)
- Added `--log <file>` (freopen stderr; the Amiga shell cannot redirect libnix stderr) and an allocation failure trace in src/core/amiga/mem.c.
- Error text on the Replay: `selecta: engine allocation failed`, trace `alloc of 2097152 bytes ... largest public block 1661048, free ~13 MB`. Cause: src/core/amiga/machine.c reported TOTAL fast RAM (192 MB), so the engine picked the 64 MB tier (8192 entries, 2 MB text buffers x ~8) that did not fit the 13 MB / 1.6 MB-block free memory. Not a single-instance lock. Fix: the probe reports free fast RAM (commit 989734c). With it `--backend mem` ran: `desktop_ui closed frames=2`. Also: a 15 MB Selecta in RAM: leaves too little memory; run it from a disk drawer (Rushhours:selecta-test).
- Replay then had no RTG mode (per coordinator) and wasabid hung; no further Replay work.
- Rig 2 (uaegfx, Workbench 800x600x24): `--backend rtg --canvas 640x400 --ticks 3 --stats` printed `rtg: screen 640x400 depth 16`, `ui_backend id=desktop backend=rtg depth=16`, `desktop_ui closed frames=4`; one Ctrl-C closed the screen, Workbench front again.
- Second run with keys `down down return` then Ctrl-C twice: process vanished without stats and the 640x400 screen stayed open (screen leak or crash on that path), unresolved. Page change by key not verified.
- selecta-cli under vamos failed to load graphics/keymap: the backend defined GfxBase etc., which libnix auto-opens; renamed to sel_rtg_* (eaea4ea). source-local-scan-vamos passes; hash-idle-vamos fails later on the host `library duplicates` md5 grouping (RED[I184b], not the library load).
