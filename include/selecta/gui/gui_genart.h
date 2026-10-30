/* Generated artwork and platform covers for songs without a cover (design doc 2.7, DUI-P2-5; I691 design
 * thoughts/shared/research/2026-10-29-i691-platform-covers.md sections 1, 2, 7). A cover is a flat field of the
 * platform family's token colour (one of three tones by title hash), the family mark top right and, on a cover of
 * 64 px or more with a font, the family name bottom left. Integer only. */
#ifndef SELECTA_GUI_GENART_H
#define SELECTA_GUI_GENART_H

#include <selecta/gui/gui_draw.h>
#include <selecta/platform.h>
#include <selecta/gui/gui_text.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FNV-1a of a NUL-terminated string. */
uint32_t sel_gui_str_hash(const char* s);

/* Tones per family in the cover palette and on an INDEX8 canvas: 3 when SEL_PLATFORM_FAMILY_COUNT * 3 colours plus the
 * text colour fit the 32 artwork palette slots, else 1 (every family keeps its base tone, table order). */
uint32_t sel_gui_cover_aga_tones(void);
/* Colour (0xRRGGBB) of a family's tone: 0 base, 1 darker (12% toward black), 2 lighter (12% toward white).
 * `tokens` is the SEL_GUI_TOKEN_COUNT role token array. */
uint32_t sel_gui_cover_tone(const uint32_t* tokens, uint32_t family, uint32_t tone);
/* Which tone (0..2) a title hash picks. */
uint32_t sel_gui_cover_tone_of(uint32_t title_hash);
/* Fills the 32 artwork palette slots of the covers (INDEX8 entries 32..63): tone-major per family
 * (index family * tones + tone), then the text colour; unused slots repeat the text colour. Returns the tones per family. */
uint32_t sel_gui_cover_palette(uint32_t out[32], const uint32_t* tokens);
/* Draws the cover of `family` into `r`. `label_font` NULL draws no name. */
void sel_gui_cover_draw(sel_gui_canvas* c, sel_gui_rect r, uint32_t family, uint32_t title_hash, const uint32_t* tokens,
                        const sel_gui_atlas* label_font);

#ifdef __cplusplus
}
#endif

#endif /* SELECTA_GUI_GENART_H */
