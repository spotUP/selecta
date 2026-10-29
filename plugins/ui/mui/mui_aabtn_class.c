/* Selecta_AAButton (I665 spike, rebuilt on mui_aa_draw in I667a, rolled out to every button in I667b). MUI 3.8:
 * MUI_CreateCustomClass on Area, same dispatcher pattern as mui_canvas_class.c. Draw: the super class fills the parent
 * background (so the corners show the panel), then one flat rounded fill in the role and state colour (mui_aa_button_rgb),
 * the baked Lucide icon drawn as pen runs in a four step ramp from face to caption colour, then the caption through aa_text
 * (TC, palette ramp or plain backend, see mui_aa_draw.c). No bevel pens, no shine, no shadow. Press tracking stays with MUI
 * (RelVerify and MUIA_Selected, notification on MUIA_Pressed), hover is a mouse move handler. */
#include "mui_aabtn_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "map/mui_icons.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aabtn_data {
    char caption[48], widest[48];
    int icon[2], alt, icon_only, role, hover, backend, minw, minh, align, on, fit;
    mui_aa_metrics_t m;
    aa_ctx aa;
    struct MUI_EventHandlerNode ehn;
} aabtn_data;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;

static int aabtn_state(Object* obj, const aabtn_data* d) {
    ULONG sel = 0, dis = 0;
    GetAttr(MUIA_Selected, obj, &sel);
    GetAttr(MUIA_Disabled, obj, &dis);
    if (d->role == MUI_AA_ROLE_TOGGLE) { /* AACheck: Selected is the checked value, not a press */
        return dis ? MUI_AA_DISABLED : sel ? MUI_AA_SELECTED : d->hover ? MUI_AA_HOVER : MUI_AA_NORMAL;
    }
    return dis ? MUI_AA_DISABLED : sel ? MUI_AA_PRESSED : d->on ? MUI_AA_SELECTED : d->hover ? MUI_AA_HOVER : MUI_AA_NORMAL;
}

static int aabtn_cur_icon(const aabtn_data* d) {
    return d->icon[d->alt] >= 0 ? d->icon[d->alt] : d->icon[0];
}

static ULONG aabtn_setup(struct IClass* cl, Object* obj, Msg msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    uint32_t face, text;
    int st;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_metrics(d->aa.font_px, &d->m);
    for (st = MUI_AA_NORMAL; st <= MUI_AA_SELECTED; st++) {
        mui_aa_button_rgb(d->aa.tok, d->role, st, &face, &text);
        aa_pen(&d->aa, face);
    }
    aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
    aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_TEXT_MUTED]);
    aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_SURFACE]);
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aabtn_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->hover = 0;
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

/* Size from the widest caption. Runs at every layout (AskMinMax), so a fit button (no widest in its spec, e.g. a breadcrumb) grows with
 * the caption mui_aabtn_set_caption gave it inside the caller's InitChange/ExitChange. */
static void aabtn_measure(Object* obj, aabtn_data* d) {
    int tw = 0;
    if (!d->icon_only) {
        tw = aa_text_width(&d->aa, obj, d->widest);
    }
    d->minw = 2 * d->m.pad_x + (aabtn_cur_icon(d) >= 0 ? d->m.icon_px : 0) + (tw > 0 ? (aabtn_cur_icon(d) >= 0 ? d->m.icon_gap : 0) + tw : 0);
    d->minh = d->m.height;
    if (!d->icon_only && aa_text_height(&d->aa, obj) + 2 * d->m.pad_y > d->minh) {
        d->minh = aa_text_height(&d->aa, obj) + 2 * d->m.pad_y;
    }
    if (d->role == MUI_AA_ROLE_TOGGLE) { /* AACheck: a fixed square box */
        d->minw = d->minh = d->m.check_px;
    }
}

