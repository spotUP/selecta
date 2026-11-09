/* Selecta_AAField (I667h, design section 3.4). Same dispatcher pattern as mui_aapanel_class.c. Colours, outline and radius come from
 * map/mui_aa.c (mui_aa_field_*); drawing goes through aa_round_fill with held pens; no colour literal, no ObtainBestPen. The group has
 * MUIA_FillArea FALSE, so the corners keep the parent panel's face; the inner stock String keeps the MUI prefs font and pens. */
#include "mui_aafield_class.h"
#include "mui_aa_class.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <intuition/sghooks.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <mcc/up.h>

#include "map/mui_theme.h"
#include "mui_aa_draw.h"
#include "mui_aatip.h"
#include "map/mui_log.h"

extern struct Library* MUIMasterBase;

typedef struct aafield_data {
    Object* str;
    mui_aa_field_t m;
    int max_w;
    const char* placeholder;
    aa_ctx aa;
    int pens_logged; /* the old String pens went to stderr once (I667n evidence) */
    struct MUI_EventHandlerNode ehn; /* I731: pointer and key events for the flat tooltip only; it never eats one */
    aatip_slot tip;
} aafield_data;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;
static char g_bg_spec[40]; /* MUIA_Background string of the inner String; MUI keeps the pointer, so it lives for the process */

static int aafield_focus(Object* obj, const aafield_data* d) {
    ULONG active = 0;
    if (!aa_live(obj)) {
        return 0;
    }
    GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
    return (Object*)active == d->str;
}

