/* Text UI theme: design tokens read from a theme file (design doc D8; no
 * hardcoded colours in the UI). Plan M1.1 phase 5, row I59. Pure C99. */
#ifndef SELECTA_TEXT_THEME_H
#define SELECTA_TEXT_THEME_H

#include <selecta/uimodel/style.h>
#include <selecta/uimodel/tokens.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Role names are the theme file keys. */
#define TXT_THEME_ROLES(X)       \
    X(NORMAL, "normal")          \
    X(HEADER, "header")          \
    X(CURSOR, "cursor")          \
    X(DIRECTORY, "directory")    \
    X(FILE, "file")              \
    X(PLAYING, "playing")        \
    X(DIM, "dim")                \
    X(LABEL, "label")            \
    X(VALUE, "value")            \
    X(BORDER, "border")          \
    X(ERROR, "error")

typedef enum txt_role {
#define X(id, name) TXT_ROLE_##id,
    TXT_THEME_ROLES(X)
#undef X
    TXT_ROLE_COUNT
} txt_role;

typedef struct txt_theme {
    txt_style role[TXT_ROLE_COUNT];
    uint32_t token[SEL_GUI_TOKEN_COUNT]; /* layer 2: role tokens (tokens.h), file keys gui.<name>; always resolved */
    uint32_t base[SEL_GUI_BASE_COUNT];   /* layer 1: base tokens, file keys gui.base.<name> (D-DUI-9) */
    int8_t ref[SEL_GUI_TOKEN_COUNT];     /* base a role follows, or -1 for a literal value */
    int8_t base_ref[SEL_GUI_BASE_COUNT]; /* base a base follows, or -1 */
    uint8_t bad_range;                   /* 1 when bad_token failed because its resolved value is outside the token's range, 0 for a missing or circular reference */
    char bad_token[64];                  /* after a failed parse that named a token: its file key, else "" */
} txt_theme;

/* Built-in dark theme. */
void txt_theme_default(txt_theme* t);
/* Applies a theme file over *t. Lines: `role = fg bg [bold] [reverse]` where fg and
 * bg are 0..255 or `default`; `#` starts a comment. Token lines: `gui.<name> = value`, decimal or 0x hex, inside the
 * token's range (tokens.h); colours are 0xRRGGBB because `#` starts a comment. A token value may also be `@gui.base.<name>`, a reference to a
 * base token, resolved here once (roles follow later base lines; a missing or circular reference fails and sets
 * t->bad_token to the offending token's file key). Returns the number of lines
 * applied, or -1 with the 1-based line number in *bad_line on the first bad line
 * (earlier lines stay applied). */
int txt_theme_parse(txt_theme* t, const char* text, int* bad_line);
const char* txt_role_name(txt_role r);

#ifdef __cplusplus
}
#endif
#endif
