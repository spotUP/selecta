/* Selecta_AAPanel (I667e, design section 3.8). MUI 3.8: MUI_CreateCustomClass on Group, same dispatcher pattern as
 * mui_aatext_class.c. The group carries the role colour as its MUIA_Background (children without a background of their own
 * inherit it). The Group super fills every gap between the children with that background, the square face included, so the class
 * paints after the super: only the corner wedges outside mui_aa_panel_radius, in the window BACKGROUND, through aa_round_corners;
 * on truecolour it first paints the face in exact RGB around the inner group (I685w). No hardcoded colour, no ObtainBestPen. */
#include "mui_aapanel_class.h"
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

#include "map/mui_theme.h"
#include "mui_aa_draw.h"
#include "map/mui_aa_paint.h"

extern struct Library* MUIMasterBase;

typedef struct aapanel_data {
    int role;
    int fixed;      /* > 0: the art frame, no pad */
    Object* inner;  /* I685e5: the plain vertical group that holds the program's children */
    aa_ctx aa;
} aapanel_data;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;
static struct Hook g_layout_hook; /* I685e5: one hook for every panel */
static char g_bg_spec[MUI_AA_PANEL_ROLES][40]; /* MUIA_Background strings; MUI keeps the pointer, so they live for the process */

/* I685e5: the pad comes from the screen the panel opened on (d->aa.aspect_q8, set in aa_setup), so it cannot be a creation-time
 * inner offsets (init-only in MUI 3.8, and no DrawInfo exists then). The layout hook adds it around the inner group. */
static void aapanel_pad(const aapanel_data* d, int* px, int* py) {
    *px = 0;
    *py = 0;
    if (d->fixed <= 0) {
        mui_aa_panel_pad(d->aa.tok, d->aa.aspect_q8, px, py);
        mui_aa_panel_role_pad(d->role, px, py);
    }
}

static ULONG aapanel_layout(struct Hook* hook, Object* obj, struct MUI_LayoutMsg* lm) {
    aapanel_data* d = INST_DATA(g_class->mcc_Class, obj);
    int px, py;
    (void)hook;
    aapanel_pad(d, &px, &py);
    switch (lm->lm_Type) {
    case MUILM_MINMAX: {
        Object* in = d->inner;
        if (in == NULL) {
            return 0;
        }
        lm->lm_MinMax.MinWidth = (WORD)(_minwidth(in) + 2 * px);
        lm->lm_MinMax.MinHeight = (WORD)(_minheight(in) + 2 * py);
        lm->lm_MinMax.DefWidth = (WORD)(_defwidth(in) + 2 * px);
        lm->lm_MinMax.DefHeight = (WORD)(_defheight(in) + 2 * py);
        lm->lm_MinMax.MaxWidth = _maxwidth(in) >= MUI_MAXMAX ? MUI_MAXMAX : (WORD)(_maxwidth(in) + 2 * px);
        lm->lm_MinMax.MaxHeight = _maxheight(in) >= MUI_MAXMAX ? MUI_MAXMAX : (WORD)(_maxheight(in) + 2 * py);
        return 0;
    }
    case MUILM_LAYOUT:
        if (d->inner != NULL) {
            MUI_Layout(d->inner, px, py, lm->lm_Layout.Width - 2 * px, lm->lm_Layout.Height - 2 * py, 0);
        }
        return TRUE;
    }
    return (ULONG)MUILM_UNKNOWN;
}

/* I685w run 6: the program's own row and column boxes (plain MUI Groups) inside the panel take the panel gap across and half of it
 * down, else they keep MUI's prefs spacing (2 px) and the card packs tight. The full gap down made the UP-Demo minimum taller than
 * the 600 row rig screen (the window did not open). A UP object, a nested panel included, keeps its own spacing. */
static void aapanel_space(Object* grp, int gx, int gy, int depth) {
    struct List* kids = NULL;
    Object *state, *child;
    struct IClass* group_cl = MUI_GetClass((char*)MUIC_Group);
    SetAttrs(grp, MUIA_Group_HorizSpacing, (ULONG)gx, MUIA_Group_VertSpacing, (ULONG)gy, TAG_DONE);
    GetAttr(MUIA_Group_ChildList, grp, (ULONG*)&kids);
    if (kids == NULL || depth >= 8 || group_cl == NULL) {
        return;
    }
    state = (Object*)kids->lh_Head;
    while ((child = NextObject(&state)) != NULL) {
        if (OCLASS(child) == group_cl) {
            aapanel_space(child, gx, depth == 0 ? gy / 2 : gy, depth + 1);
        }
    }
}

