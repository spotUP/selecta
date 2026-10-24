/* Glyph-atlas text for the desktop UI (DUI-P2-3). Integer arithmetic only: the atlas itself is built by the desktop
 * plugin (atlas_build.c, which runs stb_truetype) and handed in as plain tables, so this file lays text out and
 * blits it with the canvas mask blit on any CPU, 68020 included. */
#ifndef SEL_GUI_TEXT_H
#define SEL_GUI_TEXT_H

#include <selecta/gui/gui_draw.h>

#include <stdint.h>

/* One glyph: its box in the alpha page, its offset from the pen (bx right, by down from the baseline) and its
 * integer advance. */
typedef struct sel_gui_glyph {
    uint32_t cp;
    uint16_t x, y, w, h;
    int16_t bx, by;
    uint16_t adv;
} sel_gui_glyph;

/* A kerning pair, applied between two consecutive code points: the pen moves by `amount` pixels. */
typedef struct sel_gui_kern {
    uint32_t a, b;
    int16_t amount;
} sel_gui_kern;

typedef struct sel_gui_atlas {
    const sel_gui_glyph* glyphs; /* sorted by cp */
    uint32_t glyph_n;
    const sel_gui_kern* kerns; /* sorted by (a, b) */
    uint32_t kern_n;
    const uint8_t* alpha; /* page_w x page_h coverage bytes */
    int32_t page_w, page_h;
    int32_t ascent;      /* pixels from the top of a line to the baseline */
    int32_t line_height; /* pixels between baselines */
    sel_gui_glyph missing; /* drawn for a code point the atlas does not hold (the font's own box) */
} sel_gui_atlas;

/* What text drawing did: non-blank glyphs blitted, and code points the atlas lacked (the missing glyph was drawn). */
typedef struct sel_gui_text_stats {
    uint32_t glyphs;
    uint32_t missing;
} sel_gui_text_stats;

/* Decodes one UTF-8 sequence at s (n bytes left); returns the code point and sets *used (at least 1). A malformed
 * sequence yields U+FFFD and consumes one byte. */
uint32_t sel_gui_utf8_next(const char* s, size_t n, size_t* used);

/* The glyph for cp, or NULL when the atlas does not hold it. */
const sel_gui_glyph* sel_gui_atlas_glyph(const sel_gui_atlas* at, uint32_t cp);

/* Pixel width of the first `len` bytes of s, kerning included. Does not count statistics. */
int32_t sel_gui_text_width(const sel_gui_atlas* at, const char* s, size_t len);

/* Draws one line with the pen at x and the top of the line at y, clipped by the canvas clip; ellipsizes at the end
 * with U+2026 (or three dots when the font lacks it) when it does not fit max_w. Returns the pixel width drawn. */
int32_t sel_gui_text_draw(sel_gui_canvas* c, const sel_gui_atlas* at, int32_t x, int32_t y, const char* s,
                          uint32_t argb, int32_t max_w, sel_gui_text_stats* st);

/* Wraps s at spaces (a word longer than the box breaks between characters) into the box, at most max_lines lines
 * of at.line_height; the last line is ellipsized when text remains. Returns the lines drawn. */
int32_t sel_gui_text_wrap(sel_gui_canvas* c, const sel_gui_atlas* at, sel_gui_rect box, const char* s,
                          uint32_t argb, int32_t max_lines, sel_gui_text_stats* st);

#endif