static ULONG aafield_setup(struct IClass* cl, Object* obj, Msg msg) {
    aafield_data* d = INST_DATA(cl, obj);
    mui_aa_check_op_t ops[MUI_AA_FIELD_OPS];
    int k, n;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_field_metrics(d->aa.font_px, d->aa.aspect_q8, &d->m);
    for (k = 0; k < 2; k++) {
        n = mui_aa_field_plan(d->aa.tok, &d->m, k, ops);
        while (n-- > 0) {
            aa_hold(&d->aa, ops[n].rgb);
        }
    }
    DoMethod(_win(obj), MUIM_Notify, MUIA_Window_ActiveObject, MUIV_EveryTime, (ULONG)obj, 1, UPM_Field_Redraw);
    DoMethod(d->str, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, (ULONG)obj, 1, UPM_Field_Redraw); /* I733: the placeholder shows and hides with the contents */
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aafield_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aafield_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_KillNotifyObj, MUIA_Window_ActiveObject, (ULONG)obj);
    DoMethod(d->str, MUIM_KillNotifyObj, MUIA_String_Contents, (ULONG)obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->tip.in = 0;
    aatip_drop(obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aafield_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    int ch, h;
    DoSuperMethodA(cl, obj, (Msg)msg);
    ch = msg->MinMaxInfo->MinHeight - 2 * (d->m.pad_y + d->m.outline);
    h = mui_aa_field_height(&d->m, ch);
    msg->MinMaxInfo->MinHeight = msg->MinMaxInfo->DefHeight = msg->MinMaxInfo->MaxHeight = h;
    if (d->max_w > 0 && msg->MinMaxInfo->MaxWidth > d->max_w) {
        msg->MinMaxInfo->MaxWidth = d->max_w;
        if (msg->MinMaxInfo->DefWidth > d->max_w) {
            msg->MinMaxInfo->DefWidth = d->max_w;
        }
    }
    return 0;
}

/* I667n: MUI 3.8 String renders through an Intuition string gadget that fills its whole box with the StringExtend pens (MUI prefs
 * string colours, the window pen 121214 on the rig), so MUIA_Background never shows inside it. The face pen goes into the background
 * slot of both pen pairs; the text pens stay the user's. Runs before every draw: MUI may rebuild the gadget on setup. */
static void aafield_string_pens(aafield_data* d) {
    struct Gadget* g = NULL;
    struct StringInfo* si;
    LONG pen[2];
    int k;
    if (d->str == NULL || !GetAttr(MUIA_Gadget_Gadget, d->str, (ULONG*)&g) || g == NULL || !mui_aa_field_string_gadget(g->GadgetType)) {
        return;
    }
    si = (struct StringInfo*)g->SpecialInfo;
    if (si == NULL || si->Extension == NULL) {
        return;
    }
    for (k = 0; k < 2; k++) {
        pen[k] = aa_nearest_pen(&d->aa, mui_aa_field_string_bg(d->aa.tok, k));
    }
    if (!d->pens_logged) {
        aa_log("up-mcc: field string pens %u/%u active %u/%u -> background %ld/%ld\n", (unsigned)si->Extension->Pens[0],
                (unsigned)si->Extension->Pens[1], (unsigned)si->Extension->ActivePens[0], (unsigned)si->Extension->ActivePens[1], (long)pen[0], (long)pen[1]);
        d->pens_logged = 1;
    }
    if (pen[0] >= 0) {
        si->Extension->Pens[1] = (UBYTE)pen[0];
    }
    if (pen[1] >= 0) {
        si->Extension->ActivePens[1] = (UBYTE)pen[1];
    }
}

static ULONG aafield_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    /* The Group super fills the padding around the inner String with the parent background, so the face and outline go on after
     * it, around the String's box, which the super has drawn by then. */
    ULONG r;
    aafield_string_pens(d);
    r = DoSuperMethodA(cl, obj, (Msg)msg);
    if (msg->flags & MADF_DRAWOBJECT) {
        mui_aa_check_op_t ops[MUI_AA_FIELD_OPS];
        struct RastPort* rp = _rp(obj);
        int n = mui_aa_field_plan(d->aa.tok, &d->m, aafield_focus(obj, d), ops), k;
        if (d->aa.tc && n > 0) {
            /* I685x: the super filled the outer corner wedges with the snapped parent pen; on truecolour repaint them in the exact parent RGB. */
            aa_round_corners(&d->aa, rp, _left(obj), _top(obj), _width(obj), _height(obj), ops[0].radius, d->aa.bg_rgb);
        }
        for (k = 0; k < n; k++) {
            aa_round_fill_hole(&d->aa, rp, _left(obj) + ops[k].inset, _top(obj) + ops[k].inset, _width(obj) - 2 * ops[k].inset, _height(obj) - 2 * ops[k].inset,
                               ops[k].radius, ops[k].rgb, _left(d->str), _top(d->str), _width(d->str), _height(d->str));
        }
    }
    if ((msg->flags & MADF_DRAWOBJECT) && d->placeholder != NULL) { /* I733 (design 3.4): after the super, so it lands on the String's box */
        ULONG cont = 0;
        uint32_t rgb;
        GetAttr(MUIA_String_Contents, d->str, &cont);
        if (mui_aa_field_placeholder(d->aa.tok, (const char*)cont, aafield_focus(obj, d), &rgb)) {
            int th = aa_text_height(&d->aa, obj);
            if (d->aa.tc) {
                /* I685x: the empty String box carries the snapped face pen; on truecolour the face under the placeholder is the exact FACE RGB. */
                aa_rect(&d->aa, _rp(obj), _left(d->str), _top(d->str), _left(d->str) + _width(d->str) - 1, _top(d->str) + _height(d->str) - 1, d->aa.tok[MUI_AA_FIELD_FACE]);
            }
            aa_text(&d->aa, obj, _rp(obj), _left(d->str), _top(d->str) + (_height(d->str) - th) / 2, d->placeholder, rgb, d->aa.tok[MUI_AA_FIELD_FACE]);
        }
    }
    return r;
}

