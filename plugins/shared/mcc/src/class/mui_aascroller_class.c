/* Selecta_AAScroller (I667i, design section 3.5). Same dispatcher pattern as mui_aacycle_class.c. Geometry and colours come from
 * map/mui_aa.c (mui_aa_scroller_*); drawn with aa_round_fill and held pens; no colour literal, no ObtainBestPen. */
#include "mui_aascroller_class.h"
#include "mui_aa_class.h"

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

typedef struct aascroller_data {
    long first, visible, entries;
    int hover, drag, grab;
    mui_aa_scroller_t m;
    aa_ctx aa;
    struct MUI_EventHandlerNode ehn;
} aascroller_data;

static struct MUI_CustomClass* g_scroller_class;
static uint32_t g_scroller_made;

static int aascroller_state(const aascroller_data* d) {
    return d->drag ? MUI_AA_PRESSED : d->hover ? MUI_AA_HOVER : MUI_AA_NORMAL;
}

static ULONG aascroller_setup(struct IClass* cl, Object* obj, Msg msg) {
    aascroller_data* d = INST_DATA(cl, obj);
    int st;
    uint32_t track, thumb;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_scroller_metrics(d->aa.font_px, d->aa.aspect_q8, &d->m);
    for (st = MUI_AA_NORMAL; st <= MUI_AA_PRESSED; st++) {
        mui_aa_scroller_rgb(d->aa.tok, st, &track, &thumb);
        aa_hold(&d->aa, track);
        aa_hold(&d->aa, thumb);
    }
    d->hover = d->drag = 0;
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aascroller_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aascroller_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->hover = d->drag = 0;
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aascroller_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aascroller_data* d = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += d->m.width;
    msg->MinMaxInfo->DefWidth += d->m.width;
    msg->MinMaxInfo->MaxWidth += d->m.width;
    msg->MinMaxInfo->MinHeight += d->m.min_h;
    msg->MinMaxInfo->DefHeight += d->m.min_h;
    msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    return 0;
}

static ULONG aascroller_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aascroller_data* d = INST_DATA(cl, obj);
    int ty, th;
    uint32_t track, thumb;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    mui_aa_scroller_rgb(d->aa.tok, aascroller_state(d), &track, &thumb);
    aa_round_fill(&d->aa, _rp(obj), _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj), 0, track);
    if (mui_aa_scroller_geom(&d->m, _mheight(obj), d->first, d->visible, d->entries, &ty, &th)) {
        aa_round_fill(&d->aa, _rp(obj), _mleft(obj), _mtop(obj) + ty, _mwidth(obj), th, d->m.radius, thumb);
    }
    return 0;
}

static int aascroller_in(Object* obj, const struct IntuiMessage* im) {
    return im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj);
}

static ULONG aascroller_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aascroller_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    int ty, th, in;
    if (im == NULL || (im->Class != IDCMP_MOUSEMOVE && im->Class != IDCMP_MOUSEBUTTONS)) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    in = aascroller_in(obj, im);
    if (im->Class == IDCMP_MOUSEMOVE) {
        if (d->drag) {
            long nf = mui_aa_scroller_first_at(&d->m, _mheight(obj), d->grab, im->MouseY - _mtop(obj), d->visible, d->entries);
            if (nf != d->first) {
                set(obj, UPA_Scroller_First, (ULONG)nf);
            }
        }
        if (in != d->hover) {
            d->hover = in;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
        return 0;
    }
    if (im->Code == SELECTDOWN && in && mui_aa_scroller_geom(&d->m, _mheight(obj), d->first, d->visible, d->entries, &ty, &th)) {
        int y = im->MouseY - _mtop(obj);
        if (y >= ty && y < ty + th) {
            d->drag = 1;
            d->grab = y - ty;
        } else { /* a click on the track pages towards the pointer */
            long nf = d->first + (y < ty ? -d->visible : d->visible);
            nf = nf < 0 ? 0 : nf > d->entries - d->visible ? d->entries - d->visible : nf;
            set(obj, UPA_Scroller_First, (ULONG)nf);
        }
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

static ULONG aascroller_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aascroller_data* d = INST_DATA(cl, obj);
    struct TagItem *tf = FindTagItem(UPA_Scroller_First, msg->ops_AttrList), *tv = FindTagItem(UPA_Scroller_Visible, msg->ops_AttrList),
                   *te = FindTagItem(UPA_Scroller_Entries, msg->ops_AttrList);
    int changed = 0;
    if (tf != NULL && (long)tf->ti_Data != d->first) {
        d->first = (long)tf->ti_Data;
        changed = 1;
    }
    if (tv != NULL && (long)tv->ti_Data != d->visible) {
        d->visible = (long)tv->ti_Data;
        changed = 1;
    }
    if (te != NULL && (long)te->ti_Data != d->entries) {
        d->entries = (long)te->ti_Data;
        changed = 1;
    }
    if (changed && aa_live(obj)) {
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aascroller_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aascroller_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == UPA_Scroller_First) {
        *msg->opg_Storage = (ULONG)d->first;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aascroller_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

/* OM_NEW: UPA_Scroller_* parsed once for UP_NewScroller and a by-name MUI_NewObject("UP-Scroller.mcc"). */
static ULONG aascroller_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aascroller_data* d;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)msg);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    d->first = (long)GetTagData(UPA_Scroller_First, 0, msg->ops_AttrList);
    d->visible = (long)GetTagData(UPA_Scroller_Visible, 0, msg->ops_AttrList);
    d->entries = (long)GetTagData(UPA_Scroller_Entries, 0, msg->ops_AttrList);
    return (ULONG)obj;
}

extern ULONG aascroller_dispatch(void);
const struct aa_class_desc aascroller_class_desc = {MUIC_Area, sizeof(aascroller_data), aascroller_dispatch, &g_scroller_class};
__asm__(".text\n.even\n.globl _aascroller_dispatch\n_aascroller_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aascroller_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aascroller_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aascroller_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aascroller_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aascroller_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        return aascroller_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aascroller_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aascroller_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aascroller_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aascroller_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewScroller(const struct TagItem* tags) {
    Object* o;
    struct TagItem def[2];
    if (g_scroller_class == NULL) {
        g_scroller_class = AA_CLASS_CREATE(&aascroller_class_desc);
    }
    if (g_scroller_class == NULL) {
        return NULL;
    }
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = tags != NULL ? (ULONG)TAG_MORE : (ULONG)TAG_DONE, def[1].ti_Data = (ULONG)(uintptr_t)tags;
    o = NewObjectA(g_scroller_class->mcc_Class, NULL, def);
    if (o != NULL) {
        g_scroller_made++;
    }
    return o;
}

uint32_t mui_aascroller_made(void) {
    return g_scroller_made;
}

void mui_aascroller_class_free(void) {
    if (g_scroller_class != NULL) {
        MUI_DeleteCustomClass(g_scroller_class);
        g_scroller_class = NULL;
    }
}
