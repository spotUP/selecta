---
date: 2026-10-22
topic: UP-Term (vtcon) as the Amiga host for the text UI — what the console supports
tags: [research, upterm, vtcon, text-ui, terminal, amiga]
status: final
---

# UP-Term (vtcon) capabilities relevant to a text UI

Source: `~/Code/UP-Term/vtcon` (RULES.md, README.md, engine/, thoughts/shared/research/2026-09-28_console-conformance-matrix.md, 2026-10-04_modern-terminal-gaps.md). Read on 2026-10-22, by grep and head only; line refs below are from those documents, not re-verified in the C sources.

## What it is
- A console handler (`XCON:`) for AmigaOS 3.x with an xterm personality, built from a portable C89 engine (`engine/vtengine.c`; no OS calls, no `//` comments, no `stdint.h`, `VT_MALLOC`/`VT_FREE`; compiles for plain 68000 under vbcc). The handler itself targets 68020 (`make amiga`, vbcc).
- Ships as the UP-Term kit with ixemul-based ports (tmux, screen, neovim, python 3, `vsh`), a terminfo entry `vtcon` matching the xterm personality, a PTY: handler, SIGWINCH on resize, an FS-UAE test rig driven from Python (`tools/rig/`), and an MCP server so Claude can drive the rig or a real Amiga.

## Terminal features a text UI can rely on (xterm personality)
- Colours: 16, 256 (6x6x6 cube, greys) and 24-bit direct colour (conformance matrix ~L294). On a 24-bit Workbench the renderer allocates pens; on fewer colours it approximates.
- Unicode: cells hold one BMP code point; UTF-8 input in the xterm personality; glyphs above U+00FF drawn from Unifont pages (`glyph_rig.py`). No grapheme clustering (?2027 missing).
- Graphics: DEC sixel (up to 1024 x 1024, 2 MB total image store, scrolls with the cells, `?80` DECSDM) and kitty graphics protocol (`engine/kitty.c`, ported from CTerm: a=t/T/p/d/q, f=24/32/100 PNG, chunked m=1, z>=0 only, no animation). Both land in the same indexed `vt_image` store.
- Mouse: X10/normal/SGR 1006/UTF-8 1005; left and right buttons only; motion reports (1002/1003) encoded but the host does not send them; wheel reports have a known pixel-vs-cell bug (gaps doc area "Wheel reports"). Alternate scroll ?1007 missing.
- Keyboard: xterm key encodings, modifyOtherKeys partial (Alt only on character keys), kitty keyboard protocol missing, focus in/out ?1004 supported.
- Synchronized output ?2026 supported (host holds at most 1 s).
- Alternate screen ?1049, bracketed paste ?2004, SGR underline styles 4:x and underline colour 58, overline, blink two rates, title stack, XTWINOPS 14/16/18/19 (cell and window size reports).
- Missing: OSC 8 hyperlinks, OSC 52 clipboard (by decision), DECSLRM margins, in-band resize ?2048, DA1 answers VT220 only.

## Consequences for the player
- One text UI, written against xterm-256color + UTF-8, runs unchanged on a Mac terminal and inside an UP-Term window on the Amiga. No Amiga-specific text renderer.
- Scopes and spectrum views can be real bitmaps in the text UI through sixel or kitty graphics, drawn from the same viz plugin output as the GUI builds; fall back to block/braille characters where graphics are absent.
- Program-side requirements on Amiga: open the console in raw mode and write escape sequences; read the window size from `CSI 18 t` or terminfo; handle SIGWINCH only under ixemul. A native (libnix) binary can use `CON:`-style raw I/O on the XCON: handle; confirm the handler's raw-mode path when M1 starts (unverified here).
- vtcon's engine rules are a proven recipe for 68000-portable C in this toolchain family and are worth copying for the engine core.
