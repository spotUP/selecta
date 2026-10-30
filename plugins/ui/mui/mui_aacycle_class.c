/* Selecta_AACycle (I667g, design section 3.3). Same dispatcher pattern as mui_aaslider_class.c. Sizes, colours, the chevron rows and
 * the Up/Down step come from map/mui_aa.c (mui_aa_cycle_*); drawing goes through aa_round_fill / aa_text with held pens; no colour
 * literal, no ObtainBestPen. The popup is a borderless window with a stock List (the AAList of section 3.6 replaces it later). */
#include "mui_aacycle_class.h"

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
#include "mui_aalist_class.h"

extern struct Library* MUIMasterBase;

typedef struct aacycle_data {
    const char* const* entries;
    int n, active, hover, down;
    mui_aa_cycle_t m;
    aa_ctx aa;
    Object *popup, *list;
    struct MUI_EventHandlerNode ehn;
} aacycle_data;

static struct MUI_CustomClass* g_cycle_class;
static uint32_t g_cycle_made;

static int aacycle_state(Object* obj, const aacycle_data* d) {
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    return dis ? MUI_AA_DISABLED : d->down ? MUI_AA_PRESSED : d->hover ? MUI_AA_HOVER : MUI_AA_NORMAL;
}

static void aacycle_drop_popup(Object* obj, aacycle_data* d) {
    Object* app = NULL;
    if (d->popup == NULL) {
        return;
    }
    if (aa_live(obj)) {
        GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
    }
    set(d->popup, MUIA_Window_Open, FALSE);
    if (app != NULL) {
        DoMethod(app, OM_REMMEMBER, (ULONG)d->popup);
    }
    MUI_DisposeObject(d->popup);
    d->popup = d->list = NULL;
}

static ULONG aacycle_setup(struct IClass* cl, Object* obj, Msg msg) {
    aacycle_data* d = INST_DATA(cl, obj);
    int st, th;
    uint32_t face, text, chev;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_cycle_metrics(d->aa.font_px, &d->m);
    for (st = MUI_AA_NORMAL; st <= MUI_AA_DISABLED; st++) {
        mui_aa_cycle_rgb(d->aa.tok, st, &face, &text, &chev, NULL, NULL);
        aa_pen(&d->aa, face);
        aa_pen(&d->aa, text);
        aa_pen(&d->aa, chev);
    }
    aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
    th = aa_text_height(&d->aa, obj) + 12;
    if (th > d->m.height) {
        d->m.height = th;
    }
    d->hover = d->down = 0;
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aacycle_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aacycle_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    aacycle_drop_popup(obj, d);
    d->hover = d->down = 0;
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aacycle_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aacycle_data* d = INST_DATA(cl, obj);
    int k, tw, widest = 0, minw;
    DoSuperMethodA(cl, obj, (Msg)msg);
    for (k = 0; k < d->n; k++) {
        tw = aa_text_width(&d->aa, obj, d->entries[k]);
        widest = tw > widest ? tw : widest;
    }
    minw = mui_aa_cycle_min_w(&d->m, widest);
    msg->MinMaxInfo->MinWidth += minw;
    msg->MinMaxInfo->DefWidth += minw;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += d->m.height;
    msg->MinMaxInfo->DefHeight += d->m.height;
    msg->MinMaxInfo->MaxHeight += d->m.height;
    return 0;
}

static ULONG aacycle_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aacycle_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    ULONG active = 0;
    int l, t, w, h, state, r, cx, cy, th, rx, rw;
    uint32_t face, text, chev;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    state = aacycle_state(obj, d);
    mui_aa_cycle_rgb(d->aa.tok, state, &face, &text, &chev, NULL, NULL);
    aa_round_fill(rp, l, t, w, h, d->m.radius, aa_pen(&d->aa, face));
    if (d->n > 0) {
        th = aa_text_height(&d->aa, obj);
        aa_text(&d->aa, obj, rp, l + d->m.pad_x, t + (h - th) / 2, d->entries[d->active], text, face);
    }
    cx = l + w - d->m.pad_x - d->m.chev_w;
    cy = t + (h - d->m.chev_h) / 2;
    for (r = 0; r < d->m.chev_h; r++) {
        mui_aa_cycle_chevron_row(&d->m, r, &rx, &rw);
        aa_round_fill(rp, cx + rx, cy + r, rw, 1, 0, aa_pen(&d->aa, chev));
    }
    if (aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
        if ((Object*)active == obj) {
            aa_round_frame(rp, l + 1, t + 1, w - 2, h - 2, d->m.radius, aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]));
        }
    }
    return 0;
}

/* Row callback of the popup AAList: the entry text. */
static const char* aacycle_row(void* ctx, long row) {
    aacycle_data* d = (aacycle_data*)ctx;
    return row >= 0 && row < d->n ? d->entries[row] : "";
}

