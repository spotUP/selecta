/* Amiga MUI UI plugin: descriptor and hooks (plan 2026-10-25-amiga-mui.md MUI-P1-2). Same shape as the text and
 * desktop UI plugins. The MUI calls (MUI-P1-3) compile only on the Amiga with the MUI 3.8 headers; on the host this
 * file builds for the descriptor test and open always refuses. */
#include <selecta/engine.h>
#include <selecta/plugin.h>
#include <selecta/ui.h>

#include <stdint.h>
#include <string.h>

#include "map/mui_map.h"

#if defined(__amigaos__) || defined(AMIGA)
#define SEL_MUI_NATIVE 1
#include <exec/types.h>
#include <libraries/mui.h>
#include <clib/muimaster_protos.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
struct Library* MUIMasterBase;
#define SEL_MAKE_ID(a, b, c, d) (((ULONG)(a) << 24) | ((ULONG)(b) << 16) | ((ULONG)(c) << 8) | (ULONG)(d))

/* The SDK's sfdc inline MUI_NewObject is a function-like macro; the SDK's ApplicationObject/End idiom nests object-like
 * macros that end in an open call, which a function-like macro cannot span. So the varargs MUI_NewObject comes from
 * the toolchain's libmui.a (clib prototype, out-of-line stub that hands the caller's stacked tags to MUI_NewObjectA).
 * A local static varargs wrapper reading &tag1 is wrong: at -O3 gcc inlines it, &tag1 then points at a lone copy of
 * the first tag, the rest of the list is stack garbage, and the Window object never opens (I508). */
#undef MUI_NewObject
#endif

typedef struct mui_state {
    uint32_t opens; /* ui_open_mui sentinel (MUI-P1-5): windows opened */
    sel_tag stats[2];
#ifdef SEL_MUI_NATIVE
    Object* app;
    Object* win;
#endif
} mui_state;

static mui_state g_state;
static sel_tag g_hooks[8];
static sel_tag g_desc[5];

static void put_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

#ifdef SEL_MUI_NATIVE
/* MUI-P1-3: muimaster.library v19 (MUI 3.8), one Application with one Window. The sentinel counts after the window
 * is open (MUI-P1-5), so ui_open_mui=1 proves the window really opened. */
static void* mui_open(const sel_ui_open_args* a) {
    mui_state* s = &g_state;
    ULONG open = 0;
    (void)a;
    MUIMasterBase = OpenLibrary((CONST_STRPTR)MUIMASTER_NAME, 19);
    if (MUIMasterBase == NULL) {
        return NULL;
    }
    s->app = ApplicationObject, MUIA_Application_Title, (ULONG) "Selecta", MUIA_Application_Base, (ULONG) "SELECTA",
        MUIA_Application_Version, (ULONG) "$VER: Selecta MUI 1.0",
        SubWindow, s->win = WindowObject, MUIA_Window_Title, (ULONG) "Selecta", MUIA_Window_ID, SEL_MAKE_ID('S', 'E', 'L', 'M'),
            WindowContents, VGroup, Child, TextObject, MUIA_Text_Contents, (ULONG) "Selecta", End, End, End, End;
    if (s->app == NULL) {
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
        return NULL;
    }
    DoMethod(s->win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, s->app, 2, MUIM_Application_ReturnID,
             MUIV_Application_ReturnID_Quit);
    set(s->win, MUIA_Window_Open, TRUE);
    GetAttr(MUIA_Window_Open, s->win, &open); /* MUI opens the window inside the set; 0 here means it failed */
    if (!open) {
        MUI_DisposeObject(s->app);
        s->app = NULL;
        s->win = NULL;
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
        return NULL;
    }
    s->opens++;
    return s;
}

static int mui_wait(void* state, uint32_t timeout_ms) {
    mui_state* s = (mui_state*)state;
    ULONG sigs = 0;
    LONG id;
    (void)timeout_ms;
    id = (LONG)DoMethod(s->app, MUIM_Application_NewInput, &sigs);
    if (id == (LONG)MUIV_Application_ReturnID_Quit) {
        return -1;
    }
    if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) { /* Break ends the UI like the close gadget */
        return -1;
    }
    Delay(1);
    return id != 0 ? 1 : 0;
}

static void mui_close(void* state) {
    mui_state* s = (mui_state*)state;
    if (s->app != NULL) {
        MUI_DisposeObject(s->app); /* closes the window and frees the children */
        s->app = NULL;
        s->win = NULL;
    }
    if (MUIMasterBase != NULL) {
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
    }
}
#else
static void* mui_open(const sel_ui_open_args* a) {
    (void)a;
    return NULL; /* the host has no MUI: the shell keeps its current UI */
}

static int mui_wait(void* state, uint32_t timeout_ms) {
    (void)state;
    (void)timeout_ms;
    return -1;
}

static void mui_close(void* state) {
    (void)state;
}
#endif

static const sel_tag* mui_stats(void* state) {
    mui_state* s = &g_state; /* one instance per process */
    (void)state;
    s->stats[0].tag = SEL_UI_STAT_OPEN_MUI;
    s->stats[0].value = s->opens;
    s->stats[1].tag = SEL_TAG_END;
    s->stats[1].value = 0;
    return s->stats;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    memset(&g_state, 0, sizeof g_state);
    put_tag(&g_hooks[0], SEL_UI_NAME, (uintptr_t) "MUI");
    put_tag(&g_hooks[1], SEL_UI_OPEN, SEL_TAG_FN(mui_open));
    put_tag(&g_hooks[2], SEL_UI_WAIT, SEL_TAG_FN(mui_wait));
    put_tag(&g_hooks[3], SEL_UI_CLOSE, SEL_TAG_FN(mui_close));
    put_tag(&g_hooks[4], SEL_UI_STATS, SEL_TAG_FN(mui_stats));
    /* Planar-safe mask: the descriptor is static, an RTG screen widens it when the window opens (MUI-P6). */
    put_tag(&g_hooks[5], SEL_UI_VIEW_STYLES, mui_style_mask(0));
    put_tag(&g_hooks[6], SEL_TAG_END, 0);

    put_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_UI);
    put_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "mui");
    put_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    put_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    put_tag(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
