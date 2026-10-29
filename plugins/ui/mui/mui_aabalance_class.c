/* Selecta_AABalance (I667l, design section 3.10). MUI 3.8: MUI_CreateCustomClass on Balance, same dispatcher pattern as
 * mui_aatabs_class.c. Dragging, the resize maths and the weights stay MUI's (super class). The class overrides AskMinMax (box width =
 * the panel gap, mui_aa_balance_width) and Draw: MADF_DRAWOBJECT / MADF_DRAWUPDATE paints the whole box in the BACKGROUND pen and a
 * centred 1 px DIVIDER line (mui_aa_balance_line / mui_aa_balance_rgb) after the super draw (clip region, I667n); the box fill covers
 * the super's 3D bar, so no prefs frame or bar shows. No hardcoded colour, no ObtainBestPen (held pens through aa_pen). */
#include "mui_aabalance_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include <selecta/uimodel/theme.h>

#include "map/mui_theme.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aabal_data {
    aa_ctx aa;
    LONG box_pen, line_pen;
} aabal_data;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;
static char g_bg_spec[40]; /* fallback MUIA_Background string; MUI keeps the pointer, so it lives for the process */

static ULONG aabal_setup(struct IClass* cl, Object* obj, Msg msg) {
    aabal_data* d = INST_DATA(cl, obj);
    uint32_t box, line;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_balance_rgb(d->aa.tok, &box, &line);
    d->box_pen = aa_pen(&d->aa, box);
    d->line_pen = aa_pen(&d->aa, line);
    return TRUE;
}

static ULONG aabal_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aabal_data* d = INST_DATA(cl, obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aabal_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aabal_data* d = INST_DATA(cl, obj);
    int w = mui_aa_balance_width(d->aa.tok, d->aa.font_px);
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth = w;
    msg->MinMaxInfo->DefWidth = w;
    msg->MinMaxInfo->MaxWidth = w;
    return 0;
}

static ULONG aabal_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aabal_data* d = INST_DATA(cl, obj);
    struct RastPort* rp = _rp(obj);
    int lx, ly, lw, lh;
    /* I667n: the Area super class installs the object's clip region and the window layer lock for the draw; without it every RectFill
     * below was clipped away (rig 2: box and line measured as the parent background only). The super paints its own bar first, the
     * box fill below covers all of it. */
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)) || rp == NULL) {
        return 0;
    }
    SetAPen(rp, (ULONG)d->box_pen);
    RectFill(rp, _mleft(obj), _mtop(obj), _mright(obj), _mbottom(obj));
    mui_aa_balance_line(d->aa.tok, d->aa.font_px, _mwidth(obj), _mheight(obj), &lx, &ly, &lw, &lh);
    if (lw > 0 && lh > 0) {
        SetAPen(rp, (ULONG)d->line_pen);
        RectFill(rp, _mleft(obj) + lx, _mtop(obj) + ly, _mleft(obj) + lx + lw - 1, _mtop(obj) + ly + lh - 1);
    }
    return 0;
}

static ULONG __attribute__((used)) aabal_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aabal_dispatch(void);
__asm__(".text\n.even\n.globl _aabal_dispatch\n_aabal_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aabal_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aabal_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case MUIM_Setup:
        return aabal_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aabal_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aabal_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aabal_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Design 3.10 fallback: when MUI refuses the Balance subclass, a frameless stock Balance on the BACKGROUND face keeps the window
 * openable (a NULL child would fail the whole window tree). */
static Object* aabal_stock(void) {
    uint32_t box, line;
    mui_aa_balance_rgb(mui_theme_tokens(), &box, &line);
    mui_theme_bg_spec(box, g_bg_spec, sizeof g_bg_spec);
    return MUI_NewObject((char*)MUIC_Balance, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_Background, (ULONG)g_bg_spec, TAG_DONE);
}

Object* mui_aabalance_new(void) {
    Object* o;
    if (g_class == NULL) {
        g_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Balance, NULL, sizeof(aabal_data), (APTR)(uintptr_t)aabal_dispatch);
    }
    if (g_class == NULL) {
        return aabal_stock();
    }
    o = NewObject(g_class->mcc_Class, NULL, MUIA_Frame, (ULONG)MUIV_Frame_None, TAG_DONE);
    if (o == NULL) {
        return aabal_stock();
    }
    memset(INST_DATA(g_class->mcc_Class, o), 0, sizeof(aabal_data));
    g_made++;
    return o;
}

uint32_t mui_aabalance_made(void) {
    return g_made;
}

void mui_aabalance_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
