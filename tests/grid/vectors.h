/* T25 byte vectors for the text UI cell grid (plan M1.1 phase 5, row I59).
 * Written from the terminal protocol, not from the implementation's output. */
#ifndef SELECTA_TEST_GRID_VECTORS_H
#define SELECTA_TEST_GRID_VECTORS_H

/* CSI ? 2026 h / l: synchronized output begin and end. */
#define VEC_SYNC_BEGIN "\x1b[?2026h"
#define VEC_SYNC_END "\x1b[?2026l"

/* One default-style 'X' written at row 6, column 11 (1-based): move, glyph. */
#define VEC_ONE_CELL "\x1b[6;11HX"

/* Same cell in xterm-256 colour 196 on 21, bold: move, SGR, glyph, reset. */
#define VEC_ONE_CELL_STYLED "\x1b[6;11H\x1b[0;1;38;5;196;48;5;21mX\x1b[0m"

/* Two neighbouring cells need one cursor move only. */
#define VEC_TWO_ADJACENT "\x1b[1;1HAB"

/* A 3x2 grid after clear + text "abc" on row 0: full redraw, framed. */
#define VEC_FULL_3X2                                                          \
    VEC_SYNC_BEGIN "\x1b[1;1Habc" "\x1b[2;1H   " VEC_SYNC_END

#endif
