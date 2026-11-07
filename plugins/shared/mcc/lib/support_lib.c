/* up-mcc.library functions (I685h, design 2026-11-06-mui-mcc-package.md section 8.4). Plain C over the theme store and the icon
 * table; compiled into sel_mcc_static (host tests, Selecta plugin) and into the Amiga library: one copy. Library bases are the
 * business of lib/support_libheader.c; nothing here opens a library. */
#include <mcc/up.h>

#include <selecta/uimodel/theme.h>

#include <stdint.h>
#include <string.h>

#include "map/mui_icons.h"
#include "map/mui_theme.h"
#include "map/up_store.h"
#if defined(__amigaos__) || defined(AMIGA)
#include "mui_aa_draw.h"

#include <clib/alib_protos.h>
#include <libraries/mui.h>
#endif

/* STRPTR is unsigned char on Amiga: the store takes const char*. The library's own theme file (D5: ENV:UP-MCC/). Selecta keeps
 * ENV:Selecta/theme through its own mui_theme_load call in plugins/ui/mui; the per-application file
 * ENV:UP-MCC/<application base>.theme from the design is not read yet. */
#define UP_THEME_ENV "ENV:UP-MCC/theme"

#if defined(__amigaos__) || defined(AMIGA)
/* I685n: a recolour reaches the objects of every process. MUI objects may only be touched from the task that runs their application
 * (MUI_Redraw from another task races the window's own drawing), so nothing is redrawn here: each registered object's application
 * gets MUIM_Application_PushMethod, which MUI documents as callable from any task, with MUIM_CallHook on the hook the object's class
 * library registered. The hook then runs on the application's own task and recolours the object there (mui_aa_draw.c). */
static void push_one(void* user, void* key, void* obj, void* app, void* hook) {
    (void)user;
    (void)obj;
    DoMethod((Object*)app, MUIM_Application_PushMethod, (ULONG)(uintptr_t)app, 3UL, MUIM_CallHook, (ULONG)(uintptr_t)hook,
             (ULONG)(uintptr_t)key);
}
#endif

static void retheme(void) {
#if defined(__amigaos__) || defined(AMIGA)
    (void)aa_retheme_all(); /* contexts of this process (the static build: Selecta's own objects) */
    up_registry_each(push_one, NULL);
#endif
}

/* Library open (lib/support_libheader.c __UserLibInit; the host test calls it with a temp file): the token store holds the theme
 * before any class draws, so a theme file in ENV:UP-MCC/ is in force without the prefs page ever running UP_ThemeReload. No retheme
 * here: nothing is registered yet at open, and a failed or missing file leaves the built-in default tokens (mui_theme_resolve). The
 * static build (Selecta) never calls this; it keeps its own mui_theme_load. env_path NULL: UP_THEME_ENV. */
void up_lib_init(const char* env_path) {
    txt_theme th;
    up_registry_init();
    mui_theme_resolve(&th, env_path != NULL ? env_path : UP_THEME_ENV, NULL);
    mui_theme_tokens_set(th.token);
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

/* I685n: the registry of the one store (src/map/up_store.c). key: the object's draw context; hook: a struct Hook* whose entry runs
 * on the application's task. */
LONG UP_ThemeAttach(void* key, void* obj, void* app, void* hook) {
    return up_registry_attach(key, obj, app, hook) == 0 ? 0 : -1;
}

LONG UP_ThemeDetach(void* key) {
    return up_registry_detach(key) == 0 ? 0 : -1;
}

ULONG UP_ThemeObjects(void) {
    return (ULONG)up_registry_count();
}
