# I218e: Amiga console shows no list lines until Down

Design doc: 2026-10-22-architecture-design.md section 7 (UI plugins).

## Evidence (measured where possible)
- The size is known before the grid is built (txt_term_open runs query_size, text_ui.c:1297-1308 builds the
  grid afterwards), so candidate (a) is ruled out by reading the order: no regrow before the first frame.
- Candidate (d) ruled out: txt_term_write loops Write(Output()) to completion; there is no user-space buffer.
- Candidate (c): the replies are consumed inside query_size (loop until 't' or 'r', 200 ms); a late reply is
  decoded as a key by amiga_console parsing and cannot suppress a present. Not the cause by reading.
- The one structural difference between the first frame and every later frame (the second one is what
  "after Down" shows) is in grid.c txt_grid_flush: only a FULL redraw (dirty_all) is wrapped in
  CSI ? 2026 h ... CSI ? 2026 l, and only a full redraw writes every cell, including the bottom right one.
  Diff frames (which Johan sees work) contain neither. Byte stream of the first 3x2 frame, captured from the
  unit test: ESC[?2026h ESC[1;1Habc ESC[2;1Hdef ESC[?2026l.
- The console.device documents no private-mode (CSI ?) sequences, and scrolls the window when the last cell
  of the last row is written (the xterm pending-wrap rule does not exist there).
- Unmeasurable here: vamos has no console.device and the real Amiga was off limits, so the exact console
  reaction is NOT confirmed. The fix removes both first-frame-only constructs; Johan must confirm on the Amiga.

## Fix
- grid.h/grid.c: new `txt_grid.console` flag. A console grid emits no CSI ? 2026 frame and never writes the
  bottom right cell (front keeps it equal to back). text_ui.c sets it from txt_term_amiga_console.
- text_ui.c: `--console-frames 1` makes a host run draw console frames, so tests can compare byte hashes.

## Tests (ctest)
- grid-unit: test_console_first_frame (exact bytes of the console first frame, corner skipped, diff frame).
- amiga-console-first-frame-vamos (script tests/m2/ui.cmake): 68k ui_hash equals the host console-frame hash
  and differs from the plain xterm hash. T55 (amiga-text-ui-vamos) now runs the same script.
- Revert check: grid.c restored to HEAD: grid-unit FAILED (3 checks), amiga-console-first-frame-vamos FAILED;
  with the fix all 5 affected tests pass (grid-unit, amiga-build, both text-ui vamos tests, console-keys).

## Found
- put_sgr emits xterm 256-colour SGR (38;5;N / 48;5;N). console.device SGR knows only 0-7 attributes and
  30-37/40-47 pens; unknown parameters are at best ignored, so the Amiga console shows no theme colours.
  Not changed here (no evidence it hides text); a separate row.
- query_size sends ESC[18t; on console.device CSI n t is "set page length", so it sets the page to 18 lines.
  Candidate for a follow-up: send the xterm ask only when the native ask got no answer.
- third_party/nuade and third_party/libopenmpt are empty in worktrees; I symlinked them locally (untracked).
