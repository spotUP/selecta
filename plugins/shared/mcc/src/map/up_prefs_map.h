/* I685j: the pure half of the UP-MCC.mcp prefs page (prefs/up_prefs.c). Maps the theme tokens to the values of the page's gadgets
 * and back, so the mapping is host-testable and the MUI class holds only gadget plumbing. Pure C99. */
#ifndef UP_PREFS_MAP_H
#define UP_PREFS_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The page's values: four colour string gadgets ("RRGGBB", upper case) and the corner radius slider. */
typedef struct up_prefs_gadgets {
    char accent[12];
    char surface[12];
    char text[12];
    char background[12];
    int radius;
} up_prefs_gadgets;

/* tokens: SEL_GUI_TOKEN_COUNT entries (UPTOK order). */
void up_prefs_tokens_to_gadgets(const uint32_t* tokens, up_prefs_gadgets* g);
/* Writes the gadget values over *tokens (other tokens stay). A colour is 6 hex digits, optionally with a 0x or # prefix, any case;
 * radius is clamped to the token range. Returns 0, or -1 with *tokens untouched when a colour string is not valid. */
int up_prefs_gadgets_to_tokens(const up_prefs_gadgets* g, uint32_t* tokens);

#ifdef __cplusplus
}
#endif
#endif
