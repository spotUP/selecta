/* up-mcc.library functions (I685h, design 2026-11-06-mui-mcc-package.md section 8.4). Plain C over the theme store and the icon
 * table; compiled into sel_mcc_static (host tests, Selecta plugin) and into the Amiga library: one copy. Library bases are the
 * business of lib/support_libheader.c; nothing here opens a library. */
#include <mcc/up.h>

#include <selecta/uimodel/theme.h>

#include <stdint.h>
#include <string.h>

#include "map/mui_icons.h"
#include "map/mui_theme.h"
#if defined(__amigaos__) || defined(AMIGA)
#include "mui_aa_draw.h"
#endif

/* STRPTR is unsigned char on Amiga: the store takes const char*. The library's own theme file (D5: ENV:UP-MCC/). Selecta keeps
 * ENV:Selecta/theme through its own mui_theme_load call in plugins/ui/mui; the per-application file
 * ENV:UP-MCC/<application base>.theme from the design is not read yet. */
#define UP_THEME_ENV "ENV:UP-MCC/theme"

static void retheme(void) {
#if defined(__amigaos__) || defined(AMIGA)
    (void)aa_retheme_all();
#endif
}

LONG UP_ThemeReload(void) {
    txt_theme th;
    mui_theme_resolve(&th, UP_THEME_ENV, NULL);
    mui_theme_tokens_set(th.token);
    retheme();
    return 0;
}

LONG UP_ThemeLoad(STRPTR path) {
    if (mui_theme_request(UP_THEME_ENV, (const char*)path) != 0) {
        return -1;
    }
    return UP_ThemeReload();
}

LONG UP_ThemeSetTokens(const ULONG* rgb, ULONG count) {
    uint32_t tokens[SEL_GUI_TOKEN_COUNT];
    if (rgb == NULL || count == 0) {
        return -1;
    }
    memcpy(tokens, mui_theme_tokens(), sizeof tokens);
    for (ULONG i = 0; i < count && i < SEL_GUI_TOKEN_COUNT; i++) {
        tokens[i] = (uint32_t)rgb[i];
    }
    mui_theme_tokens_set(tokens);
    retheme();
    return 0;
}

const ULONG* UP_ThemeTokens(void) {
    return (const ULONG*)(const void*)mui_theme_tokens();
}

LONG UP_IconFind(STRPTR name) {
    return mui_icon_index((const char*)name);
}

LONG UP_SetFontFile(STRPTR path) {
    return mui_font_file_add((const char*)path);
}
