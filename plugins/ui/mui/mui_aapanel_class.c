/* Selecta_AAPanel (I667e, design section 3.8). MUI 3.8: MUI_CreateCustomClass on Group, same dispatcher pattern as
 * mui_aatext_class.c. The group carries the role colour as its MUIA_Background (children without a background of their own
 * inherit it). The Group super fills every gap between the children with that background, the square face included, so the class
 * paints after the super: only the corner wedges outside mui_aa_panel_radius, in the window BACKGROUND, through aa_round_corners
 * (held pen). No hardcoded colour, no ObtainBestPen. */
#include "mui_aapanel_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "map/mui_theme.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aapanel_data {
    int role;
    aa_ctx aa;
} aapanel_data;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;
static char g_bg_spec[MUI_AA_PANEL_ROLES][40]; /* MUIA_Background strings; MUI keeps the pointer, so they live for the process */

static ULONG aapanel_setup(struct IClass* cl, Object* obj, Msg msg) {
    aapanel_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    return TRUE;
}

static ULONG aapanel_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aapanel_data* d = INST_DATA(cl, obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aapanel_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aapanel_data* d = INST_DATA(cl, obj);
    ULONG r = DoSuperMethodA(cl, obj, (Msg)msg);
    if (msg->flags & MADF_DRAWOBJECT) {
        aa_round_corners(_rp(obj), _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj), mui_aa_panel_radius(d->aa.tok, d->aa.font_px), aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_BACKGROUND])); /* I676c: read at draw, a live THEME changes both */
    }
    return r;
}

static ULONG __attribute__((used)) aapanel_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aapanel_dispatch(void);
__asm__(".text\n.even\n.globl _aapanel_dispatch\n_aapanel_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapanel_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aapanel_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case MUIM_Setup:
        return aapanel_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aapanel_cleanup(cl, obj, msg);
    case MUIM_Draw:
        return aapanel_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aapanel_new(const mui_aapanel_spec* sp) {
    Object* o;
    int pad;
    if ((unsigned)sp->role >= MUI_AA_PANEL_ROLES) {
        return NULL;
    }
    if (g_class == NULL) {
        g_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Group, NULL, sizeof(aapanel_data), (APTR)(uintptr_t)aapanel_dispatch);
    }
    if (g_class == NULL) {
        return NULL;
    }
    pad = sp->fixed_px > 0 ? 0 : mui_aa_panel_spacing(sp->tokens);
    mui_theme_bg_spec(mui_aa_panel_rgb(sp->tokens, sp->role), g_bg_spec[sp->role], sizeof g_bg_spec[sp->role]);
    o = NewObject(g_class->mcc_Class, NULL, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_Background, (ULONG)g_bg_spec[sp->role], MUIA_FillArea, (ULONG)FALSE,
                  MUIA_Group_Spacing, (ULONG)pad, MUIA_InnerLeft, (ULONG)pad, MUIA_InnerRight, (ULONG)pad, MUIA_InnerTop, (ULONG)pad,
                  MUIA_InnerBottom, (ULONG)pad, sp->fixed_px > 0 ? (ULONG)MUIA_FixWidth : (ULONG)TAG_IGNORE, (ULONG)sp->fixed_px,
                  sp->fixed_px > 0 ? (ULONG)MUIA_FixHeight : (ULONG)TAG_IGNORE, (ULONG)sp->fixed_px, TAG_DONE);
    if (o != NULL) {
        aapanel_data* d = INST_DATA(g_class->mcc_Class, o);
        d->role = sp->role;
        g_made++;
    }
    return o;
}

uint32_t mui_aapanel_made(void) {
    return g_made;
}

void mui_aapanel_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
