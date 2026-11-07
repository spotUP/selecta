/* Selecta_AAButton (I665 spike, rebuilt on mui_aa_draw in I667a, rolled out to every button in I667b). MUI 3.8:
 * MUI_CreateCustomClass on Area, same dispatcher pattern as mui_canvas_class.c. Draw: the super class fills the parent
 * background (so the corners show the panel), then one flat rounded fill in the role and state colour (mui_aa_button_rgb),
 * the baked Lucide icon drawn as pen runs in a four step ramp from face to caption colour, then the caption through aa_text
 * (TC, palette ramp or plain backend, see mui_aa_draw.c). No bevel pens, no shine, no shadow. Press tracking stays with MUI
 * (RelVerify and MUIA_Selected, notification on MUIA_Pressed), hover is a mouse move handler. */
#include "mui_aabtn_class.h"
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

#include "map/mui_icons.h"
#include "mui_aa.h"
#include "mui_aa_draw.h"
#include "mui_aatip.h"
#include "map/mui_log.h"

extern struct Library* MUIMasterBase;

typedef struct aabtn_data {
    char caption[48], widest[48];
    const UP_IconData* data; /* UPA_Button_IconData, caller owned */
    int icon[2], alt, icon_only, role, hover, backend, minw, minh, align, on, fit, fixed_w, fixed_h;
    mui_aa_metrics_t m;
    aa_ctx aa;
    struct MUI_EventHandlerNode ehn;
    aatip_slot tip; /* I731: flat tooltip text, side and hover edge */
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

/* Icon slots: a body index of the baked set, AABTN_NO_ICON, or AABTN_DATA_ICON (the caller's UP_IconData). */
enum { AABTN_NO_ICON = -1, AABTN_DATA_ICON = -2 };

static int aabtn_cur_icon(const aabtn_data* d) {
    return d->icon[d->alt] != AABTN_NO_ICON ? d->icon[d->alt] : d->icon[0];
}

static ULONG aabtn_setup(struct IClass* cl, Object* obj, Msg msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    uint32_t face, text;
    int st;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_metrics_aspect(d->aa.font_px, d->aa.aspect_q8, &d->m);
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
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aabtn_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->hover = 0;
    d->tip.in = 0;
    aatip_drop(obj);
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
    d->minw = 2 * d->m.pad_x + (aabtn_cur_icon(d) != AABTN_NO_ICON ? d->m.icon_px : 0) + (tw > 0 ? (aabtn_cur_icon(d) != AABTN_NO_ICON ? d->m.icon_gap : 0) + tw : 0);
    d->minh = d->m.height;
    if (aabtn_cur_icon(d) != AABTN_NO_ICON && d->m.icon_h + 2 * d->m.pad_y > d->minh) {
        d->minh = d->m.icon_h + 2 * d->m.pad_y;
    }
    if (!d->icon_only && aa_text_height(&d->aa, obj) + 2 * d->m.pad_y > d->minh) {
        d->minh = aa_text_height(&d->aa, obj) + 2 * d->m.pad_y;
    }
    if (d->role == MUI_AA_ROLE_TOGGLE) { /* AACheck: a fixed square box */
        d->minw = d->m.check_px, d->minh = d->m.check_h;
    }
}

static ULONG aabtn_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    mui_btn_box_t bx;
    DoSuperMethodA(cl, obj, (Msg)msg);
    aabtn_measure(obj, d);
    mui_aabtn_minmax((uint32_t)d->minw, (uint32_t)d->minh, (uint32_t)d->fixed_w, (uint32_t)d->fixed_h, d->role == MUI_AA_ROLE_TOGGLE, &bx); /* I973b */
    msg->MinMaxInfo->MinWidth += bx.min_w;
    msg->MinMaxInfo->DefWidth += bx.def_w;
    msg->MinMaxInfo->MaxWidth = bx.w_fixed ? msg->MinMaxInfo->MinWidth : MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += bx.min_h;
    msg->MinMaxInfo->DefHeight += bx.def_h;
    msg->MinMaxInfo->MaxHeight += bx.min_h;
    return 0;
}

/* The baked icon body of exactly px (2 planes, rows interleaved, pen 0 transparent; I667m); a px with no body falls back to the
 * largest body scaled by nearest neighbour. Drawn
 * as horizontal runs of one ramp pen from the face colour to the caption colour. */
