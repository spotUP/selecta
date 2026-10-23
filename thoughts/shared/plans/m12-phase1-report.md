# M1.2 Phase 1 report (I65, I66, I67)

Branch: worktree-agent-af7a6eb01a25ea649. Plan: 2026-10-23-m1-2-playlists-views-control.md, Phase 1.

## Done
- I65: `src/protocol/parse.c` holds `sel_engine_send_line`; selecta-cli script mode, the selecta shell and the text UI call it. No behaviour change.
- I66: `include/selecta/prefs.def` (X-macro, 4 prefs), `src/engine/prefs.{h,c}`, `prefs.conf` in the config directory, `sel_config_dir` (`src/core/config_dir.c`), `sel_config_mkdir` (`src/core/macos/config_mkdir.c`, Amiga `CreateDir` version untested, no Amiga build run), PREF_GET and PREF_SET in the engine. The shell passes the directory (`--config-dir <dir>` overrides). Ratchet limit lowered 45 to 43 (measured live count 43).
- I67: T33 `title-format` written first. Red run: `Test #24: title-format .....***Failed 0.25 sec`, `0% tests passed, 1 tests failed out of 1`. Then `src/engine/format.c` (`%field%`, `[...]`, `%%`), used by the view model for queue rows. T33 green.

## Tests
Whole suite except coreaudio-play (needs an audio device) green, includes T1 to T27 and T33. Ratchet green at 43.

## Deviations
- One commit, not three: I65 touches engine.h, shell and CMake files that I66 also changes, so a split would not build.
- PREF_SET with an invalid format value is rejected (`ERROR bad_format`) instead of stored. Expand-time fallback to the default format exists for values loaded from the file.
- PREF_GET replies with a `PREFS_CHANGED key value` event.
- Queue row fields come from the path only (title = file = basename with extension, format = lowercase extension); other fields empty until the M3 metadata store. Browse rows are not formatted.
- `sel_engine_send_line` is declared in `engine.h` (needs the engine type), not `protocol.h`.
- The worktree needed `git submodule update --init --reference` for third_party/nuade and libopenmpt (not committed).
- Ratchet target 27 comes from later phases.

## Open questions
- Amiga `sel_config_mkdir` should be checked on the rig.
- Link step prints "ignoring duplicate libraries: libselecta_core.a" for test targets (linked twice); check whether it predates this phase.
