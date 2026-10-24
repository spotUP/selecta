# I218f and I218g report

## Doc citations
Source: AmigaOS wiki, Console Device (https://wiki.amigaos.net/wiki/Console_Device), the RKM Devices console.device control sequence table.
- Set Page Length: `9B <length> 74` (CSI n t), "in character raster lines, causes console to recalculate ... how many text lines will fit on the page". Set Line Length CSI n u, Set Left Offset CSI n x, Set Top Offset CSI n y likewise.
- "Give Back What You Take": the console handles these four automatically; "to allow it to do so again after setting your own values, send the functions without a parameter" (parameterless CSI t / u / x / y). The page does not state numeric defaults and does not say explicitly that the page follows window resizes; it says the console recalculates. Unverified beyond that: whether an 18-line page set by an older Selecta persists in the window (it would, the console keeps it until given back).
- Window Status Request `9B 30 20 71` (CSI 0 SPACE q); reply `CSI 1;1;<bottom>;<right> SPACE r`.
- SGR: 0 plain, 1 bold, 3 italic, 4 underline, 7 reverse, 22-29 off, 30-37 / 40-47 system colours 0-7, 39 default fg, `>0-7` background. 49 is not listed (we never send 39/49: put_sgr starts from 0). The page does not say which colour each pen number shows.
- xterm reads CSI 0 SPACE q as DECSCUSR with parameter 0 (blinking block / default cursor style): harmless, no effect on size. (From xterm's ctlseqs; not run against UP-Term.)

## I218f
Cause: query_size sent ESC[18t with ESC[0 q; the console.device reads CSI 18 t as Set Page Length 18.
Fix: new plugins/ui/text/sizeprobe.c/.h (txt_size_probe, pure C99, I/O injected). Order: CSI 0 SPACE q first; on a native answer send parameterless CSI t (give the page length back, in case an older run set one) and ask again; only without a native answer send CSI 18 t. amiga/term.c query_size now calls it (WaitForChar 200 ms per read, as before). Cost: UP-Term start waits one 200 ms timeout before CSI 18 t.
Test: grid-unit test_size_probe (fake console: native answer -> no ESC[18t written, ESC[t written, native size used; xterm-only -> ESC[0 q first then ESC[18t, no ESC[t; silent -> size 0).
vamos byte-stream test: NOT possible. vamos has no SetMode/WaitForChar and IsInteractive needs a tty, so txt_term_open fails under vamos and query_size never runs there. The host test drives the same function the Amiga binary links; amiga-build compiles it for 68k.
Revert check: probe sends 18t first -> test_size_probe fails (2 checks). Restored, passes.

## I218g
Cause: put_sgr always emitted 38;5;N / 48;5;N.
Fix: plugins/ui/text/grid.c: with g->console, each 256-colour index maps to the nearest of 8 ANSI colours (RGB distance, xterm RGB table) and is emitted as SGR 30-37 / 40-47. No existing colour-to-8 mapping in the codebase (plugins/ui/text/palette.c is the command palette). Xterm path unchanged (VEC tests pass; test checks bytes). Assumption: pen N shows the ANSI colour N; on the stock Workbench palette pens differ, so colours are right only with an ANSI-like palette. Unverified on hardware.
Test: grid-unit test_console_sgr (header 231/24 bold -> `ESC[0;1;37;46m`, red 203 -> `ESC[0;31m`, no 38;5/48;5; xterm grid keeps `ESC[0;1;38;5;231;48;5;24m`).
Revert check: console flag ignored in put_sgr -> 3 checks fail. Restored, passes.

## Run
grid-unit, amiga-build, amiga-text-ui-vamos, amiga-console-first-frame-vamos, amiga-console-keys-vamos, text-ui-keys-wait: 6/6 pass. 0 warnings outside third_party in host and amiga builds. `commands --unimplemented | wc -l` = 8.
Johan to confirm on the Replay: list fills the window, colours show, no 18-line page.