static ULONG aapanel_setup(struct IClass* cl, Object* obj, Msg msg) {
    aapanel_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    if (d->inner != NULL) { /* I685w run 6: the gap between children is its own rung of the spacing scale, not the card pad */
        int gx, gy;
        mui_aa_panel_gap(d->aa.tok, d->aa.aspect_q8, d->role, &gx, &gy);
        aapanel_space(d->inner, gx, gy, 0);
    }
    return TRUE;
}

static ULONG aapanel_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aapanel_data* d = INST_DATA(cl, obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

/* I685w run 6: the rectangles the face must not cover. A plain MUI Group (the program's own row and column boxes) fills its gaps
 * with MUI's pen-snapped copy of the inherited background, a second colour inside one card; so the walk goes through plain groups
 * (only the shown page of a page group) and keeps their leaves as holes. A UP object, a nested panel included, paints itself. */
static int aapanel_holes(Object* grp, int* holes, int n, int depth) {
    struct List* kids = NULL;
    Object *state, *child;
    static struct IClass* group_cl;
    LONG page = -1;
    ULONG pm = 0;
    if (group_cl == NULL) {
        group_cl = MUI_GetClass((char*)MUIC_Group);
    }
    GetAttr(MUIA_Group_ChildList, grp, (ULONG*)&kids);
    if (kids == NULL) {
        return n;
    }
    if (GetAttr(MUIA_Group_PageMode, grp, &pm) && pm) {
        ULONG ap = 0;
        GetAttr(MUIA_Group_ActivePage, grp, &ap);
        page = (LONG)ap;
    }
    state = (Object*)kids->lh_Head;
    {
        LONG i = 0;
        while (n < MUI_AA_PAINT_MAX_HOLES && (child = NextObject(&state)) != NULL) {
            if (page >= 0 && i++ != page) {
                continue;
            }
            if (depth < 8 && group_cl != NULL && OCLASS(child) == group_cl) {
                n = aapanel_holes(child, holes, n, depth + 1);
                continue;
            }
            holes[4 * n] = _left(child);
            holes[4 * n + 1] = _top(child);
            holes[4 * n + 2] = _width(child);
            holes[4 * n + 3] = _height(child);
            n++;
        }
    }
    return n;
}

static ULONG aapanel_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aapanel_data* d = INST_DATA(cl, obj);
    ULONG r = DoSuperMethodA(cl, obj, (Msg)msg);
    if (msg->flags & MADF_DRAWOBJECT) {
        if (d->aa.tc) { /* I685w: MUI's background spec snaps to a pen; paint the face in exact RGB around the children, whatever MUI filled */
            int holes[MUI_AA_PAINT_MAX_HOLES * 4], n = 0;
            Object* grp = d->inner != NULL ? d->inner : obj;
            n = aapanel_holes(grp, holes, 0, 0);
            {   /* I685w: the window margin around the root object is filled by MUI with the snapped spec too; the root repaints the ring in exact RGB */
                ULONG root = 0, wptr = 0;
                GetAttr(MUIA_Window_RootObject, _win(obj), &root);
                GetAttr(MUIA_Window_Window, _win(obj), &wptr);
                if ((Object*)root == obj && wptr != 0) {
                    struct Window* w = (struct Window*)wptr;
                    int ix0 = w->BorderLeft, iy0 = w->BorderTop, ix1 = w->Width - w->BorderRight - 1, iy1 = w->Height - w->BorderBottom - 1;
                    int ox0 = _left(obj), oy0 = _top(obj), ox1 = _left(obj) + _width(obj) - 1, oy1 = _top(obj) + _height(obj) - 1;
                    uint32_t face = mui_aa_panel_rgb(d->aa.tok, d->role);
                    if (oy0 > iy0) aa_rect(&d->aa, _rp(obj), ix0, iy0, ix1, oy0 - 1, face);
                    if (oy1 < iy1) aa_rect(&d->aa, _rp(obj), ix0, oy1 + 1, ix1, iy1, face);
                    if (ox0 > ix0) aa_rect(&d->aa, _rp(obj), ix0, oy0, ox0 - 1, oy1, face);
                    if (ox1 < ix1) aa_rect(&d->aa, _rp(obj), ox1 + 1, oy0, ix1, oy1, face);
                }
            }
            {   /* I685w: the object's own frame and inner spacing (outer box minus the inner box) is MUI-filled with the snapped spec as well */
                int ox0 = _left(obj), oy0 = _top(obj), ox1 = _left(obj) + _width(obj) - 1, oy1 = _top(obj) + _height(obj) - 1;
                int mx0 = _mleft(obj), my0 = _mtop(obj), mx1 = _mright(obj), my1 = _mbottom(obj);
                uint32_t face = mui_aa_panel_rgb(d->aa.tok, d->role);
                if (my0 > oy0) aa_rect(&d->aa, _rp(obj), ox0, oy0, ox1, my0 - 1, face);
                if (my1 < oy1) aa_rect(&d->aa, _rp(obj), ox0, my1 + 1, ox1, oy1, face);
                if (mx0 > ox0) aa_rect(&d->aa, _rp(obj), ox0, my0, mx0 - 1, my1, face);
                if (mx1 < ox1) aa_rect(&d->aa, _rp(obj), mx1 + 1, my0, ox1, my1, face);
            }
            aa_round_fill_holes(&d->aa, _rp(obj), _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj), mui_aa_panel_radius(d->aa.tok, d->aa.font_px),
                                mui_aa_panel_rgb(d->aa.tok, d->role), holes, n);
        }
        aa_round_corners(&d->aa, _rp(obj), _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj), mui_aa_panel_radius(d->aa.tok, d->aa.font_px), d->aa.tok[SEL_GUI_TOKEN_BACKGROUND]); /* I676c: read at draw, a live THEME changes both */
    }
    return r;
}

