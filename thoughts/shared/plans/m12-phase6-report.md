# M1.2 Phase 6 report (I79, I80, I81)

Branch `worktree-agent-adac28ccf09c20f45`.

## Evidence

- Red (before implementation):
  - `1/2 Test #43: control-socket ...................***Failed    0.28 sec`
  - `2/2 Test #44: integration-nowplaying ...........***Failed    0.12 sec`
- Green: `control-socket Passed 3.37 sec`, `integration-nowplaying Passed 0.15 sec`.
- Whole host suite `ctest -j2 -E coreaudio`: 100% passed out of 51.
- Amiga build (`build/amiga`, Ninja, amiga-68020 toolchain): green. no_float object count unchanged (new files are in apps/ and plugins/).
- Ratchet `tests/ratchet/unimplemented.txt`: measured 27 in this tree, unchanged.

## Deviations

1. `SELECTA_SOCKET` env var overrides the socket path for shell and CLI. Default: `$XDG_RUNTIME_DIR/selecta.sock`, else `$TMPDIR/selecta-<uid>.sock`.
2. The test uses a relative socket path (`ctl.sock`, test cds into its work dir): an absolute path under a worktree build dir exceeds macOS `sun_path` (104 bytes).
3. With `--keys` the socket is off unless `SELECTA_SOCKET` is set.
4. `socket_cmds` counts only OK-answered lines; unparseable lines get `ERROR ...`, `send` exits 1.
5. Shell polls the socket after `ui_wait` (20 ms timeout with null output so `wait 3000` really lasts 3 s).
6. `INT_ON_EVENT` takes an extra `const sel_state*` snapshot argument. Remote commands are queued only from `INT_POLL`; the macOS plugin spins the CFRunLoop inside `INT_POLL`.
7. Shell option `--integration <id|none>`; default `nowplaying` (macOS) or `mpris` (Linux) only with a non-null output and no `--keys`. `selecta-cli script` has `--integration` and the `@integration_poll` directive.
8. T38 expects `duration_ms=0`: the engine reports no duration for the Protracker test module (the first guess of 30000 was wrong). Publish count `np_publish=3 np_command=1` matched as planned.
9. On Amiga `send` is a stub (exit 2), `SEL_NO_CONTROL_SOCKET`.
10. T5 plugin count: base 6, +2 on APPLE (coreaudio, nowplaying), +1 on Linux with `SEL_HAVE_MPRIS`. Passes on macOS.
11. Macos mapping: play -> PLAY, pause -> `PAUSE_PLAYBACK on`, toggle -> PAUSE, next -> NEXT, previous -> PREV, position -> `SEEK <ms>`.

## Unverified

- `plugins/integrations/mpris/linux/mpris.c` (sd-bus) could not be compiled on this macOS host. UNVERIFIED. Built only when libsystemd is found.
- macOS Now Playing plugin compiles clean under `-Wall -Wextra -Werror` but was not driven by a real Control Center.

## Manual check (macOS)

1. `build/host/apps/selecta/selecta --output coreaudio --start-dir <dir with a song>`
2. Open and play a song in the shell.
3. Open Control Center: the Now Playing tile shows the song title.
4. Press pause in Control Center: playback pauses in the shell within one turn.
5. Optional: from another terminal run `selecta-cli send PAUSE`; the shell replies `OK` and toggles.
