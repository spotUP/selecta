/* Generated artwork and platform covers for songs without a cover (design doc 2.7, DUI-P2-5; I691 design
 * thoughts/shared/research/2026-10-29-i691-platform-covers.md sections 1, 2, 7). A cover is a flat field of the
 * platform family's token colour (one of three tones by title hash), the family mark top right and, on a cover of
 * 64 px or more with a font, the family name bottom left. Integer only. */
#ifndef SELECTA_GUI_GENART_H
#define SELECTA_GUI_GENART_H

#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_text.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FNV-1a of a NUL-terminated string. */
uint32_t sel_gui_str_hash(const char* s);

/* Platform families (I691a). Each console that has formats of its own is its own family; SEL_GUI_FAMILY_CONSOLE is
 * for consoles with too few formats to stand alone. The order of 1..12 is the order of the gui.cover.* tokens
 * (SEL_GUI_TOKEN_COVER_AMIGA ... SEL_GUI_TOKEN_COVER_CONSOLE); the music family uses the ACCENT token. */
enum {
    SEL_GUI_FAMILY_MUSIC = 0,
    SEL_GUI_FAMILY_AMIGA,
    SEL_GUI_FAMILY_COMMODORE_64,
    SEL_GUI_FAMILY_ATARI,
    SEL_GUI_FAMILY_ZX_SPECTRUM,
    SEL_GUI_FAMILY_PC_TRACKER,
    SEL_GUI_FAMILY_NES,
    SEL_GUI_FAMILY_GAME_BOY,
    SEL_GUI_FAMILY_SNES,
    SEL_GUI_FAMILY_PC_ENGINE,
    SEL_GUI_FAMILY_MSX,
    SEL_GUI_FAMILY_MEGA_DRIVE,
    SEL_GUI_FAMILY_CONSOLE,
    SEL_GUI_FAMILY_COUNT
};

/* The family of a file extension without the dot, any case; SEL_GUI_FAMILY_MUSIC when unknown. */
uint32_t sel_gui_platform_of_ext(const char* ext);
/* The family of a file name: its last extension ("x.mod"), else its Amiga prefix, the part before the first dot
 * ("mod.x"; the extension table plus the explicit UADE prefix list), case-insensitively. Music when neither is known. */
uint32_t sel_gui_platform_of_name(const char* name);
/* Number of rows of the one extension table (for tests). */
uint32_t sel_gui_platform_ext_count(void);
/* The UADE file name prefixes ("dum" of "dum.song") that map to the Amiga family. */
uint32_t sel_gui_platform_uade_prefix_count(void);
const char* sel_gui_platform_uade_prefix(uint32_t i);
/* The family's name in full English words ("Commodore 64", "Game Boy") and its lower case id ("game_boy"). */
const char* sel_gui_platform_label(uint32_t family);
const char* sel_gui_platform_id(uint32_t family);

/* The chip a format plays on: "paula", "sid", "ay", "2a03", "software". Derived from the family. */
const char* sel_gui_genart_chip(const char* format);

/* Tones per family in the cover palette and on an INDEX8 canvas: 3 when SEL_GUI_FAMILY_COUNT * 3 colours plus the
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

/* Legacy gradient art, replaced by sel_gui_cover_draw at the call sites in I691b-e and removed in I691e. */
/* The 32 colours the generated art is drawn from, for the INDEX8 budget: 16 hues at the gradient's dark and bright ends. */
void sel_gui_genart_palette(uint32_t out[32]);
void sel_gui_genart_draw(sel_gui_canvas* c, sel_gui_rect r, const char* format, uint32_t title_hash);

#ifdef __cplusplus
}
#endif

#endif /* SELECTA_GUI_GENART_H */