/* MUIA_String_Contents is the field's contents: set goes to the inner string, get reads it. Everything else is the group's. */
static ULONG aafield_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    struct TagItem *tag, *list = msg->ops_AttrList;
    while ((tag = NextTagItem(&list)) != NULL) {
        if (tag->ti_Tag == MUIA_String_Contents && d->str != NULL) {
            set(d->str, MUIA_String_Contents, tag->ti_Data);
        }
    }
    return aatip_super_set(cl, obj, msg, offsetof(aafield_data, tip));
}

static ULONG aafield_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    if (im != NULL) {
        aatip_event(obj, &d->tip, im, im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj));
    }
    return 0;
}

static ULONG aafield_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == MUIA_String_Contents && d->str != NULL) {
        return GetAttr(MUIA_String_Contents, d->str, msg->opg_Storage);
    }
    if (msg->opg_AttrID == UPA_Field_String) {
        *msg->opg_Storage = (ULONG)(uintptr_t)d->str;
        return TRUE;
    }
    if (aatip_get(msg, &d->tip)) {
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

/* OM_NEW: a by-name MUI_NewObject("UP-Field.mcc") has no MUIA_Group_Child, so the field builds its own String and the two flexible
 * spacers here (the same object UP_NewField assembles); UP_NewField passes its children and goes straight through. */
static ULONG aafield_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aafield_data* d;
    struct opSet nm;
    struct TagItem def[12];
    struct TagItem* tags = msg->ops_AttrList;
    Object* str = NULL;
    mui_aa_field_t m;
    if (FindTagItem(MUIA_Group_Child, tags) == NULL) {
        mui_aa_field_metrics(0, 256, &m);
        mui_theme_bg_spec(mui_theme_tokens()[MUI_AA_FIELD_FACE], g_bg_spec, sizeof g_bg_spec);
        str = MUI_NewObject((char*)MUIC_String, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_Background, (ULONG)g_bg_spec, MUIA_CycleChain, (ULONG)TRUE, MUIA_String_MaxLen,
                            GetTagData(MUIA_String_MaxLen, 80, tags), MUIA_String_Contents, (ULONG) "", TAG_DONE);
        if (str == NULL) {
            return 0;
        }
        def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
        def[1].ti_Tag = MUIA_Background, def[1].ti_Data = (ULONG)g_bg_spec;
        def[2].ti_Tag = MUIA_FillArea, def[2].ti_Data = (ULONG)FALSE;
        def[3].ti_Tag = MUIA_Group_Spacing, def[3].ti_Data = 0;
        def[4].ti_Tag = MUIA_InnerLeft, def[4].ti_Data = (ULONG)m.pad_x;
        def[5].ti_Tag = MUIA_InnerRight, def[5].ti_Data = (ULONG)m.pad_x;
        def[6].ti_Tag = MUIA_InnerTop, def[6].ti_Data = (ULONG)(m.pad_y + m.outline);
        def[7].ti_Tag = MUIA_InnerBottom, def[7].ti_Data = (ULONG)(m.pad_y + m.outline);
        def[8].ti_Tag = MUIA_Group_Child, def[8].ti_Data = (ULONG)MUI_NewObject((char*)MUIC_Rectangle, MUIA_VertWeight, 100UL, TAG_DONE);
        def[9].ti_Tag = MUIA_Group_Child, def[9].ti_Data = (ULONG)str;
        def[10].ti_Tag = MUIA_Group_Child, def[10].ti_Data = (ULONG)MUI_NewObject((char*)MUIC_Rectangle, MUIA_VertWeight, 100UL, TAG_DONE);
        def[11].ti_Tag = TAG_MORE, def[11].ti_Data = (ULONG)(uintptr_t)tags;
        nm = *msg;
        nm.ops_AttrList = def;
        msg = &nm;
    }
    obj = (Object*)aatip_super_set(cl, obj, msg, offsetof(aafield_data, tip));
    if (obj == NULL) {
        return 0;
    }
    if (str != NULL) {
        d = INST_DATA(cl, obj);
        d->str = str;
        d->max_w = (int)GetTagData(UPA_Field_MaxWidth, 0, tags);
        d->placeholder = (const char*)GetTagData(UPA_Field_Placeholder, 0, tags);
    }
    return (ULONG)obj;
}

