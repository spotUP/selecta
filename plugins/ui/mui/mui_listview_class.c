/* Selecta_Listview (MUI-P4-6, I558). With the pointer over the song Listview the Amiga-1..5 Song menu shortcuts never reach a
 * Menuitem Notify (measured on rig 3: ui_p4 songmenu=0 over the list, 3 off it). Intuition delivers them to the window as
 * IDCMP_RAWKEY (right Amiga + 1..5), not as IDCMP_MENUPICK. The class registers a window event handler for RAWKEY and, when
 * the pointer is inside the list, sends MUIM_Application_ReturnID with the same CTL_SM id the window strip sends, so
 * mui_io_new_input -> mui_control_fire stays the single dispatch. The handler node is static (one song Listview), so the
 * class needs no instance data (BOOPSI cl_InstSize is a UWORD). */
#include "mui_listview_class.h"

#include "mui_aa_draw.h"

#include <stdint.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

extern struct Library* MUIMasterBase;

#define LV_KEY_1 0x01 /* rawkey code of the 1 key, 2..5 follow */

static struct MUI_CustomClass* g_lv_class;
static struct MUI_EventHandlerNode g_lv_ehn; /* one song Listview per process */
static ULONG g_lv_sm_base; /* ReturnID of Song menu item 0, from mui_ui.c CTL_SM_BASE */
static ULONG g_lv_sm_count;

static ULONG __attribute__((used)) lv_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG lv_dispatch(void);
__asm__(".text\n.even\n.globl _lv_dispatch\n_lv_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _lv_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) lv_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    if (msg->MethodID == MUIM_Setup) {
        ULONG r = DoSuperMethodA(cl, obj, msg);
        if (r != 0 && aa_live(obj)) {
            g_lv_ehn.ehn_Priority = 0;
            g_lv_ehn.ehn_Flags = 0;
            g_lv_ehn.ehn_Object = obj;
            g_lv_ehn.ehn_Class = cl;
            g_lv_ehn.ehn_Events = IDCMP_RAWKEY;
            DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&g_lv_ehn);
        }
        return r;
    }
    if (msg->MethodID == MUIM_Cleanup) {
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&g_lv_ehn);
        return DoSuperMethodA(cl, obj, msg);
    }
    if (msg->MethodID == MUIM_HandleEvent) {
        struct IntuiMessage* im = ((struct MUIP_HandleEvent*)msg)->imsg;
        /* Measured on rig 3: Amiga-1 over the list arrives as IDCMP_RAWKEY (right Amiga 0x67, then code 1 with
         * IEQUALIFIER_RCOMMAND), never as IDCMP_MENUPICK. Off the list Intuition turns the same keys into MENUPICK of the window strip. */
        if (im != NULL && im->Class == IDCMP_RAWKEY && (im->Qualifier & IEQUALIFIER_RCOMMAND) != 0 && (im->Qualifier & IEQUALIFIER_REPEAT) == 0 &&
            im->Code >= LV_KEY_1 && im->Code < LV_KEY_1 + g_lv_sm_count && im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj)) {
            DoMethod(_app(obj), MUIM_Application_ReturnID, g_lv_sm_base + (ULONG)(im->Code - LV_KEY_1));
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_listview_new(Object* list, ULONG sm_base, ULONG sm_count) {
    g_lv_sm_base = sm_base;
    g_lv_sm_count = sm_count;
    if (g_lv_class == NULL) {
        g_lv_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Listview, NULL, 0, (APTR)(uintptr_t)lv_dispatch);
    }
    if (g_lv_class == NULL) {
        return NULL;
    }
    return NewObject(g_lv_class->mcc_Class, NULL, MUIA_Listview_List, (ULONG)list, TAG_DONE);
}

void mui_listview_class_free(void) {
    if (g_lv_class != NULL) {
        MUI_DeleteCustomClass(g_lv_class);
        g_lv_class = NULL;
    }
}
