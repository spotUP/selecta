/* Design tokens of the desktop UI (plan DUI-P1-10, M4 plan section 2): one X-macro lists every token with its file
 * key, default and range. The tokens live inside txt_theme (theme.h), so the one theme parser reads them. Integers
 * only: colours are 0xRRGGBB, sizes are pixels. Pure C99. */
#ifndef SELECTA_UIMODEL_TOKENS_H
#define SELECTA_UIMODEL_TOKENS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* X(id, file key, default, minimum, maximum) */
#define SEL_GUI_TOKENS(X)                                \
    X(ACCENT, "gui.accent", 0x3B82F6u, 0u, 0xFFFFFFu)    \
    X(SURFACE, "gui.surface", 0x1E1E24u, 0u, 0xFFFFFFu)  \
    X(TEXT, "gui.text", 0xE6E6EBu, 0u, 0xFFFFFFu)        \
    X(RADIUS, "gui.radius", 6u, 0u, 32u)                 \
    X(SPACING, "gui.spacing", 8u, 0u, 64u)

typedef enum sel_gui_token {
#define X(id, name, def, lo, hi) SEL_GUI_TOKEN_##id,
    SEL_GUI_TOKENS(X)
#undef X
    SEL_GUI_TOKEN_COUNT
} sel_gui_token;

const char* sel_gui_token_name(sel_gui_token t);
uint32_t sel_gui_token_default(sel_gui_token t);
uint32_t sel_gui_token_min(sel_gui_token t);
uint32_t sel_gui_token_max(sel_gui_token t);
/* Index of the token with this file key, or -1. */
int sel_gui_token_find(const char* name, uint32_t len);
/* Bit mask (1 << token) of the tokens that differ between two token arrays. */
uint32_t sel_gui_tokens_diff(const uint32_t* a, const uint32_t* b);

#ifdef __cplusplus
}
#endif
#endif
