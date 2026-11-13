/* Selecta_Keys (I1165, I1171). The window event handler for the multi-char action keys (Space, Enter, Home, End, M-*, C-*)
 * and the page keys (@home @library @folders @info @viz @pattern). It registers a RAWKEY handler at priority -1, so the
 * active object (string gadget, list, button) eats its own keys first; only an unhandled key reaches this class. The key
 * table is the pure one in map/mui_p8.c (mui_key_find, built from the shared action table), never a second table here.
 * A hit sends the ReturnID the matching menu item or button already sends, so mui_io_new_input -> mui_control_fire stays
 * the single dispatch. Down from the Search string moves the active object to the results list (mui_focus_route).
 * Static state (one window): BOOPSI cl_InstSize is a UWORD. */
/* The keymap base is renamed: libnix opens a library at program start when its base symbol is referenced or defined
 * (KeymapBase pulled libnix's keymap auto-open into selecta-cli, I427e3; I339 regression). This class opens and
 * closes keymap.library itself with the window, as backend_rtg.c does for its bases (eaea4ea). */
#define KeymapBase sel_mui_KeymapBase

#include "mui_keys_class.h"

#include "map/mui_map.h"
#include "selecta/uimodel/keybind.h"

#include <stdint.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <proto/muimaster.h>

extern struct Library* MUIMasterBase;
struct Library* KeymapBase; /* opened with the window, closed at cleanup */

static struct MUI_CustomClass* g_k_class;
static struct MUI_EventHandlerNode g_k_ehn;
static ULONG g_k_key_base, g_k_door_base, g_k_pane_base;
static Object* g_k_search;
static Object* g_k_list;

static ULONG __attribute__((used)) k_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG k_dispatch(void);
__asm__(".text\n.even\n.globl _k_dispatch\n_k_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _k_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

/* The key code of a RAWKEY message: the shared special-key table, else the mapped character (Ctrl/Alt stripped, as the
 * desktop input does, so C-x and M-x name the base letter). 0: no key. */
static uint32_t k_code_of(struct IntuiMessage* im) {
    uint32_t code = txt_key_from_amiga_raw(im->Code & 0x7Fu);
    if (code == 0u && KeymapBase != NULL) {
        struct InputEvent ev;
        UBYTE ch = 0;
        ev.ie_NextEvent = NULL;
        ev.ie_Class = IECLASS_RAWKEY;
        ev.ie_SubClass = 0;
        ev.ie_Code = im->Code & 0x7Fu;
        ev.ie_Qualifier = im->Qualifier & ~(UWORD)(IEQUALIFIER_CONTROL | IEQUALIFIER_LALT | IEQUALIFIER_RALT);
        ev.ie_EventAddress = NULL;
        if (MapRawKey(&ev, (STRPTR)&ch, 1, NULL) == 1) {
            code = ch;
        }
    }
    return code;
}

static ULONG __attribute__((used)) k_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    /* I1307: the key object is invisible, so it must take no layout space. MUIA_FixHeight/FixWidth 0 mean "not fixed" to MUI,
     * so the Rectangle stayed unbounded (maxh 10000) and the root VGroup gave it the spare window height: on rig 2's 800x600
     * screen it took the bottom 147 rows (layout dump c65dc052) and the body stopped short. Pin min, default and max to 0. */
    if (msg->MethodID == MUIM_AskMinMax) {
        struct MUI_MinMax* mm;
        DoSuperMethodA(cl, obj, msg);
        mm = ((struct MUIP_AskMinMax*)msg)->MinMaxInfo;
        mm->MinWidth = mm->DefWidth = mm->MaxWidth = 0;
        mm->MinHeight = mm->DefHeight = mm->MaxHeight = 0;
        return 0;
    }
    if (msg->MethodID == MUIM_Setup) {
        ULONG r = DoSuperMethodA(cl, obj, msg);
        if (r != 0) {
            g_k_ehn.ehn_Priority = -1;
            g_k_ehn.ehn_Flags = 0;
            g_k_ehn.ehn_Object = obj;
            g_k_ehn.ehn_Class = cl;
            g_k_ehn.ehn_Events = IDCMP_RAWKEY;
            DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&g_k_ehn);
            KeymapBase = OpenLibrary((CONST_STRPTR) "keymap.library", 37);
        }
        return r;
    }
    if (msg->MethodID == MUIM_Cleanup) {
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&g_k_ehn);
        if (KeymapBase != NULL) {
            CloseLibrary(KeymapBase);
            KeymapBase = NULL;
        }
        return DoSuperMethodA(cl, obj, msg);
    }
    if (msg->MethodID == MUIM_HandleEvent) {
        struct IntuiMessage* im = ((struct MUIP_HandleEvent*)msg)->imsg;
        if (im != NULL && im->Class == IDCMP_RAWKEY && (im->Code & IECODE_UP_PREFIX) == 0 && (im->Qualifier & IEQUALIFIER_REPEAT) == 0) {
            uint32_t code = k_code_of(im);
            uint32_t mods = txt_mods_from_amiga_qual(im->Qualifier);
            Object* act = NULL;
            ULONG text = 0, here = 0, there = 0;
            int text_active, search_active = 0;
            int32_t row;
            if (code == 0u) {
                return 0;
            }
            get(_win(obj), MUIA_Window_ActiveObject, &act);
            text_active = act != NULL && GetAttr(MUIA_String_Contents, act, &text) != 0;
            if (text_active && g_k_search != NULL) {
                GetAttr(MUIA_String_Contents, g_k_search, &there); /* the field forwards to its inner String: same buffer */
                here = text;
                search_active = here != 0 && here == there;
            }
            if (mui_focus_route(code, mods, search_active) == MUI_FOCUS_RESULTS && g_k_list != NULL) {
                set(_win(obj), MUIA_Window_ActiveObject, (ULONG)g_k_list);
                return MUI_EventHandlerRC_Eat;
            }
            row = mui_key_find(code, mods, text_active);
            if (row >= 0) {
                const mui_key_row* r = mui_key_row_at((uint32_t)row);
                ULONG id = r->kind == MUI_KEYROW_DOOR ? g_k_door_base + r->arg : r->kind == MUI_KEYROW_PANE ? g_k_pane_base + r->arg : g_k_key_base + (ULONG)row;
                DoMethod(_app(obj), MUIM_Application_ReturnID, id);
                return MUI_EventHandlerRC_Eat;
            }
            if (code == 27u) { /* I1354: an unbound Esc reached MUI, which closed the main window = quit with no requester */
                return MUI_EventHandlerRC_Eat;
            }
        }
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

void mui_keys_targets(Object* search, Object* list) {
    g_k_search = search;
    g_k_list = list;
}

Object* mui_keys_new(ULONG key_base, ULONG door_base, ULONG pane_base) {
    g_k_key_base = key_base;
    g_k_door_base = door_base;
    g_k_pane_base = pane_base;
    if (g_k_class == NULL) {
        g_k_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Rectangle, NULL, 0, (APTR)(uintptr_t)k_dispatch);
    }
    if (g_k_class == NULL) {
        return NULL;
    }
    return NewObject(g_k_class->mcc_Class, NULL, MUIA_FixHeight, 0, MUIA_FixWidth, 0, TAG_DONE);
}

void mui_keys_class_free(void) {
    if (g_k_class != NULL) {
        MUI_DeleteCustomClass(g_k_class);
        g_k_class = NULL;
    }
}
