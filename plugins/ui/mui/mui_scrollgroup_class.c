/* Selecta_AAScrollGroup (I1351). A Group subclass holding a Virtgroup (the contents) and an AAScroller, the pairing mui_aalist_class.c
 * uses for a list: the scroller's First notifies the Virtgroup's Top, and after every draw the scroller is set (MUIA_NoNotify) from the
 * Virtgroup's total height, the visible height and its Top. Units are pixels. The wheel moves Top while the pointer is over the group. */
#include "mui_scrollgroup_class.h"

#include "map/mui_map.h"
#include "mui_aa_draw.h"

#include <stdint.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

extern struct Library* MUIMasterBase;

#define SG_WHEEL_UP 0x7A
#define SG_WHEEL_DOWN 0x7B
#define SG_WHEEL_LINES 3

typedef struct {
    Object* vg;
    Object* scroller;
    struct MUI_EventHandlerNode ehn;
} sg_data;

static struct MUI_CustomClass* g_sg_class;

static ULONG __attribute__((used)) sg_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG sg_dispatch(void);
__asm__(".text\n.even\n.globl _sg_dispatch\n_sg_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _sg_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static void sg_geom(sg_data* d, long* total, long* vis, long* top) {
    ULONG h = 0, t = 0;
    GetAttr(MUIA_Virtgroup_Height, d->vg, &h);
    GetAttr(MUIA_Virtgroup_Top, d->vg, &t);
    *total = (long)h;
    *vis = (long)_mheight(d->vg);
    *top = (long)t;
}

static void sg_sync(sg_data* d) {
    long total, vis, top;
    sg_geom(d, &total, &vis, &top);
    if (vis <= 0) {
        return;
    }
    SetAttrs(d->scroller, MUIA_NoNotify, TRUE, UPA_Scroller_Visible, (ULONG)vis, UPA_Scroller_Entries, (ULONG)total, UPA_Scroller_First, (ULONG)top, TAG_DONE);
}

static ULONG sg_setup(struct IClass* cl, Object* obj, Msg msg) {
    sg_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_RAWKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG sg_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    sg_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG sg_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    ULONG r = DoSuperMethodA(cl, obj, (Msg)msg);
    if (msg->flags & MADF_DRAWOBJECT) {
        sg_sync(INST_DATA(cl, obj));
    }
    return r;
}

static ULONG sg_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    sg_data* d = INST_DATA(cl, obj);
    const struct IntuiMessage* im = msg->imsg;
    if (im == NULL || im->Class != IDCMP_RAWKEY || (im->Code != SG_WHEEL_UP && im->Code != SG_WHEEL_DOWN)) {
        return 0;
    }
    if (im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj)) {
        long total, vis, top, step, nt;
        sg_geom(d, &total, &vis, &top);
        step = (long)_font(obj)->tf_YSize * SG_WHEEL_LINES;
        nt = top + (im->Code == SG_WHEEL_UP ? -step : step);
        if (nt > total - vis) {
            nt = total - vis;
        }
        if (nt < 0) {
            nt = 0;
        }
        if (nt != top) {
            set(d->vg, MUIA_Virtgroup_Top, (ULONG)nt);
            sg_sync(d);
        }
        return MUI_EventHandlerRC_Eat;
    }
    return 0;
}

static ULONG __attribute__((used)) sg_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case MUIM_Setup:
        return sg_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return sg_cleanup(cl, obj, msg);
    case MUIM_Draw:
        return sg_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return sg_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aa_scrollgroup_new(Object* contents, int hidden) {
    Object *vg, *scroller, *grp;
    sg_data* d;
    if (contents == NULL) {
        return NULL;
    }
    if (g_sg_class == NULL) {
        g_sg_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Group, NULL, sizeof(sg_data), (APTR)(uintptr_t)sg_dispatch);
    }
    vg = g_sg_class != NULL ? MUI_NewObject((char*)MUIC_Virtgroup, MUIA_Group_Child, (ULONG)contents, TAG_DONE) : NULL;
    scroller = vg != NULL ? UP_NewScroller(NULL) : NULL;
    grp = scroller != NULL ? NewObject(g_sg_class->mcc_Class, NULL, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 0, MUIA_Group_HorizSpacing, 0,
                                       MUIA_Group_Child, (ULONG)vg, MUIA_Group_Child, (ULONG)scroller, MUIA_ShowMe, (ULONG)(hidden ? FALSE : TRUE), TAG_DONE)
                           : NULL;
    if (grp == NULL) {
        if (scroller != NULL) {
            MUI_DisposeObject(scroller);
        }
        if (vg != NULL) {
            MUI_DisposeObject(vg); /* takes contents with it */
        } else {
            MUI_DisposeObject(contents);
        }
        return NULL;
    }
    d = INST_DATA(g_sg_class->mcc_Class, grp);
    d->vg = vg;
    d->scroller = scroller;
    DoMethod(scroller, MUIM_Notify, UPA_Scroller_First, MUIV_EveryTime, vg, 3, MUIM_Set, MUIA_Virtgroup_Top, MUIV_TriggerValue);
    return grp;
}

void mui_scrollgroup_class_free(void) {
    if (g_sg_class != NULL) {
        MUI_DeleteCustomClass(g_sg_class);
        g_sg_class = NULL;
    }
}
