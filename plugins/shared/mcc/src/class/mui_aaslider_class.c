/* Selecta_AASlider and Selecta_AAProgress (I667f, design section 3.2). Same dispatcher pattern as mui_aapanel_class.c. Every size,
 * colour and position comes from map/mui_aa.c (mui_aa_slider_*, mui_aa_progress_fill_w), drawn with aa_round_fill and held pens;
 * no colour literal, no ObtainBestPen. The slider paints track and fill (no knob) after erasing its box to the parent background. */
#include "mui_aaslider_class.h"
#include "mui_aa_class.h"

#include <stddef.h>
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
#include "mui_aatip.h"

extern struct Library* MUIMasterBase;

typedef struct aaslider_data {
    int hover, drag;
    int drawn_fw, drawn_w, drawn_h, drawn_state; /* I739: what the last draw left on screen (drawn_fw < 0: nothing yet) */
    mui_aa_slider_t m;
    aa_ctx aa;
    struct MUI_EventHandlerNode ehn;
    aatip_slot tip; /* I731 */
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
    d->drawn_fw = -1;
    mui_aa_slider_metrics(d->aa.font_px, d->aa.aspect_q8, &d->m);
    for (st = MUI_AA_NORMAL; st <= MUI_AA_DISABLED; st++) {
        mui_aa_slider_rgb(d->aa.tok, st, &track, &fill);
        aa_hold(&d->aa, track);
        aa_hold(&d->aa, fill);
    }
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_INACTIVEWINDOW;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aaslider_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aaslider_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->hover = d->drag = 0;
    d->tip.in = 0;
    aatip_drop(obj);
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
    {
        int st = aaslider_state(obj, d);
        /* I739: a value change alone (MADF_DRAWUPDATE, same box and state) paints only the pills, never the box erase */
        if (!(msg->flags & MADF_DRAWOBJECT) && d->drawn_fw >= 0 && d->drawn_w == w && d->drawn_h == h && d->drawn_state == st) {
            n = mui_aa_slider_update_plan(d->aa.tok, &d->m, st, w, h, d->drawn_fw, val, lo, hi, ops);
        } else {
            n = mui_aa_slider_plan(d->aa.tok, &d->m, st, w, h, val, lo, hi, ops);
        }
        d->drawn_fw = mui_aa_slider_fill_w(w, val, lo, hi);
        d->drawn_w = w;
        d->drawn_h = h;
        d->drawn_state = st;
    }
    for (i = 0; i < n; i++) {
        if (ops[i].kind == MUI_AA_SLIDER_ERASE) {
            /* the parent background, as the super class paints it on a full draw */
            aa_erase(&d->aa, obj, l, t, w, h);
        } else {
            aa_round_fill(&d->aa, rp, l + ops[i].x, t + ops[i].y, ops[i].w, ops[i].h, ops[i].radius, ops[i].rgb);
        }
    }
    if (aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
        if ((Object*)active == obj) {
            aa_round_frame(&d->aa, rp, l, t + (h - d->m.track_h) / 2, w, d->m.track_h, d->m.radius, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
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
    if (im != NULL && im->Class == IDCMP_INACTIVEWINDOW) {
        aatip_event(obj, &d->tip, im, 0);
        return 0;
    }
    if (im == NULL || (im->Class != IDCMP_MOUSEMOVE && im->Class != IDCMP_MOUSEBUTTONS)) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    GetAttr(MUIA_Disabled, obj, &dis);
    if (im->Class == IDCMP_MOUSEBUTTONS) {
        aatip_event(obj, &d->tip, im, 0);
    }
    if (im->Class == IDCMP_MOUSEMOVE) {
        int in = im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj);
        aatip_event(obj, &d->tip, im, in);
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
const struct aa_class_desc aaslider_class_desc = {MUIC_Numeric, sizeof(aaslider_data), aaslider_dispatch, &g_slider_class};
__asm__(".text\n.even\n.globl _aaslider_dispatch\n_aaslider_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aaslider_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aaslider_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
    case OM_SET:
        return aatip_super_set(cl, obj, (struct opSet*)msg, offsetof(aaslider_data, tip));
    case OM_GET:
        if (aatip_get((struct opGet*)msg, &((aaslider_data*)INST_DATA(cl, obj))->tip)) {
            return TRUE;
        }
        break;
    case UPM_Shared_Fire: {
        aaslider_data* d = INST_DATA(cl, obj);
        return aatip_fire(obj, &d->aa, &d->tip);
    }
    case MUIM_Hide:
        aatip_drop(obj);
        break;
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

/* Tags: MUIA_Numeric_Min/Max/Value and MUIA_FixWidth are the Numeric class's own and pass through. Defaults: no frame, on the cycle chain. */
Object* UP_NewSlider(const struct TagItem* tags) {
    Object* o;
    struct TagItem def[3];
    if (g_slider_class == NULL) {
        g_slider_class = AA_CLASS_CREATE(&aaslider_class_desc);
    }
    if (g_slider_class == NULL) {
        return NULL;
    }
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = MUIA_CycleChain, def[1].ti_Data = (ULONG)TRUE;
    def[2].ti_Tag = TAG_MORE, def[2].ti_Data = (ULONG)(uintptr_t)tags;
    o = NewObjectA(g_slider_class->mcc_Class, NULL, def);
    if (o != NULL) {
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
    mui_aa_slider_metrics(d->aa.font_px, d->aa.aspect_q8, &d->m);
    mui_aa_slider_rgb(d->aa.tok, MUI_AA_NORMAL, &track, &fill);
    aa_hold(&d->aa, track);
    aa_hold(&d->aa, fill);
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
    aa_round_fill(&d->aa, _rp(obj), _mleft(obj), _mtop(obj), _mwidth(obj), d->m.track_h, d->m.radius, track);
    fw = mui_aa_progress_fill_w(_mwidth(obj), d->value, d->max);
    if (fw > 0) {
        aa_round_fill(&d->aa, _rp(obj), _mleft(obj), _mtop(obj), fw, d->m.track_h, d->m.radius, fill);
    }
    return 0;
}

static ULONG aaprogress_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aaprogress_data* d = INST_DATA(cl, obj);
    struct TagItem *tv = FindTagItem(UPA_Progress_Value, msg->ops_AttrList), *tm = FindTagItem(UPA_Progress_Max, msg->ops_AttrList);
    int changed = 0;
    if (tm != NULL && (long)tm->ti_Data != d->max) {
        d->max = (long)tm->ti_Data;
        changed = 1;
    }
    if (tv != NULL && (long)tv->ti_Data != d->value) {
        d->value = (long)tv->ti_Data;
        changed = 1;
    }
    if (changed && aa_live(obj)) {
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aaprogress_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aaprogress_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == UPA_Progress_Value) {
        *msg->opg_Storage = (ULONG)d->value;
        return TRUE;
    }
    if (msg->opg_AttrID == UPA_Progress_Max) {
        *msg->opg_Storage = (ULONG)d->max;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aaprogress_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

/* OM_NEW: UPA_Progress_* parsed once for UP_NewProgress and a by-name MUI_NewObject("UP-Progress.mcc"). */
static ULONG aaprogress_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aaprogress_data* d;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)msg);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    d->value = (long)GetTagData(UPA_Progress_Value, 0, msg->ops_AttrList);
    d->max = (long)GetTagData(UPA_Progress_Max, 100, msg->ops_AttrList);
    return (ULONG)obj;
}

extern ULONG aaprogress_dispatch(void);
const struct aa_class_desc aaprogress_class_desc = {MUIC_Area, sizeof(aaprogress_data), aaprogress_dispatch, &g_progress_class};
__asm__(".text\n.even\n.globl _aaprogress_dispatch\n_aaprogress_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aaprogress_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aaprogress_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aaprogress_new(cl, obj, (struct opSet*)msg);
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

Object* UP_NewProgress(const struct TagItem* tags) {
    Object* o;
    struct TagItem def[2];
    if (g_progress_class == NULL) {
        g_progress_class = AA_CLASS_CREATE(&aaprogress_class_desc);
    }
    if (g_progress_class == NULL) {
        return NULL;
    }
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = TAG_MORE, def[1].ti_Data = (ULONG)(uintptr_t)tags;
    o = NewObjectA(g_progress_class->mcc_Class, NULL, def);
    if (o != NULL) {
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