static void aabtn_icon(aabtn_data* d, struct RastPort* rp, int icon, int x, int y, int px, int ph, uint32_t face, uint32_t ink) {
    int bp = px, bh = ph;
    const uint8_t* body = icon >= 0 ? mui_icon_body_wh(icon, px, ph) : NULL; /* I949e2: the exact px x ph set (the 16x8 body on tall pixels) first */
    const UP_IconData* data = icon == AABTN_DATA_ICON ? d->data : NULL;
    if (data != NULL && (data->alpha == NULL || data->w <= 0 || data->h <= 0)) {
        return;
    }
    if (body == NULL && data == NULL && icon >= 0) {
        bh = px;
        body = mui_icon_body(icon, px);
    }
    int rb, dy, dx, run0, run_pen;
    uint32_t col[4];
    LONG pen[4];
    int i;
    if (body == NULL && data == NULL && icon >= 0 && mui_icon_size_count > 0) {
        bp = (int)mui_icon_sizes[mui_icon_size_count - 1];
        bh = bp;
        body = mui_icon_body_wh(icon, bp, (int)mui_icon_heights[mui_icon_size_count - 1]);
    }
    if ((body == NULL && data == NULL) || px <= 0 || ph <= 0) {
        return;
    }
    if (data != NULL) { /* the caller's alpha mask: the same four pen steps the bake uses (48, 112, 184) */
        bp = (int)data->w;
        bh = (int)data->h;
    }
    rb = ((bp + 15) / 16) * 2;
    for (i = 1; i < 4; i++) {
        col[i] = mui_aa_ramp_rgb(ink, face, 4, i);
        pen[i] = aa_pen(&d->aa, col[i]);
    }
    for (dy = 0; dy < ph; dy++) { /* I949e: ph rows for a px wide icon: a square icon on tall pixels is px x ph */
        const uint8_t* row = data != NULL ? data->alpha + (size_t)(dy * bh / ph) * (size_t)bp : body + (size_t)(dy * bh / ph) * (size_t)rb * 2u;
        run0 = 0;
        run_pen = 0;
        for (dx = 0; dx <= px; dx++) {
            int p = 0;
            if (dx < px) {
                int sx = dx * bp / px;
                if (data != NULL) {
                    unsigned a = row[sx];
                    p = a < 48u ? 0 : a < 112u ? 1 : a < 184u ? 2 : 3;
                } else {
                    p = ((row[sx >> 3] >> (7 - (sx & 7))) & 1) | (((row[rb + (sx >> 3)] >> (7 - (sx & 7))) & 1) << 1);
                }
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
        int bx = l + (w - d->m.check_px) / 2, by = t + (h - d->m.check_h) / 2, cp = d->m.check_px, ch = d->m.check_h, asp = d->m.aspect_q8;
        mui_aa_check_op_t ops[MUI_AA_CHECK_OPS];
        int n = mui_aa_check_plan(d->aa.tok, &d->m, state, ops), k;
        for (k = 0; k < n; k++) {
            int iv = ops[k].inset > 0 ? mui_aa_rows(ops[k].inset, asp) : 0; /* I949e: the inset is a physical length, fewer rows on tall pixels */
            aa_round_fill(rp, bx + ops[k].inset, by + iv, cp - 2 * ops[k].inset, ch - 2 * iv, ops[k].radius, aa_pen(&d->aa, ops[k].rgb));
        }
        if (state == MUI_AA_SELECTED) {
            aabtn_icon(d, rp, icon, bx + 2, by + mui_aa_rows(2, asp), cp - 4, ch - 2 * mui_aa_rows(2, asp), face, text);
        }
        if (aa_live(obj)) {
            GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
            if ((Object*)active == obj) {
                aa_round_frame(rp, bx, by, cp, ch, ops[n - 1].inset > 0 ? ops[n - 1].radius + ops[n - 1].inset : ops[n - 1].radius, aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]));
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
    cw = (icon != AABTN_NO_ICON ? d->m.icon_px : 0) + (icon != AABTN_NO_ICON && tw > 0 ? d->m.icon_gap : 0) + tw;
    x = l + mui_aa_btn_content_x(d->align, w, cw, d->m.pad_x);
    if (icon != AABTN_NO_ICON) {
        aabtn_icon(d, rp, icon, x, t + (h - d->m.icon_h) / 2, d->m.icon_px, d->m.icon_h, face, text);
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
        aatip_event(obj, &d->tip, msg->imsg, in);
        if (in != d->hover) {
            d->hover = in;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
    }
    if (msg->imsg != NULL && msg->imsg->Class != IDCMP_MOUSEMOVE) {
        aatip_event(obj, &d->tip, msg->imsg, 0);
    }
    return 0;
}

/* I625a6: first-call guards of the --log trace (one line per method, the first button that reaches it). */
static unsigned long g_n_setup, g_n_ask, g_n_show, g_n_draw;
static ULONG __attribute__((used)) aabtn_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aabtn_dispatch(void);
const struct aa_class_desc aabtn_class_desc = {MUIC_Area, sizeof(aabtn_data), aabtn_dispatch, &g_class};
__asm__(".text\n.even\n.globl _aabtn_dispatch\n_aabtn_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aabtn_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aabtn_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
    case OM_SET:
        return aatip_super_set(cl, obj, (struct opSet*)msg, offsetof(aabtn_data, tip));
    case OM_GET:
        if (aatip_get((struct opGet*)msg, &((aabtn_data*)INST_DATA(cl, obj))->tip)) {
            return TRUE;
        }
        break;
    case UPM_Shared_Fire: {
        aabtn_data* d = INST_DATA(cl, obj);
        return aatip_fire(obj, &d->aa, &d->tip);
    }
    case MUIM_Hide:
        aatip_drop(obj);
        break;
    case MUIM_Show:
        if (g_n_show++ == 0) {
            aa_trace("mcc: first MUIM_Show reached a button (window is open, layout done)");
        }
        break;
    case MUIM_Setup:
        if (g_n_setup++ == 0) {
            aa_trace("mcc: first MUIM_Setup reached a button");
        }
        return aabtn_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aabtn_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        if (g_n_ask++ == 0) {
            aa_trace("mcc: first MUIM_AskMinMax reached a button");
        }
        return aabtn_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        if (g_n_draw++ == 0) {
            aa_trace("mcc: first MUIM_Draw reached a button");
        }
        return aabtn_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aabtn_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Tags: UPA_Button_* are read here; MUIA_ShortHelp, MUIA_Selected, MUIA_Weight, MUIA_ControlChar pass through to the Area. Defaults: no
 * frame, on the cycle chain, input mode by role (toggle or release). */
Object* UP_NewButton(const struct TagItem* tags) {
    Object* o;
    aabtn_data* d;
    struct TagItem def[4];
    int role = (int)GetTagData(UPA_Button_Role, UPV_Button_Role_Normal, (struct TagItem*)tags);
    const char* caption = (const char*)GetTagData(UPA_Button_Caption, (ULONG)(uintptr_t)"", (struct TagItem*)tags);
    const char* widest = (const char*)GetTagData(UPA_Button_Widest, 0, (struct TagItem*)tags);
    const UP_IconData* data = (const UP_IconData*)(uintptr_t)GetTagData(UPA_Button_IconData, 0, (struct TagItem*)tags);
    int icon = mui_icon_index((const char*)(uintptr_t)GetTagData(UPA_Button_Icon, 0, (struct TagItem*)tags));
    if (icon < 0) {
        icon = data != NULL ? AABTN_DATA_ICON : AABTN_NO_ICON;
    }
    if (g_class == NULL) {
        g_class = AA_CLASS_CREATE(&aabtn_class_desc);
    }
    if (g_class == NULL) {
        return NULL;
    }
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = MUIA_InputMode, def[1].ti_Data = role == MUI_AA_ROLE_TOGGLE ? MUIV_InputMode_Toggle : MUIV_InputMode_RelVerify;
    def[2].ti_Tag = MUIA_CycleChain, def[2].ti_Data = (ULONG)TRUE;
    def[3].ti_Tag = TAG_MORE, def[3].ti_Data = (ULONG)(uintptr_t)tags;
    o = NewObjectA(g_class->mcc_Class, NULL, def);
    if (o != NULL) {
        d = INST_DATA(g_class->mcc_Class, o);
        strncpy(d->caption, caption, sizeof d->caption - 1);
        strncpy(d->widest, widest != NULL ? widest : caption, sizeof d->widest - 1);
        d->fit = widest == NULL;
        d->icon[0] = icon;
        d->data = data;
        d->icon[1] = mui_icon_index((const char*)(uintptr_t)GetTagData(UPA_Button_AltIcon, 0, (struct TagItem*)tags));
        if (d->icon[1] < 0) {
            d->icon[1] = AABTN_NO_ICON;
        }
        d->icon_only = GetTagData(UPA_Button_IconOnly, FALSE, (struct TagItem*)tags) && icon != AABTN_NO_ICON;
        d->role = role;
        d->align = (int)GetTagData(UPA_Button_Align, UPV_Button_Align_Left, (struct TagItem*)tags);
        d->fixed_w = (int)GetTagData(UPA_Button_FixedWidth, 0, (struct TagItem*)tags);
        d->fixed_h = (int)GetTagData(UPA_Button_FixedHeight, 0, (struct TagItem*)tags);
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
    if (aa_live(obj)) { /* AATabs sets the lit state while it builds, before any window exists (I684a) */
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
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
