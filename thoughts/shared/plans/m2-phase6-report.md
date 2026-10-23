# M2 Phase 6 report (I130, I131, I132)

Done
- I130: `plugins/ui/text/amiga/term.c` (SetMode raw, WaitForChar timed wait, CSI 18 t size query once at open, restore on close). Not run on a real console; only the headless path ran (T55).
- I132: `apps/selecta` and `plugins/ui/text` build for Amiga (`build/amiga/apps/selecta/selecta`). Control socket compiled out with SEL_NO_CONTROL_SOCKET. T55 `amiga-text-ui-vamos` green (ui_hash identical to desktop).
- I131 PARTIAL: `amiga/spawn.c` (SystemTagList) written and linked, NO vamos spawn test. Row stays open.

Changes outside amiga/: `--size WxH` headless mode in text_ui.c (fixed grid, keys from stdin, no terminal control), stats `ui_frames`/`ui_hash` (FNV-1a of flushed bytes; tags 0x5017/0x5018 in tags.def), main.c prints them and no longer needs a function-pointer cast through void*. ui.cmake passes `--start-dir ""` to both runs so the frame does not depend on the working directory. RULES.md Commands row updated.

Tests: amiga-text-ui-vamos, amiga-build, text-ui-keys-wait, selecta-browse-play, cmdline, palette, ui-panel*, ui-viz*, m13 keys, no-float-interrupt all pass. Unimplemented verbs: 21.

Deviations / open
- vamos does not implement WaitForChar or GetCurrentDirName; read on a non-interactive stdin skips WaitForChar. Interactive path (T56 on the rig) untested.
- Window size is read once at open; live resize on Amiga is not handled.
- Amiga start dir: main.c only browses when the start path begins with '/', so on a real Amiga the default start dir will not browse (AmigaDOS paths). Needs a decision: separate row.
- txt_spawn quoting of paths with a double quote is unhandled.
- I accidentally pkill-ed a vamos process once; it may have affected another agent's run.
