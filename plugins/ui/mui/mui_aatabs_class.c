/* Selecta_AATabs (I667j, design section 3.9). MUI 3.8: MUI_CreateCustomClass on Group, same dispatcher pattern as mui_aapanel_class.c.
 * The children are AAButtons of role TAB (mui_aabtn_class.c): a click on one sets AATab_Active on the strip (MUIM_Notify with MUIM_Set),
 * the strip lights the active caption (mui_aabtn_set_on) and, after the super class has drawn the children, paints the 2 px ACCENT
 * underline under the active tab through mui_aa_tabs_line / mui_aa_tabs_rgb. Cursor left and right move the active tab while one of
 * the tab buttons has the focus (mui_aa_tabs_key). No hardcoded colour, no ObtainBestPen. */
#include "mui_aatabs_class.h"

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

#include "mui_aa_draw.h"
#include "mui_aabtn_class.h"

extern struct Library* MUIMasterBase;

typedef struct aatabs_data {
    int n, active;
    Object* btn[MUI_AATABS_MAX];
    aa_ctx aa;
    LONG line_pen;
    struct MUI_EventHandlerNode ehn;
} aatabs_data;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;

static void aatabs_sync(aatabs_data* d) {
    int i;
    for (i = 0; i < d->n; i++) {
        mui_aabtn_set_on(d->btn[i], i == d->active);
    }
}

static ULONG aatabs_setup(struct IClass* cl, Object* obj, Msg msg) {
    aatabs_data* d = INST_DATA(cl, obj);
    uint32_t line;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_tabs_rgb(d->aa.tok, &line, NULL, NULL);
    d->line_pen = aa_pen(&d->aa, line);
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_RAWKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aatabs_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aatabs_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aatabs_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aatabs_data* d = INST_DATA(cl, obj);
    ULONG r = DoSuperMethodA(cl, obj, (Msg)msg);
    if ((msg->flags & MADF_DRAWOBJECT) && d->n > 0) {
        Object* b = d->btn[d->active];
        int lx, ly, lw, lh;
        mui_aa_tabs_line(d->aa.font_px, _mleft(b), _mtop(b), _mwidth(b), _mheight(b), &lx, &ly, &lw, &lh);
        SetAPen(_rp(obj), (ULONG)d->line_pen);
        RectFill(_rp(obj), lx, ly, lx + lw - 1, ly + lh - 1);
    }
    return r;
}

static ULONG aatabs_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aatabs_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    ULONG act = 0;
    int i, next;
    if (im == NULL || im->Class != IDCMP_RAWKEY || (im->Code != 0x4E && im->Code != 0x4F) || !aa_live(obj)) {
        return 0;
    }
    GetAttr(MUIA_Window_ActiveObject, _win(obj), &act);
    for (i = 0; i < d->n; i++) {
        if ((Object*)act == d->btn[i]) {
            next = mui_aa_tabs_key(im->Code, d->active, d->n);
            if (next != d->active) {
                set(obj, AATab_Active, (ULONG)next);
            }
            return MUI_EventHandlerRC_Eat;
        }
    }
    return 0;
}

static ULONG aatabs_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aatabs_data* d = INST_DATA(cl, obj);
    struct TagItem* ta = FindTagItem(AATab_Active, msg->ops_AttrList);
    if (ta != NULL && mui_aa_tabs_click((int)ta->ti_Data, d->n) >= 0 && (int)ta->ti_Data != d->active) {
        d->active = (int)ta->ti_Data;
        aatabs_sync(d);
        if (aa_live(obj)) {
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aatabs_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aatabs_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == AATab_Active) {
        *msg->opg_Storage = (ULONG)d->active;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aatabs_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aatabs_dispatch(void);
__asm__(".text\n.even\n.globl _aatabs_dispatch\n_aatabs_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aatabs_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aatabs_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_SET:
        return aatabs_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aatabs_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        return aatabs_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aatabs_cleanup(cl, obj, msg);
    case MUIM_Draw:
        return aatabs_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aatabs_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aatabs_new(const mui_aatabs_spec* sp) {
    Object* o;
    aatabs_data* d;
    int i, n = 0;
    while (sp->captions != NULL && sp->captions[n] != NULL && n < MUI_AATABS_MAX) {
        n++;
    }
    if (n == 0) {
        return NULL;
    }
    if (g_class == NULL) {
        g_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Group, NULL, sizeof(aatabs_data), (APTR)(uintptr_t)aatabs_dispatch);
    }
    if (g_class == NULL) {
        return NULL;
    }
    o = NewObject(g_class->mcc_Class, NULL, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_Group_Spacing, (ULONG)0, MUIA_Frame, (ULONG)MUIV_Frame_None, TAG_DONE);
    if (o == NULL) {
        return NULL;
    }
    d = INST_DATA(g_class->mcc_Class, o);
    memset(d, 0, sizeof *d);
    d->n = n;
    d->active = mui_aa_tabs_click(sp->active, n) >= 0 ? sp->active : 0;
    for (i = 0; i < n; i++) {
        mui_aabtn_spec bs;
        memset(&bs, 0, sizeof bs);
        bs.caption = sp->captions[i];
        bs.widest = sp->captions[i];
        bs.icon = -1;
        bs.alt_icon = -1;
        bs.role = MUI_AA_ROLE_TAB;
        bs.id = 0;
        bs.weight = 0;
        bs.align = MUI_AA_ALIGN_CENTRE;
        d->btn[i] = mui_aabtn_new(&bs);
        if (d->btn[i] == NULL) {
            MUI_DisposeObject(o);
            return NULL;
        }
        DoMethod(o, OM_ADDMEMBER, d->btn[i]);
        DoMethod(d->btn[i], MUIM_Notify, MUIA_Pressed, FALSE, o, 3, MUIM_Set, AATab_Active, (ULONG)i);
    }
    DoMethod(o, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE)); /* the strip packs left */
    aatabs_sync(d);
    g_made++;
    return o;
}

uint32_t mui_aatabs_made(void) {
    return g_made;
}

void mui_aatabs_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
