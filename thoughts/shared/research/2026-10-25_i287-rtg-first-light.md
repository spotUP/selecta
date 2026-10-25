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
