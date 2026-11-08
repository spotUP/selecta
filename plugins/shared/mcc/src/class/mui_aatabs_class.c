/* Selecta_AATabs (I667j, design section 3.9). MUI 3.8: MUI_CreateCustomClass on Group, same dispatcher pattern as mui_aapanel_class.c.
 * The children are AAButtons of role TAB (mui_aabtn_class.c): a click on one sets UPA_Tabs_Active on the strip (MUIM_Notify with MUIM_Set),
 * the strip lights the active caption (mui_aabtn_set_on) and, after the super class has drawn the children, paints the 2 px ACCENT
 * underline under the active tab through mui_aa_tabs_line / mui_aa_tabs_rgb. Cursor left and right move the active tab while one of
 * the tab buttons has the focus (mui_aa_tabs_key). No hardcoded colour, no ObtainBestPen. */
#include "mui_aatabs_class.h"
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
#include "mui_aabtn_class.h"
#include "mui_aatext_class.h"

extern struct Library* MUIMasterBase;

typedef struct aatabs_data {
    int n, active;
    Object* btn[MUI_AATABS_MAX];
    aa_ctx aa;
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
    aa_hold(&d->aa, line);
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
    ULONG r;
    if ((msg->flags & MADF_DRAWOBJECT) && d->aa.tc) {
        /* I685w run 6: the strip has MUIA_FillArea FALSE; on truecolour paint the gaps between the chips in the exact panel token, not MUI's snapped pen */
        aa_rect(&d->aa, _rp(obj), _mleft(obj), _mtop(obj), _mright(obj), _mbottom(obj), d->aa.bg_rgb);
    }
    r = DoSuperMethodA(cl, obj, (Msg)msg);
    if ((msg->flags & MADF_DRAWOBJECT) && d->n > 0) {
        Object* b = d->btn[d->active];
        int lx, ly, lw, lh;
        uint32_t line;
        mui_aa_tabs_rgb(d->aa.tok, &line, NULL, NULL);
        mui_aa_tabs_line(d->aa.font_px, _mleft(b), _mtop(b), _mwidth(b), _mheight(b), &lx, &ly, &lw, &lh);
        aa_rect(&d->aa, _rp(obj), lx, ly, lx + lw - 1, ly + lh - 1, line);
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
                set(obj, UPA_Tabs_Active, (ULONG)next);
            }
            return MUI_EventHandlerRC_Eat;
        }
    }
    return 0;
}

static ULONG aatabs_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aatabs_data* d = INST_DATA(cl, obj);
    struct TagItem* ta = FindTagItem(UPA_Tabs_Active, msg->ops_AttrList);
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
    if (msg->opg_AttrID == UPA_Tabs_Active) {
        *msg->opg_Storage = (ULONG)d->active;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aatabs_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

/* OM_NEW: UPA_Tabs_* parsed once for UP_NewTabs and a by-name MUI_NewObject("UP-Tabs.mcc"): one button per caption, then a spacer
 * so the strip packs left. No captions fails the object. */
static ULONG aatabs_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aatabs_data* d;
    struct TagItem def[5];
    struct opSet ns = *msg;
    int i, n = 0, active;
    char** captions = (char**)GetTagData(UPA_Tabs_Captions, 0, msg->ops_AttrList);
    while (captions != NULL && captions[n] != NULL && n < MUI_AATABS_MAX) {
        n++;
    }
    if (n == 0) {
        return 0;
    }
    def[0].ti_Tag = MUIA_Group_Horiz, def[0].ti_Data = (ULONG)TRUE;
    def[1].ti_Tag = MUIA_Group_Spacing, def[1].ti_Data = 0;
    def[2].ti_Tag = MUIA_Frame, def[2].ti_Data = (ULONG)MUIV_Frame_None;
    def[3].ti_Tag = MUIA_FillArea, def[3].ti_Data = (ULONG)FALSE;
    def[4].ti_Tag = TAG_MORE, def[4].ti_Data = (ULONG)(uintptr_t)msg->ops_AttrList;
    ns.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&ns);
    if (obj == NULL) {
        return 0;
    }
    active = (int)GetTagData(UPA_Tabs_Active, 0, msg->ops_AttrList);
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->n = n;
    d->active = mui_aa_tabs_click(active, n) >= 0 ? active : 0;
    for (i = 0; i < n; i++) {
        struct TagItem bt[] = {{UPA_Button_Caption, (ULONG)(uintptr_t)captions[i]},
                               {UPA_Button_Widest, (ULONG)(uintptr_t)captions[i]},
                               {UPA_Button_Role, UPV_Button_Role_Tab},
                               {UPA_Button_Align, UPV_Button_Align_Centre},
                               {MUIA_Weight, 0},
                               {MUIA_ShortHelp, (ULONG)(uintptr_t)captions[i]},
                               {TAG_DONE, 0}};
        d->btn[i] = UP_NewButton(bt);
        if (d->btn[i] == NULL) {
            CoerceMethod(cl, obj, OM_DISPOSE);
            return 0;
        }
        if (i > 0) {
            /* I685w run 6: a one-space UP-Text keeps the chips apart; a group spacing would fill the gap with MUI's snapped pen */
            struct TagItem gt[] = {{UPA_Text_Contents, (ULONG)(uintptr_t)" "}, {MUIA_Weight, 0}, {TAG_DONE, 0}};
            DoMethod(obj, OM_ADDMEMBER, UP_NewText(gt));
        }
        DoMethod(obj, OM_ADDMEMBER, d->btn[i]);
        DoMethod(d->btn[i], MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MUIM_Set, UPA_Tabs_Active, (ULONG)i);
    }
    {
        /* an UP-Text spacer erases in the panel token; a stock Rectangle would fill with MUI's snapped background pen */
        struct TagItem st[] = {{UPA_Text_Contents, (ULONG)(uintptr_t)" "}, {TAG_DONE, 0}};
        DoMethod(obj, OM_ADDMEMBER, UP_NewText(st));
    }
    aatabs_sync(d);
    return (ULONG)obj;
}

extern ULONG aatabs_dispatch(void);
const struct aa_class_desc aatabs_class_desc = {MUIC_Group, sizeof(aatabs_data), aatabs_dispatch, &g_class};
__asm__(".text\n.even\n.globl _aatabs_dispatch\n_aatabs_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aatabs_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aatabs_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aatabs_new(cl, obj, (struct opSet*)msg);
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

Object* UP_NewTabs(const struct TagItem* tags) {
    Object* o;
    if (g_class == NULL) {
        g_class = AA_CLASS_CREATE(&aatabs_class_desc);
    }
    if (g_class == NULL) {
        return NULL;
    }
    o = NewObjectA(g_class->mcc_Class, NULL, (struct TagItem*)tags);
    if (o != NULL) {
        g_made++;
    }
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