static ULONG aabtn_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    aabtn_measure(obj, d);
    msg->MinMaxInfo->MinWidth += d->minw;
    msg->MinMaxInfo->DefWidth += d->minw;
    msg->MinMaxInfo->MaxWidth = d->role == MUI_AA_ROLE_TOGGLE ? msg->MinMaxInfo->MinWidth : MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += d->minh;
    msg->MinMaxInfo->DefHeight += d->minh;
    msg->MinMaxInfo->MaxHeight += d->minh;
    return 0;
}

/* The baked icon (mui_icon_body_px square, 2 planes, rows interleaved, pen 0 transparent) scaled to px by nearest neighbour,
 * drawn as horizontal runs of one ramp pen from the face colour to the caption colour. */
static void aabtn_icon(aabtn_data* d, struct RastPort* rp, int icon, int x, int y, int px, uint32_t face, uint32_t ink) {
    const uint8_t* body = mui_icon_body(icon);
    int bp = (int)mui_icon_body_px, rb = ((bp + 15) / 16) * 2, dy, dx, run0, run_pen;
    uint32_t col[4];
    LONG pen[4];
    int i;
    if (body == NULL || px <= 0) {
        return;
    }
    for (i = 1; i < 4; i++) {
        col[i] = mui_aa_ramp_rgb(ink, face, 4, i);
        pen[i] = aa_pen(&d->aa, col[i]);
    }
    for (dy = 0; dy < px; dy++) {
        const uint8_t* row = body + (size_t)(dy * bp / px) * (size_t)rb * 2u;
        run0 = 0;
        run_pen = 0;
        for (dx = 0; dx <= px; dx++) {
            int p = 0;
            if (dx < px) {
                int sx = dx * bp / px;
                p = ((row[sx >> 3] >> (7 - (sx & 7))) & 1) | (((row[rb + (sx >> 3)] >> (7 - (sx & 7))) & 1) << 1);
            }
            if (dx == px || p != run_pen) {
                if (run_pen != 0 && pen[run_pen] >= 0) {
                    SetAPen(rp, (ULONG)pen[run_pen]);
                    RectFill(rp, x + run0, y + dy, x + dx - 1, y + dy);
                }
                run0 = dx;
                run_pen = p;
            }
        }
    }
}

static ULONG aabtn_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    ULONG active = 0;
    int l, t, w, h, tw = 0, th = 0, state, icon = aabtn_cur_icon(d), cw, x;
    uint32_t face, text;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    state = aabtn_state(obj, d);
    mui_aa_button_rgb(d->aa.tok, d->role, state, &face, &text);
    if (d->role == MUI_AA_ROLE_TOGGLE) {
        /* AACheck: the paint plan (mui_aa_check_plan) draws corners, outline and face; the checked box is ACCENT with the ACCENT_TEXT check icon. */
        int bx = l + (w - d->m.check_px) / 2, by = t + (h - d->m.check_px) / 2, cp = d->m.check_px;
        mui_aa_check_op_t ops[MUI_AA_CHECK_OPS];
        int n = mui_aa_check_plan(d->aa.tok, &d->m, state, ops), k;
        for (k = 0; k < n; k++) {
            aa_round_fill(rp, bx + ops[k].inset, by + ops[k].inset, cp - 2 * ops[k].inset, cp - 2 * ops[k].inset, ops[k].radius, aa_pen(&d->aa, ops[k].rgb));
        }
        if (state == MUI_AA_SELECTED) {
            aabtn_icon(d, rp, icon, bx + 2, by + 2, cp - 4, face, text);
        }
        if (aa_live(obj)) {
            GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
            if ((Object*)active == obj) {
                aa_round_frame(rp, bx, by, cp, cp, ops[n - 1].inset > 0 ? ops[n - 1].radius + ops[n - 1].inset : ops[n - 1].radius, aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]));
            }
        }
        d->backend = d->aa.backend + 1;
        return 0;
    }
    aa_round_fill(rp, l, t, w, h, d->m.radius, aa_pen(&d->aa, face));
    if (!d->icon_only) {
        tw = aa_text_width(&d->aa, obj, d->caption);
        th = aa_text_height(&d->aa, obj);
    }
    cw = (icon >= 0 ? d->m.icon_px : 0) + (icon >= 0 && tw > 0 ? d->m.icon_gap : 0) + tw;
    x = l + mui_aa_btn_content_x(d->align, w, cw, d->m.pad_x);
    if (icon >= 0) {
        aabtn_icon(d, rp, icon, x, t + (h - d->m.icon_px) / 2, d->m.icon_px, face, text);
        x += d->m.icon_px + d->m.icon_gap;
    }
    if (tw > 0) {
        aa_text(&d->aa, obj, rp, x, t + (h - th) / 2, d->caption, text, face);
    }
    if (aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
        if ((Object*)active == obj) {
            aa_round_frame(rp, l, t, w, h, d->m.radius, aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]));
        }
    }
    d->backend = d->aa.backend + 1;
    return 0;
}