static void aapanel_hook_init(void) {
    if (g_layout_hook.h_Entry == NULL) {
        g_layout_hook.h_Entry = (__typeof__(g_layout_hook.h_Entry))(void (*)(void))HookEntry;
        g_layout_hook.h_SubEntry = (__typeof__(g_layout_hook.h_SubEntry))(void (*)(void))aapanel_layout;
    }
}

/* I685u / I685w run 6: created by name (UP-Panel.mcc, no UP_NewPanel) the object has no background tag, so MUI would paint the
 * window background. Add the role colour from the theme store, exactly what UP_NewPanel passes, and build the same shape: the
 * program's MUIA_Group_Child objects go into the inner group and the layout hook puts the card pad around it. Without the inner
 * group and the hook the pad (aapanel_pad) was never reached for a by-name panel: children sat on the card edge. */
static ULONG aapanel_new_by_name(struct IClass* cl, Object* obj, struct opSet* ops, int role) {
    struct TagItem *tag, *state = ops->ops_AttrList, *copy, *t;
    struct opSet nm = *ops;
    Object* inner;
    ULONG made, horiz = GetTagData(MUIA_Group_Horiz, FALSE, ops->ops_AttrList);
    int count = 0;
    while (NextTagItem(&state) != NULL) {
        count++;
    }
    copy = (struct TagItem*)AllocVec((ULONG)(count + 8) * sizeof(struct TagItem), MEMF_ANY);
    inner = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, horiz, MUIA_FillArea, (ULONG)FALSE, TAG_DONE);
    if (copy == NULL || inner == NULL) {
        if (inner != NULL) {
            MUI_DisposeObject(inner);
        }
        if (copy != NULL) {
            FreeVec(copy);
        }
        return 0;
    }
    mui_theme_bg_spec(mui_aa_panel_rgb(mui_theme_tokens(), role), g_bg_spec[role], sizeof g_bg_spec[role]);
    aapanel_hook_init();
    t = copy;
    t->ti_Tag = MUIA_Background, (t++)->ti_Data = (ULONG)g_bg_spec[role];
    t->ti_Tag = MUIA_FillArea, (t++)->ti_Data = (ULONG)FALSE; /* I685w: the draw paints the face; a MUI fill would be the snapped pen */
    t->ti_Tag = MUIA_Frame, (t++)->ti_Data = (ULONG)MUIV_Frame_None;
    t->ti_Tag = MUIA_Group_Spacing, (t++)->ti_Data = 0;
    t->ti_Tag = MUIA_Group_LayoutHook, (t++)->ti_Data = (ULONG)(uintptr_t)&g_layout_hook;
    t->ti_Tag = MUIA_Group_Child, (t++)->ti_Data = (ULONG)(uintptr_t)inner;
    state = ops->ops_AttrList;
    while ((tag = NextTagItem(&state)) != NULL) {
        if (tag->ti_Tag == MUIA_Group_Child) {
            if (tag->ti_Data != 0) {
                DoMethod(inner, OM_ADDMEMBER, tag->ti_Data);
            }
        } else if (tag->ti_Tag != MUIA_Group_Horiz && tag->ti_Tag != MUIA_Background && tag->ti_Tag != MUIA_FillArea) {
            *t++ = *tag;
        }
    }
    t->ti_Tag = TAG_DONE;
    nm.ops_AttrList = copy;
    made = DoSuperMethodA(cl, obj, (Msg)&nm); /* on failure the Group super disposes its children, the inner group with them */
    FreeVec(copy);
    if (made != 0) { /* I685w: the by-name panel has no UP_NewPanel to record the role the face is painted in */
        aapanel_data* d = (aapanel_data*)INST_DATA(cl, (Object*)made);
        d->role = role;
        d->inner = inner;
        g_made++;
    }
    return made;
}

