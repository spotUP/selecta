/* Text UI cell grid with a diff-based flush (design doc section 7, "own small
 * widget layer over a cell grid with a diff-based flush, no curses").
 * Plan M1.1 phase 5, row I59. Pure C99 plus libc; no OS headers. */
#ifndef SELECTA_TEXT_GRID_H
#define SELECTA_TEXT_GRID_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TXT_COLOR_DEFAULT 256u /* terminal default foreground or background */
#define TXT_ATTR_BOLD 1u
#define TXT_ATTR_REVERSE 2u

typedef struct txt_style {
    uint16_t fg; /* xterm-256color index or TXT_COLOR_DEFAULT */
    uint16_t bg;
    uint8_t attr; /* TXT_ATTR_* */
} txt_style;

typedef struct txt_cell {
    uint32_t cp; /* Unicode code point; ' ' for empty */
    txt_style style;
} txt_cell;

typedef struct txt_grid {
    uint32_t cols, rows;
    txt_cell* back;  /* what the UI draws into */
    txt_cell* front; /* what the terminal shows after the last flush */
    int dirty_all;   /* next flush redraws everything */
    uint32_t flushes;
    uint32_t bytes; /* total bytes produced by flush */
    int latin1;     /* set by the caller after init: flush writes Latin-1, '?' above U+00FF (row I218a) */
    int console;    /* set by the caller after init: the Amiga console.device (row I218e): no CSI ? 2026 frame, never writes the bottom right cell */
} txt_grid;

txt_style txt_style_make(uint16_t fg, uint16_t bg, uint8_t attr);

/* Allocates both buffers (UI thread, never the audio thread). Returns 0 or -1. */
int txt_grid_init(txt_grid* g, uint32_t cols, uint32_t rows);
void txt_grid_free(txt_grid* g);
/* Changes the size; the next flush is a full redraw. Returns 0 or -1. */
int txt_grid_resize(txt_grid* g, uint32_t cols, uint32_t rows);
/* Forces the next flush to redraw every cell (after a resize or a corrupted screen). */
void txt_grid_invalidate(txt_grid* g);

void txt_grid_clear(txt_grid* g, txt_style style);
void txt_grid_put(txt_grid* g, uint32_t col, uint32_t row, uint32_t cp, txt_style style);
/* Draws UTF-8 text, clipped at the right edge and at maxcols (0 = no extra limit).
 * Returns the number of columns drawn. Invalid bytes draw U+FFFD. */
uint32_t txt_grid_text(txt_grid* g, uint32_t col, uint32_t row, const char* utf8, uint32_t maxcols,
                       txt_style style);
void txt_grid_fill(txt_grid* g, uint32_t col, uint32_t row, uint32_t w, uint32_t h, uint32_t cp,
                   txt_style style);

/* Writes the escape sequences that turn the terminal's picture into the back
 * buffer: a cursor move before every run of changed cells, SGR only when the
 * style changes, and a trailing reset when a non-default style was left active.
 * A full redraw is wrapped in synchronized output (CSI ? 2026 h ... l), except on a console grid.
 * Returns the byte count, 0 for an unchanged frame, or -1 when cap is too small
 * (then nothing is committed). txt_grid_flush_bound() is a cap that always suffices. */
long txt_grid_flush(txt_grid* g, char* out, size_t cap);
size_t txt_grid_flush_bound(const txt_grid* g);

#ifdef __cplusplus
}
#endif
#endif
