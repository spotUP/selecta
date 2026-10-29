/* Selecta_AAField (I667h, design section 3.4). Same dispatcher pattern as mui_aapanel_class.c. Colours, outline and radius come from
 * map/mui_aa.c (mui_aa_field_*); drawing goes through aa_round_fill with held pens; no colour literal, no ObtainBestPen. The group has
 * MUIA_FillArea FALSE, so the corners keep the parent panel's face; the inner stock String keeps the MUI prefs font and pens. */
#include "mui_aafield_class.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <intuition/sghooks.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "map/mui_theme.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

#define AAF_Redraw (TAG_USER + 0x53410301UL) /* method: the window's active object changed, repaint the outline */

typedef struct aafield_data {
    Object* str;
    mui_aa_field_t m;
    aa_ctx aa;
    int pens_logged; /* the old String pens went to stderr once (I667n evidence) */
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
    mui_aa_field_metrics(d->aa.font_px, &d->m);
    for (k = 0; k < 2; k++) {
        n = mui_aa_field_plan(d->aa.tok, &d->m, k, ops);
        while (n-- > 0) {
            aa_pen(&d->aa, ops[n].rgb);
        }
    }
    DoMethod(_win(obj), MUIM_Notify, MUIA_Window_ActiveObject, MUIV_EveryTime, (ULONG)obj, 1, AAF_Redraw);
    return TRUE;
}

static ULONG aafield_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aafield_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_KillNotifyObj, MUIA_Window_ActiveObject, (ULONG)obj);
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
        pen[k] = aa_pen(&d->aa, mui_aa_field_string_bg(d->aa.tok, k));
    }
    if (!d->pens_logged) {
        fprintf(stderr, "selecta mui: field string pens %u/%u active %u/%u -> background %ld/%ld\n", (unsigned)si->Extension->Pens[0],
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
        for (k = 0; k < n; k++) {
            aa_round_fill_hole(rp, _left(obj) + ops[k].inset, _top(obj) + ops[k].inset, _width(obj) - 2 * ops[k].inset, _height(obj) - 2 * ops[k].inset,
                               ops[k].radius, aa_pen(&d->aa, ops[k].rgb), _left(d->str), _top(d->str), _width(d->str), _height(d->str));
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
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aafield_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == MUIA_String_Contents && d->str != NULL) {
        return GetAttr(MUIA_String_Contents, d->str, msg->opg_Storage);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aafield_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aafield_dispatch(void);
__asm__(".text\n.even\n.globl _aafield_dispatch\n_aafield_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aafield_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aafield_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_SET:
        return aafield_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aafield_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        return aafield_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aafield_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aafield_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aafield_draw(cl, obj, (struct MUIP_Draw*)msg);
    case AAF_Redraw:
        MUI_Redraw(obj, MADF_DRAWOBJECT);
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aafield_new(const mui_aafield_spec* sp) {
    Object *o, *str;
    mui_aa_field_t m;
    if (g_class == NULL) {
        g_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Group, NULL, sizeof(aafield_data), (APTR)(uintptr_t)aafield_dispatch);
    }
    if (g_class == NULL) {
        return NULL;
    }
    mui_aa_field_metrics(0, &m); /* the paddings are the same on every tier */
    mui_theme_bg_spec(sp->tokens[MUI_AA_FIELD_FACE], g_bg_spec, sizeof g_bg_spec);
    str = MUI_NewObject((char*)MUIC_String, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_Background, (ULONG)g_bg_spec, MUIA_String_MaxLen, (ULONG)sp->maxlen,
                        MUIA_String_Contents, (ULONG) "", TAG_DONE);
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
    if (sp->id != 0) {
        DoMethod(str, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)sp->id);
    }
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
