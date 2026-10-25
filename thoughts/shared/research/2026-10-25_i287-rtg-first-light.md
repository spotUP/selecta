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

## Update 2026-10-25 (rig 2: Home, keys, Ctrl-C leak)
- Note: the `amiga` MCP tools were NOT pointed at rig 2 (they use 192.168.0.74 / port 1234); rig 2 was driven with `UPTERM_RIG=2 python3 tools/rig/ami.py` (exec, put, key, screens, shot, uitree). Selecta put to SYS:selecta-test (rig 2 has no Rushhours:). Run: `run >NIL: SYS:selecta-test/Selecta --ui desktop --backend rtg --canvas 640x400 --log RAM:l.log`, Ctrl-C by `break <proc> C`.
- (a) Home renders at 640x400x16: nav rail with icons, transport bar (Stopped, play, volume); the song list area is empty (empty library).
- (b) Root cause of "process ends without stats, screen stays open": libnix's own `__chkabort` polls Ctrl-C inside stdio and calls exit(), which skips desk_rtg_close and the UI close. Idle in the backend's Wait the break is caught by the backend (clean close, screens 3 -> 2); once keys were handled the break landed elsewhere and exit() ran. Proof: key trace via stderr (keys arrive as IDCMP_RAWKEY, map, queue; no crash on Return), process vanished with no quit or close trace. Fix: apps/selecta/main.c defines an empty `__chkabort` on Amiga. After the fix: keys then break closes the screen (screens 2 -> 1) on a fresh rig 2.
- Regression: ctest `amiga-selecta-overrides-libnix-ctrl-c-exit` (tests/amiga/check_no_libnix_abort.cmake) checks the 68k main.c object defines `__chkabort` (fails without the override). Host-only, no rig; the screen close itself was checked on rig 2 by hand.
- Keys do reach the backend (IDCMP path fine: window active, RAWKEY delivered, queue_key returned 1 for Down and Return). Home showed no visible change because Home has no navigable rows with an empty library; this is not confirmed to be a desktop_ui key-handling gap (desk_key in desktop_ui.c, not touched). Check with a populated library.
- I340 (oversized layout): the canvas is chosen in plugins/ui/desktop/desktop_ui.c desk_make_canvas, default 1280x800 (`--canvas WxH` overrides). The Amiga backend opens a screen of exactly that size (pick_mode), so a default run opens a 1280x800 screen that exceeds a smaller monitor / Workbench (rig 2 Workbench is 800x600, Replay 680x270). Fix lands in desktop_ui.c (default = Workbench screen size on Amiga, or backend reports its preferred size): not done, owned by another agent.
