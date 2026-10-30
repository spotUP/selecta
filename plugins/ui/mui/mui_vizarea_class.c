/* Selecta_VizArea (MUI-P5-3, MUI-P5-5). MUI 3.8 only: MUI_CreateCustomClass on Area, same shape as Selecta_Canvas. The step
 * (mui_vizarea_step) runs the host-tested mui_vizset_step into a heap pixel buffer sized to the object, then redraws; Draw
 * only pushes that buffer through Picasso96. Planar screens have no Picasso96: nothing renders and nothing counts. */
#include "mui_vizarea_class.h"

#include "mui_canvas_class.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <inline/macros.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>

extern struct Library* MUIMasterBase;

/* Small instance data (BOOPSI keeps the size in a UWORD); the surface and pixels live in the heap-owned mui_viz. */
typedef struct viza_data {
    mui_vizset* set;
    mui_viz* v;
    uint32_t* px;
    uint32_t pw, ph; /* size px holds a rendered frame for, 0 when none */
    uint32_t cap;    /* words allocated */
    int rtg, shown;
    struct Library* p96;
} viza_data;

typedef char viza_data_fits_uword[(sizeof(viza_data) < 65536u) ? 1 : -1];

static struct MUI_CustomClass* g_class;

static ULONG viza_new(struct IClass* cl, Object* obj, Msg msg) {
    obj = (Object*)DoSuperMethodA(cl, obj, msg);
    if (obj != NULL) {
        viza_data* d = INST_DATA(cl, obj);
        memset(d, 0, sizeof *d);
        d->v = calloc(1, sizeof *d->v);
        if (d->v == NULL) {
            CoerceMethod(cl, obj, OM_DISPOSE);
            return 0;
        }
    }
    return (ULONG)obj;
}

static ULONG viza_dispose(struct IClass* cl, Object* obj, Msg msg) {
    viza_data* d = INST_DATA(cl, obj);
    if (d->v != NULL) {
        mui_viz_release(d->v);
        free(d->v);
        d->v = NULL;
    }
    free(d->px);
    d->px = NULL;
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG viza_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    viza_data* d = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    if ((msg->flags & MADF_DRAWOBJECT) == 0 || d->p96 == NULL || d->px == NULL || d->pw != (uint32_t)_mwidth(obj) ||
        d->ph != (uint32_t)_mheight(obj)) {
        return 0;
    }
    mui_canvas_push(d->p96, d->px, (int32_t)d->pw, (int32_t)d->ph, _rp(obj), _mleft(obj), _mtop(obj));
    return 0;
}

static ULONG viza_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += 32;
    msg->MinMaxInfo->DefWidth += 160;
    msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += 32;
    msg->MinMaxInfo->DefHeight += 80;
    msg->MinMaxInfo->MaxHeight += 160;
    return 0;
}

static ULONG __attribute__((used)) viza_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG viza_dispatch(void);
__asm__(".text\n.even\n.globl _viza_dispatch\n_viza_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _viza_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) viza_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    viza_data* d;
    switch (msg->MethodID) {
    case OM_NEW:
        return viza_new(cl, obj, msg);
    case OM_DISPOSE:
        return viza_dispose(cl, obj, msg);
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg)) {
            return FALSE;
        }
        d = INST_DATA(cl, obj);
        d->p96 = OpenLibrary((CONST_STRPTR) "Picasso96API.library", 0);
        return TRUE;
    case MUIM_Cleanup:
        d = INST_DATA(cl, obj);
        if (d->p96 != NULL) {
            CloseLibrary(d->p96);
            d->p96 = NULL;
        }
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Show:
        d = INST_DATA(cl, obj);
        d->shown = 1;
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Hide:
        d = INST_DATA(cl, obj);
        d->shown = 0;
        d->pw = d->ph = 0;
        mui_viz_release(d->v);
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_AskMinMax:
        return viza_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return viza_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_vizarea_new(mui_vizset* set, const uint32_t* tokens, int screen_is_rtg) {
    Object* o;
    viza_data* d;
    if (g_class == NULL) {
        g_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Area, NULL, sizeof(viza_data), (APTR)(uintptr_t)viza_dispatch);
    }
    if (g_class == NULL) {
        return NULL;
    }
    o = NewObject(g_class->mcc_Class, NULL, MUIA_FillArea, FALSE, MUIA_Weight, 100, TAG_DONE);
    if (o != NULL) {
        d = INST_DATA(g_class->mcc_Class, o);
        d->set = set;
        d->rtg = screen_is_rtg;
        mui_viz_palette_from_tokens(d->v, tokens);
    }
    return o;
}

int mui_vizarea_step(Object* obj, const sel_viz_frame* f, const sel_uim_budget* b, uint32_t now_ms) {
    viza_data* d;
    uint32_t w, h;
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(g_class->mcc_Class, obj);
    w = (uint32_t)_mwidth(obj);
    h = (uint32_t)_mheight(obj);
    if (!d->shown || !d->rtg || d->p96 == NULL || w == 0u || h == 0u) {
        return 0;
    }
    if (w * h > d->cap) {
        uint32_t* bigger = realloc(d->px, (size_t)w * h * sizeof *d->px);
        if (bigger == NULL) {
            return 0;
        }
        d->px = bigger;
        d->cap = w * h;
    }
    if (!mui_vizset_step(d->set, d->v, f, b, now_ms, w, h, d->px)) {
        return 0;
    }
    d->pw = w;
    d->ph = h;
    MUI_Redraw(obj, MADF_DRAWOBJECT);
    return 1;
}

uint32_t mui_vizarea_frames(Object* obj) {
    return ((viza_data*)INST_DATA(g_class->mcc_Class, obj))->v->frames;
}

void mui_vizarea_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
