/* Generated artwork for songs without a cover (design doc 2.7, DUI-P2-5): a gradient and a format glyph, fully
 * determined by format, chip and title hash. Integer only. */
#ifndef SELECTA_GUI_GENART_H
#define SELECTA_GUI_GENART_H

#include <selecta/gui/gui_draw.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FNV-1a of a NUL-terminated string. */
uint32_t sel_gui_str_hash(const char* s);
/* The chip a format plays on: "paula" (mod, ahx), "sid", "ay", "2a03", "software" (xm, it, s3m, others). The
 * format is a file extension without the dot, any case. */
const char* sel_gui_genart_chip(const char* format);
/* Fills `r` with the generated tile of (format, chip of the format, title_hash). */
/* The 32 colours the generated art is drawn from, for the INDEX8 budget: 16 hues at the gradient's dark and bright ends. */
void sel_gui_genart_palette(uint32_t out[32]);
void sel_gui_genart_draw(sel_gui_canvas* c, sel_gui_rect r, const char* format, uint32_t title_hash);

#ifdef __cplusplus
}
#endif

#endif /* SELECTA_GUI_GENART_H */