static void aacycle_open_popup(Object* obj, aacycle_data* d) {
    Object *app = NULL, *list = NULL, *group;
    mui_aalist_spec spec;
    struct Window* win = _window(obj);
    if (win == NULL || d->n <= 0 || d->popup != NULL) {
        return;
    }
    GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
    if (app == NULL) {
        return;
    }
    spec.count = d->n;
    spec.active = d->active;
    spec.row = aacycle_row;
    spec.ctx = d;
    spec.max_rows = 12;
    group = mui_aalist_new(&spec, &list); /* I667i: the AAList look (padding, row spacing, flat scroller) instead of the stock List */
    if (group == NULL) {
        return;
    }
    /* The popup opens on the cycle's own screen; without MUIA_Window_Screen MUI opens it on the default public screen and brings
     * that screen to the front, hiding Selecta's screen (Johan rig 2 2026-10-29). */
    d->popup = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Screen, (ULONG)_screen(obj), MUIA_Window_Borderless, TRUE, MUIA_Window_DragBar, FALSE, MUIA_Window_CloseGadget, FALSE,
                             MUIA_Window_SizeGadget, FALSE, MUIA_Window_DepthGadget, FALSE, MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)),
                             MUIA_Window_TopEdge, (ULONG)(win->TopEdge + _bottom(obj) + 1), MUIA_Window_Width, (ULONG)_width(obj),
                             MUIA_Window_RootObject, (ULONG)group, TAG_DONE);
    if (d->popup == NULL) {
        MUI_DisposeObject(group);
        return;
    }
    d->list = list;
    DoMethod(app, OM_ADDMEMBER, (ULONG)d->popup);
    /* A pick sets AAC_Active on the cycle and closes the popup; the active row set above fired before these notifications exist. */
    DoMethod(list, MUIM_Notify, AAL_Picked, MUIV_EveryTime, obj, 3, MUIM_Set, AAC_Active, MUIV_TriggerValue);
    DoMethod(list, MUIM_Notify, AAL_Picked, MUIV_EveryTime, d->popup, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(d->popup, MUIM_Notify, MUIA_Window_Activate, FALSE, d->popup, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    set(d->popup, MUIA_Window_Open, TRUE);
    set(d->popup, MUIA_Window_ActiveObject, (ULONG)list); /* keys go to the list */
}

static ULONG aacycle_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aacycle_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    ULONG dis = 0, act = 0;
    int in;
    if (im == NULL) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    GetAttr(MUIA_Disabled, obj, &dis);
    in = im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj);
    if (im->Class == IDCMP_MOUSEMOVE) {
        if (in != d->hover) {
            d->hover = in;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
        return 0;
    }
    if (im->Class == IDCMP_MOUSEBUTTONS) {
        if (im->Code == SELECTDOWN && in && !dis) {
            d->down = 1;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
            return MUI_EventHandlerRC_Eat;
        }
        if (im->Code == SELECTUP && d->down) {
            d->down = 0;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
            if (in && !dis) {
                aacycle_open_popup(obj, d);
            }
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    if (im->Class == IDCMP_RAWKEY && !dis && aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &act);
        if ((Object*)act == obj && (im->Code == 0x4C || im->Code == 0x4D)) { /* cursor up, cursor down */
            set(obj, AAC_Active, (ULONG)mui_aa_cycle_step(d->active, d->n, im->Code == 0x4C ? -1 : 1));
            return MUI_EventHandlerRC_Eat;
        }
    }
    return 0;
}

static ULONG aacycle_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aacycle_data* d;
    struct TagItem *te = FindTagItem(AAC_Entries, msg->ops_AttrList), *ta = FindTagItem(AAC_Active, msg->ops_AttrList);
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)msg);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->entries = te != NULL ? (const char* const*)te->ti_Data : NULL;
    while (d->entries != NULL && d->entries[d->n] != NULL) {
        d->n++;
    }
    d->active = ta != NULL && mui_aa_cycle_valid((int)ta->ti_Data, d->n) ? (int)ta->ti_Data : 0;
    return (ULONG)obj;
}

static ULONG aacycle_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aacycle_data* d = INST_DATA(cl, obj);
    struct TagItem* ta = FindTagItem(AAC_Active, msg->ops_AttrList);
    if (ta != NULL && mui_aa_cycle_valid((int)ta->ti_Data, d->n) && (int)ta->ti_Data != d->active) {
        d->active = (int)ta->ti_Data;
        if (aa_live(obj)) {
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aacycle_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aacycle_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == AAC_Active) {
        *msg->opg_Storage = (ULONG)d->active;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aacycle_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aacycle_dispatch(void);
__asm__(".text\n.even\n.globl _aacycle_dispatch\n_aacycle_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aacycle_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aacycle_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aacycle_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aacycle_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aacycle_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        return aacycle_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aacycle_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aacycle_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aacycle_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aacycle_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aacycle_new(const mui_aacycle_spec* sp) {
    Object* o;
    if (g_cycle_class == NULL) {
        g_cycle_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Area, NULL, sizeof(aacycle_data), (APTR)(uintptr_t)aacycle_dispatch);
    }
    if (g_cycle_class == NULL) {
        return NULL;
    }
    o = NewObject(g_cycle_class->mcc_Class, NULL, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_CycleChain, (ULONG)TRUE, AAC_Entries, (ULONG)sp->entries,
                  AAC_Active, (ULONG)sp->active, TAG_DONE);
    if (o != NULL) {
        if (sp->id != 0) {
            DoMethod(o, MUIM_Notify, AAC_Active, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)sp->id);
        }
        g_cycle_made++;
    }
    return o;
}

uint32_t mui_aacycle_made(void) {
    return g_cycle_made;
}

void mui_aacycle_class_free(void) {
    if (g_cycle_class != NULL) {
        MUI_DeleteCustomClass(g_cycle_class);
        g_cycle_class = NULL;
    }
}