static ULONG __attribute__((used)) aafield_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aafield_dispatch(void);
const struct aa_class_desc aafield_class_desc = {MUIC_Group, sizeof(aafield_data), aafield_dispatch, &g_class};
__asm__(".text\n.even\n.globl _aafield_dispatch\n_aafield_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aafield_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aafield_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aafield_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aafield_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aafield_get(cl, obj, (struct opGet*)msg);
    case UPM_Shared_Fire: {
        aafield_data* d = INST_DATA(cl, obj);
        return aatip_fire(obj, &d->aa, &d->tip);
    }
    case MUIM_Hide:
        aatip_drop(obj);
        break;
    case MUIM_HandleEvent:
        return aafield_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    case MUIM_Setup:
        return aafield_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aafield_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aafield_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aafield_draw(cl, obj, (struct MUIP_Draw*)msg);
    case UPM_Field_Redraw:
        MUI_Redraw(obj, MADF_DRAWOBJECT);
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Tags: UPA_Field_* are read here; MUIA_String_MaxLen goes to the inner string. */
Object* UP_NewField(const struct TagItem* tags) {
    Object *o, *str;
    mui_aa_field_t m;
    const uint32_t* tokens = (const uint32_t*)GetTagData(UPA_Field_Tokens, 0, (struct TagItem*)tags);
    if (tokens == NULL) {
        return NULL;
    }
    if (g_class == NULL) {
        g_class = AA_CLASS_CREATE(&aafield_class_desc);
    }
    if (g_class == NULL) {
        return NULL;
    }
    mui_aa_field_metrics(0, 256, &m); /* the paddings are the same on every tier */
    mui_theme_bg_spec(tokens[MUI_AA_FIELD_FACE], g_bg_spec, sizeof g_bg_spec);
    str = MUI_NewObject((char*)MUIC_String, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_Background, (ULONG)g_bg_spec,
                        MUIA_CycleChain, (ULONG)TRUE, MUIA_String_MaxLen, GetTagData(MUIA_String_MaxLen, 80, (struct TagItem*)tags), MUIA_String_Contents, (ULONG) "", TAG_DONE);
    if (str == NULL) {
        return NULL;
    }
    o = NewObject(g_class->mcc_Class, NULL, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_Background, (ULONG)g_bg_spec, MUIA_FillArea, (ULONG)FALSE, MUIA_Group_Spacing, 0UL, MUIA_InnerLeft,
                  (ULONG)m.pad_x, MUIA_InnerRight, (ULONG)m.pad_x, MUIA_InnerTop, (ULONG)(m.pad_y + m.outline), MUIA_InnerBottom, (ULONG)(m.pad_y + m.outline),
                  MUIA_Group_Child, (ULONG)MUI_NewObject((char*)MUIC_Rectangle, MUIA_VertWeight, 100UL, TAG_DONE), /* flexible VSpace(0) above and below: MUI 3.8 centres a fixed-height child */
                  MUIA_Group_Child, (ULONG)str, MUIA_Group_Child, (ULONG)MUI_NewObject((char*)MUIC_Rectangle, MUIA_VertWeight, 100UL, TAG_DONE), TAG_DONE);
    if (o == NULL) {
        MUI_DisposeObject(str);
        return NULL;
    }
    ((aafield_data*)INST_DATA(g_class->mcc_Class, o))->str = str;
    ((aafield_data*)INST_DATA(g_class->mcc_Class, o))->max_w = (int)GetTagData(UPA_Field_MaxWidth, 0, (struct TagItem*)tags);
    ((aafield_data*)INST_DATA(g_class->mcc_Class, o))->placeholder = (const char*)GetTagData(UPA_Field_Placeholder, 0, (struct TagItem*)tags);
    g_made++;
    return o;
}

uint32_t mui_aafield_made(void) {
    return g_made;
}

void mui_aafield_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