static ULONG __attribute__((used)) aapanel_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aapanel_dispatch(void);
const struct aa_class_desc aapanel_class_desc = {MUIC_Group, sizeof(aapanel_data), aapanel_dispatch, &g_class};
__asm__(".text\n.even\n.globl _aapanel_dispatch\n_aapanel_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapanel_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aapanel_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW: {
        struct opSet* ops = (struct opSet*)msg;
        if (FindTagItem(MUIA_Background, ops->ops_AttrList) == NULL) {
            int role = (int)GetTagData(UPA_Panel_Role, UPV_Panel_Role_Panel, ops->ops_AttrList);
            if ((unsigned)role < MUI_AA_PANEL_ROLES) {
                return aapanel_new_by_name(cl, obj, ops, role);
            }
        }
        break;
    }
    case OM_GET: { /* I685w: children read the role to take their erase colour from the panel they sit in */
        struct opGet* og = (struct opGet*)msg;
        if (og->opg_AttrID == UPA_Panel_Role) {
            *og->opg_Storage = (ULONG)((aapanel_data*)INST_DATA(cl, obj))->role;
            return TRUE;
        }
        if (og->opg_AttrID == UPA_Panel_Self) {
            *og->opg_Storage = (ULONG)(uintptr_t)obj;
            return TRUE;
        }
        break;
    }
    case MUIM_Setup:
        return aapanel_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aapanel_cleanup(cl, obj, msg);
    case MUIM_Draw:
        return aapanel_draw(cl, obj, (struct MUIP_Draw*)msg);
    case OM_ADDMEMBER:
    case OM_REMMEMBER: {
        aapanel_data* d = INST_DATA(cl, obj);
        if (d->inner != NULL) { /* I685e5: callers add to the panel, the children live in the inner group */
            return DoMethodA(d->inner, msg);
        }
        break;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Tags: UPA_Panel_Tokens (required), UPA_Panel_Role, UPA_Panel_FixedWidth, UPA_Panel_FixedHeight. The inner group is built here (design 4.1.1). */
Object* UP_NewPanel(const struct TagItem* tags) {
    Object *o, *inner;
    const uint32_t* tokens = (const uint32_t*)GetTagData(UPA_Panel_Tokens, 0, (struct TagItem*)tags);
    int role = (int)GetTagData(UPA_Panel_Role, UPV_Panel_Role_Panel, (struct TagItem*)tags);
    int fixed_w = (int)GetTagData(UPA_Panel_FixedWidth, 0, (struct TagItem*)tags);
    int fixed_h = (int)GetTagData(UPA_Panel_FixedHeight, 0, (struct TagItem*)tags);
    if ((unsigned)role >= MUI_AA_PANEL_ROLES || tokens == NULL) {
        return NULL;
    }
    if (g_class == NULL) {
        g_class = AA_CLASS_CREATE(&aapanel_class_desc);
    }
    if (g_class == NULL) {
        return NULL;
    }
    aapanel_hook_init();
    inner = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, (ULONG)FALSE, MUIA_FillArea, (ULONG)FALSE, TAG_DONE); /* I685w: no snapped MUI fill under the children */
    if (inner == NULL) {
        return NULL;
    }
    mui_theme_bg_spec(mui_aa_panel_rgb(tokens, role), g_bg_spec[role], sizeof g_bg_spec[role]);
    o = NewObject(g_class->mcc_Class, NULL, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_Background, (ULONG)g_bg_spec[role], MUIA_FillArea, (ULONG)FALSE,
                  MUIA_Group_Spacing, (ULONG)0, MUIA_Group_LayoutHook, (ULONG)(uintptr_t)&g_layout_hook, MUIA_Group_Child, (ULONG)(uintptr_t)inner,
                  fixed_w > 0 ? (ULONG)MUIA_FixWidth : (ULONG)TAG_IGNORE, (ULONG)fixed_w,
                  fixed_w > 0 ? (ULONG)MUIA_FixHeight : (ULONG)TAG_IGNORE, (ULONG)(fixed_h > 0 ? fixed_h : fixed_w), TAG_DONE);
    if (o == NULL) {
        MUI_DisposeObject(inner);
    } else {
        aapanel_data* d = INST_DATA(g_class->mcc_Class, o);
        d->role = role;
        d->fixed = fixed_w;
        d->inner = inner;
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
