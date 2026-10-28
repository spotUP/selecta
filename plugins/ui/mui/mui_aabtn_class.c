/* Selecta_AAButton (I665 spike, rebuilt on mui_aa_draw in I667a). MUI 3.8: MUI_CreateCustomClass on Area, same dispatcher
 * pattern as mui_canvas_class.c. Draw: the super class fills the parent background (so the corners show the panel), then
 * one flat rounded fill in the state colour (mui_aa_state_rgb), then the caption through aa_text (TC, palette ramp or plain
 * backend, see mui_aa_draw.c). No bevel pens, no shine, no shadow. */
#include "mui_aabtn_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

#define PAD_X 14
#define PAD_Y 6
#define RADIUS 6

typedef struct aabtn_data {
    char caption[48], widest[48];
    uint32_t face, text;
    aa_ctx aa;
    int backend, minw, minh;
} aabtn_data;

static struct MUI_CustomClass* g_class;

static ULONG aabtn_setup(struct IClass* cl, Object* obj, Msg msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    int tw;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    aa_pen(&d->aa, mui_aa_state_rgb(d->aa.tok, d->face, MUI_AA_NORMAL));
    aa_pen(&d->aa, mui_aa_state_rgb(d->aa.tok, d->face, MUI_AA_PRESSED));
    aa_pen(&d->aa, d->text);
    tw = aa_text_width(&d->aa, obj, d->widest);
    d->minw = tw;
    d->minh = aa_text_height(&d->aa, obj);
    return TRUE;
}

static ULONG aabtn_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aabtn_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    int w = d->minw + 2 * PAD_X, h = d->minh + 2 * PAD_Y;
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += w;
    msg->MinMaxInfo->DefWidth += w;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += h;
    msg->MinMaxInfo->DefHeight += h;
    msg->MinMaxInfo->MaxHeight += h;
    return 0;
}

static ULONG aabtn_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    ULONG sel = 0;
    int l, t, w, h, tw, th, state;
    uint32_t face;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    GetAttr(MUIA_Selected, obj, &sel);
    state = sel ? MUI_AA_PRESSED : MUI_AA_NORMAL;
    face = mui_aa_state_rgb(d->aa.tok, d->face, state);
    aa_round_fill(rp, l, t, w, h, RADIUS, aa_pen(&d->aa, face));
    tw = aa_text_width(&d->aa, obj, d->caption);
    th = aa_text_height(&d->aa, obj);
    aa_text(&d->aa, obj, rp, l + (w - tw) / 2, t + (h - th) / 2, d->caption, d->text, face);
    d->backend = d->aa.backend + 1;
    return 0;
}

static ULONG __attribute__((used)) aabtn_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aabtn_dispatch(void);
__asm__(".text\n.even\n.globl _aabtn_dispatch\n_aabtn_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aabtn_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aabtn_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case MUIM_Setup:
        return aabtn_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aabtn_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aabtn_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aabtn_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aabtn_new(const char* caption, const char* widest, uint32_t face, uint32_t text, LONG id) {
    Object* o;
    aabtn_data* d;
    if (g_class == NULL) {
        g_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Area, NULL, sizeof(aabtn_data), (APTR)(uintptr_t)aabtn_dispatch);
    }
    if (g_class == NULL) {
        return NULL;
    }
    o = NewObject(g_class->mcc_Class, NULL, MUIA_Frame, MUIV_Frame_None, MUIA_InputMode, MUIV_InputMode_RelVerify, MUIA_Weight, 100,
                  MUIA_ShortHelp, (ULONG)caption, TAG_DONE);
    if (o != NULL) {
        d = INST_DATA(g_class->mcc_Class, o);
        strncpy(d->caption, caption, sizeof d->caption - 1);
        strncpy(d->widest, widest, sizeof d->widest - 1);
        d->face = face;
        d->text = text;
        DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)id);
    }
    return o;
}

void mui_aabtn_set_caption(Object* obj, const char* caption) {
    aabtn_data* d = INST_DATA(g_class->mcc_Class, obj);
    if (strcmp(d->caption, caption) == 0) {
        return;
    }
    strncpy(d->caption, caption, sizeof d->caption - 1);
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

int mui_aabtn_backend(Object* obj) {
    return ((aabtn_data*)INST_DATA(g_class->mcc_Class, obj))->backend;
}

void mui_aabtn_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
    aa_free();
}
