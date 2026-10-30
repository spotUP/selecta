/* Selecta_AASlider and Selecta_AAProgress (I667f, design section 3.2). Same dispatcher pattern as mui_aapanel_class.c. Every size,
 * colour and position comes from map/mui_aa.c (mui_aa_slider_*, mui_aa_progress_fill_w), drawn with aa_round_fill and held pens;
 * no colour literal, no ObtainBestPen. The slider paints track and fill (no knob) after erasing its box to the parent background. */
#include "mui_aaslider_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aaslider_data {
    int hover, drag;
    mui_aa_slider_t m;
    aa_ctx aa;
    struct MUI_EventHandlerNode ehn;
} aaslider_data;

typedef struct aaprogress_data {
    long value, max;
    mui_aa_slider_t m;
    aa_ctx aa;
} aaprogress_data;

static struct MUI_CustomClass *g_slider_class, *g_progress_class;
static uint32_t g_slider_made, g_progress_made;

static int aaslider_state(Object* obj, const aaslider_data* d) {
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    return dis ? MUI_AA_DISABLED : (d->hover || d->drag) ? MUI_AA_HOVER : MUI_AA_NORMAL;
}

static void aaslider_range(Object* obj, long* lo, long* hi, long* val) {
    ULONG v = 0;
    GetAttr(MUIA_Numeric_Min, obj, &v);
    *lo = (LONG)v;
    GetAttr(MUIA_Numeric_Max, obj, &v);
    *hi = (LONG)v;
    GetAttr(MUIA_Numeric_Value, obj, &v);
    *val = (LONG)v;
}

static ULONG aaslider_setup(struct IClass* cl, Object* obj, Msg msg) {
    aaslider_data* d = INST_DATA(cl, obj);
    int st;
    uint32_t track, fill;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_slider_metrics(d->aa.font_px, &d->m);
    for (st = MUI_AA_NORMAL; st <= MUI_AA_DISABLED; st++) {
        mui_aa_slider_rgb(d->aa.tok, st, &track, &fill);
        aa_pen(&d->aa, track);
        aa_pen(&d->aa, fill);
    }
    aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aaslider_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aaslider_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->hover = d->drag = 0;
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aaslider_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aaslider_data* d = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += d->m.min_w;
    msg->MinMaxInfo->DefWidth += d->m.min_w;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += d->m.height;
    msg->MinMaxInfo->DefHeight += d->m.height;
    msg->MinMaxInfo->MaxHeight += d->m.height;
    return 0;
}

static ULONG aaslider_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aaslider_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    ULONG active = 0;
    long lo, hi, val;
    int l, t, w, h, i, n;
    mui_aa_slider_op_t ops[MUI_AA_SLIDER_OPS];
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    aaslider_range(obj, &lo, &hi, &val);
    n = mui_aa_slider_plan(d->aa.tok, &d->m, aaslider_state(obj, d), w, h, val, lo, hi, ops);
    for (i = 0; i < n; i++) {
        if (ops[i].kind == MUI_AA_SLIDER_ERASE) {
            /* the parent background, as the super class paints it on a full draw */
            DoMethod(obj, MUIM_DrawBackground, l, t, w, h, l, t, 0);
        } else {
            aa_round_fill(rp, l + ops[i].x, t + ops[i].y, ops[i].w, ops[i].h, ops[i].radius, aa_pen(&d->aa, ops[i].rgb));
        }
    }
    if (_win(obj) != NULL) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
        if ((Object*)active == obj) {
            aa_round_frame(rp, l, t + (h - d->m.track_h) / 2, w, d->m.track_h, d->m.radius, aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]));
        }
    }
    return 0;
}

static void aaslider_move_to(Object* obj, int mouse_x) {
    long lo, hi, val, nv;
    aaslider_range(obj, &lo, &hi, &val);
    nv = mui_aa_slider_value_at(_mwidth(obj), mouse_x - _mleft(obj), lo, hi);
    if (nv != val) {
        set(obj, MUIA_Numeric_Value, (ULONG)nv);
    }
}

static ULONG aaslider_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aaslider_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    ULONG dis = 0;
    if (im == NULL || (im->Class != IDCMP_MOUSEMOVE && im->Class != IDCMP_MOUSEBUTTONS)) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    GetAttr(MUIA_Disabled, obj, &dis);
    if (im->Class == IDCMP_MOUSEMOVE) {
        int in = im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj);
        if (d->drag && !dis) {
            aaslider_move_to(obj, im->MouseX);
        }
        if (in != d->hover) {
            d->hover = in;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
        return 0;
    }
    if (im->Code == SELECTDOWN && !dis && im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj)) {
        d->drag = 1;
        aaslider_move_to(obj, im->MouseX);
        MUI_Redraw(obj, MADF_DRAWOBJECT);
        return MUI_EventHandlerRC_Eat;
    }
    if (im->Code == SELECTUP && d->drag) {
        d->drag = 0;
        MUI_Redraw(obj, MADF_DRAWOBJECT);
        return MUI_EventHandlerRC_Eat;
    }
    return 0;
}

