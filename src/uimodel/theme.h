/* Text UI theme: design tokens read from a theme file (design doc D8; no
 * hardcoded colours in the UI). Plan M1.1 phase 5, row I59. Pure C99. */
#ifndef SELECTA_TEXT_THEME_H
#define SELECTA_TEXT_THEME_H

#include "style.h"

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
} txt_theme;

/* Built-in dark theme. */
void txt_theme_default(txt_theme* t);
/* Applies a theme file over *t. Lines: `role = fg bg [bold] [reverse]` where fg and
 * bg are 0..255 or `default`; `#` starts a comment. Returns the number of lines
 * applied, or -1 with the 1-based line number in *bad_line on the first bad line
 * (earlier lines stay applied). */
int txt_theme_parse(txt_theme* t, const char* text, int* bad_line);
const char* txt_role_name(txt_role r);

#ifdef __cplusplus
}
#endif
#endif