static ULONG aabtn_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    if (msg->imsg != NULL && msg->imsg->Class == IDCMP_MOUSEMOVE) {
        int in = msg->imsg->MouseX >= _left(obj) && msg->imsg->MouseX <= _right(obj) && msg->imsg->MouseY >= _top(obj) && msg->imsg->MouseY <= _bottom(obj);
        if (in != d->hover) {
            d->hover = in;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
    }
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
    case MUIM_HandleEvent:
        return aabtn_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aabtn_new(const mui_aabtn_spec* sp) {
    Object* o;
    aabtn_data* d;
    if (g_class == NULL) {
        g_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Area, NULL, sizeof(aabtn_data), (APTR)(uintptr_t)aabtn_dispatch);
    }
    if (g_class == NULL) {
        return NULL;
    }
    o = NewObject(g_class->mcc_Class, NULL, MUIA_Frame, MUIV_Frame_None, MUIA_InputMode,
                  sp->role == MUI_AA_ROLE_TOGGLE ? MUIV_InputMode_Toggle : MUIV_InputMode_RelVerify, MUIA_Selected, (ULONG)(sp->selected ? TRUE : FALSE), MUIA_Weight, (ULONG)sp->weight,
                  MUIA_CycleChain, TRUE, MUIA_ShortHelp, (ULONG)(sp->help != NULL ? sp->help : sp->caption), TAG_DONE);
    if (o != NULL) {
        d = INST_DATA(g_class->mcc_Class, o);
        strncpy(d->caption, sp->caption, sizeof d->caption - 1);
        strncpy(d->widest, sp->widest != NULL ? sp->widest : sp->caption, sizeof d->widest - 1);
        d->fit = sp->widest == NULL;
        d->icon[0] = sp->icon;
        d->icon[1] = sp->alt_icon;
        d->icon_only = sp->icon_only && sp->icon >= 0;
        d->role = sp->role;
        d->align = sp->align;
        if (sp->role == MUI_AA_ROLE_TOGGLE) {
            DoMethod(o, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)sp->id);
        } else {
            DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)sp->id);
        }
        g_made++;
    }
    return o;
}

uint32_t mui_aabtn_made(void) {
    return g_made;
}

void mui_aabtn_set_caption(Object* obj, const char* caption) {
    aabtn_data* d = INST_DATA(g_class->mcc_Class, obj);
    if (strcmp(d->caption, caption) == 0) {
        return;
    }
    strncpy(d->caption, caption, sizeof d->caption - 1);
    if (d->fit) {
        strncpy(d->widest, caption, sizeof d->widest - 1);
        d->widest[sizeof d->widest - 1] = '\0';
    }
    SetAttrs(obj, MUIA_ShortHelp, (ULONG)d->caption, TAG_DONE); /* icon-only buttons: the state text is the tooltip */
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

void mui_aabtn_set_on(Object* obj, int on) {
    aabtn_data* d = INST_DATA(g_class->mcc_Class, obj);
    if (d->on == (on != 0)) {
        return;
    }
    d->on = on != 0;
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

void mui_aabtn_set_alt(Object* obj, int alt) {
    aabtn_data* d = INST_DATA(g_class->mcc_Class, obj);
    if (d->alt == (alt != 0)) {
        return;
    }
    d->alt = alt != 0;
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