static ULONG __attribute__((used)) aaslider_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aaslider_dispatch(void);
__asm__(".text\n.even\n.globl _aaslider_dispatch\n_aaslider_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aaslider_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aaslider_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case MUIM_Setup:
        return aaslider_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aaslider_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aaslider_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aaslider_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aaslider_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aaslider_new(const mui_aaslider_spec* sp) {
    Object* o;
    if (g_slider_class == NULL) {
        g_slider_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Numeric, NULL, sizeof(aaslider_data), (APTR)(uintptr_t)aaslider_dispatch);
    }
    if (g_slider_class == NULL) {
        return NULL;
    }
    o = NewObject(g_slider_class->mcc_Class, NULL, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_Numeric_Min, (ULONG)sp->lo, MUIA_Numeric_Max, (ULONG)sp->hi,
                  MUIA_Numeric_Value, (ULONG)sp->value, MUIA_CycleChain, (ULONG)TRUE, TAG_DONE);
    if (o != NULL) {
        if (sp->id != 0) {
            DoMethod(o, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)sp->id);
        }
        g_slider_made++;
    }
    return o;
}

/* AAProgress: a bar, TRACK with the ACCENT filled part, no knob. */
static ULONG aaprogress_setup(struct IClass* cl, Object* obj, Msg msg) {
    aaprogress_data* d = INST_DATA(cl, obj);
    uint32_t track, fill;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_slider_metrics(d->aa.font_px, &d->m);
    mui_aa_slider_rgb(d->aa.tok, MUI_AA_NORMAL, &track, &fill);
    aa_pen(&d->aa, track);
    aa_pen(&d->aa, fill);
    return TRUE;
}

static ULONG aaprogress_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aaprogress_data* d = INST_DATA(cl, obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aaprogress_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aaprogress_data* d = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += d->m.min_w;
    msg->MinMaxInfo->DefWidth += d->m.min_w;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += d->m.track_h;
    msg->MinMaxInfo->DefHeight += d->m.track_h;
    msg->MinMaxInfo->MaxHeight += d->m.track_h;
    return 0;
}

static ULONG aaprogress_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aaprogress_data* d = INST_DATA(cl, obj);
    uint32_t track, fill;
    int fw;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    mui_aa_slider_rgb(d->aa.tok, MUI_AA_NORMAL, &track, &fill);
    aa_round_fill(_rp(obj), _mleft(obj), _mtop(obj), _mwidth(obj), d->m.track_h, d->m.radius, aa_pen(&d->aa, track));
    fw = mui_aa_progress_fill_w(_mwidth(obj), d->value, d->max);
    if (fw > 0) {
        aa_round_fill(_rp(obj), _mleft(obj), _mtop(obj), fw, d->m.track_h, d->m.radius, aa_pen(&d->aa, fill));
    }
    return 0;
}

static ULONG aaprogress_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aaprogress_data* d = INST_DATA(cl, obj);
    struct TagItem *tv = FindTagItem(AAP_Value, msg->ops_AttrList), *tm = FindTagItem(AAP_Max, msg->ops_AttrList);
    int changed = 0;
    if (tm != NULL && (long)tm->ti_Data != d->max) {
        d->max = (long)tm->ti_Data;
        changed = 1;
    }
    if (tv != NULL && (long)tv->ti_Data != d->value) {
        d->value = (long)tv->ti_Data;
        changed = 1;
    }
    if (changed && _win(obj) != NULL) {
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aaprogress_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aaprogress_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == AAP_Value) {
        *msg->opg_Storage = (ULONG)d->value;
        return TRUE;
    }
    if (msg->opg_AttrID == AAP_Max) {
        *msg->opg_Storage = (ULONG)d->max;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aaprogress_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aaprogress_dispatch(void);
__asm__(".text\n.even\n.globl _aaprogress_dispatch\n_aaprogress_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aaprogress_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aaprogress_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case MUIM_Setup:
        return aaprogress_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aaprogress_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aaprogress_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aaprogress_draw(cl, obj, (struct MUIP_Draw*)msg);
    case OM_SET:
        return aaprogress_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aaprogress_get(cl, obj, (struct opGet*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aaprogress_new(long value, long max) {
    Object* o;
    if (g_progress_class == NULL) {
        g_progress_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Area, NULL, sizeof(aaprogress_data), (APTR)(uintptr_t)aaprogress_dispatch);
    }
    if (g_progress_class == NULL) {
        return NULL;
    }
    o = NewObject(g_progress_class->mcc_Class, NULL, MUIA_Frame, (ULONG)MUIV_Frame_None, TAG_DONE);
    if (o != NULL) {
        aaprogress_data* d = INST_DATA(g_progress_class->mcc_Class, o);
        d->value = value;
        d->max = max;
        g_progress_made++;
    }
    return o;
}

uint32_t mui_aaslider_made(void) {
    return g_slider_made;
}

uint32_t mui_aaprogress_made(void) {
    return g_progress_made;
}

void mui_aaslider_class_free(void) {
    if (g_slider_class != NULL) {
        MUI_DeleteCustomClass(g_slider_class);
        g_slider_class = NULL;
    }
    if (g_progress_class != NULL) {
        MUI_DeleteCustomClass(g_progress_class);
        g_progress_class = NULL;
    }
}
