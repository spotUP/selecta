/* I731: see mui_aatip.h. One process-wide tip (only one is ever visible): g_tip holds the owner, the hover state, the MUI timer node
 * and the window. Colours and sizes come from mui_aa_tip_plan only; drawing is aa_round_fill and aa_text of the owner's aa_ctx. */
#include "mui_aatip.h"

#include <stdio.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "map/mui_aa.h"

static struct {
    Object* owner;
    Object* app;
    int state, armed;
    struct MUI_InputHandlerNode ihn;
    struct Window* win;
} g_tip;
static uint32_t g_shows;

ULONG aatip_super_set(struct IClass* cl, Object* obj, struct opSet* msg, ULONG slot_off) {
    struct TagItem *tag, *list = msg->ops_AttrList, *held_text = NULL, *held_side = NULL;
    const char* text = NULL;
    int side = 0;
    ULONG r;
    Object* target;
    while ((tag = NextTagItem(&list)) != NULL) {
        if (tag->ti_Tag == MUIA_ShortHelp) {
            text = (const char*)tag->ti_Data;
            held_text = tag;
        } else if (tag->ti_Tag == AATIP_A_Side) {
            side = (int)tag->ti_Data;
            held_side = tag;
        }
    }
    if (held_text != NULL) {
        held_text->ti_Tag = TAG_IGNORE;
    }
    if (held_side != NULL) {
        held_side->ti_Tag = TAG_IGNORE;
    }
    r = DoSuperMethodA(cl, obj, (Msg)msg);
    if (held_text != NULL) {
        held_text->ti_Tag = MUIA_ShortHelp;
    }
    if (held_side != NULL) {
        held_side->ti_Tag = AATIP_A_Side;
    }
    target = msg->MethodID == OM_NEW ? (Object*)r : obj;
    if (target != NULL && (held_text != NULL || held_side != NULL)) {
        aatip_slot* t = (aatip_slot*)((UBYTE*)INST_DATA(cl, target) + slot_off);
        if (held_text != NULL) {
            t->text = text;
        }
        if (held_side != NULL) {
            t->side = side;
        }
    }
    return r;
}

int aatip_get(struct opGet* msg, const aatip_slot* t) {
    if (msg->opg_AttrID == MUIA_ShortHelp) {
        *msg->opg_Storage = (ULONG)t->text;
        return 1;
    }
    return 0;
}

static void tip_disarm(void) {
    if (g_tip.armed) {
        g_tip.armed = 0;
        DoMethod(g_tip.app, MUIM_Application_RemInputHandler, (ULONG)&g_tip.ihn);
    }
}

static void tip_arm(Object* obj) {
    ULONG app = 0;
    if (g_tip.armed) {
        return;
    }
    GetAttr(MUIA_ApplicationObject, _win(obj), &app);
    if (app == 0) {
        return;
    }
    g_tip.app = (Object*)app;
    g_tip.ihn.ihn_Object = obj;
    g_tip.ihn.ihn_Flags = MUIIHNF_TIMER;
    g_tip.ihn.ihn_Millis = MUI_AA_TIP_DELAY_MS;
    g_tip.ihn.ihn_Method = AATIP_M_FIRE;
    g_tip.armed = 1;
    DoMethod(g_tip.app, MUIM_Application_AddInputHandler, (ULONG)&g_tip.ihn);
}

static void tip_hide(void) {
    if (g_tip.win != NULL) {
        CloseWindow(g_tip.win);
        g_tip.win = NULL;
    }
}

void aatip_drop(Object* obj) {
    if (obj != NULL && g_tip.owner != obj) {
        return;
    }
    tip_disarm();
    tip_hide();
    g_tip.state = MUI_AA_TIP_IDLE;
    g_tip.owner = NULL;
}

static void tip_run(int ev, Object* obj) {
    int act = MUI_AA_TIP_DO_NONE;
    g_tip.state = mui_aa_tip_step(g_tip.state, ev, &act);
    if (act & MUI_AA_TIP_DO_DISARM) {
        tip_disarm();
    }
    if (act & MUI_AA_TIP_DO_HIDE) {
        tip_hide();
    }
    if (act & MUI_AA_TIP_DO_ARM) {
        tip_arm(obj);
    }
}

void aatip_event(Object* obj, aatip_slot* t, const struct IntuiMessage* im, int in) {
    int ev = -1;
    if (im == NULL) {
        return;
    }
    if (im->Class == IDCMP_MOUSEMOVE) {
        if (in && !t->in) {
            t->in = 1;
            ev = MUI_AA_TIP_EV_ENTER;
        } else if (!in && t->in) {
            t->in = 0;
            ev = MUI_AA_TIP_EV_LEAVE;
        }
    } else if (im->Class == IDCMP_MOUSEBUTTONS) {
        ev = MUI_AA_TIP_EV_PRESS;
    } else if (im->Class == IDCMP_RAWKEY) {
        ev = MUI_AA_TIP_EV_KEY;
    } else if (im->Class == IDCMP_INACTIVEWINDOW) {
        t->in = 0;
        ev = MUI_AA_TIP_EV_INACTIVE;
    }
    if (ev < 0) {
        return;
    }
    if (ev == MUI_AA_TIP_EV_ENTER) {
        if (t->text == NULL || t->text[0] == '\0') {
            return;
        }
        if (g_tip.owner != obj) {
            aatip_drop(NULL); /* the old owner's tip leaves before this one waits its own delay */
            g_tip.owner = obj;
        }
    } else if (g_tip.owner != obj) {
        return;
    }
    tip_run(ev, obj);
}

ULONG aatip_fire(Object* obj, aa_ctx* aa, const aatip_slot* t) {
    int act = MUI_AA_TIP_DO_NONE, tw, th, x = 0, y = 0, side;
    mui_aa_tip_t p;
    struct Window* w;
    struct RastPort* rp;
    if (g_tip.owner != obj) {
        tip_disarm();
        return 0;
    }
    g_tip.state = mui_aa_tip_step(g_tip.state, MUI_AA_TIP_EV_TIMER, &act);
    if (act & MUI_AA_TIP_DO_DISARM) { /* MUI timers repeat: the one shot disarms first */
        tip_disarm();
    }
    if (!(act & MUI_AA_TIP_DO_SHOW) || t->text == NULL || !aa_live(obj) || !(_window(obj)->Flags & WFLG_WINDOWACTIVE)) {
        return 0;
    }
    tw = aa_text_width(aa, obj, t->text);
    th = aa_text_height(aa, obj);
    mui_aa_tip_plan(aa->tok, aa->font_px, tw, th, &p);
    side = mui_aa_tip_place(_window(obj)->LeftEdge + _left(obj), _window(obj)->TopEdge + _top(obj), _width(obj), _height(obj), p.w, p.h, _screen(obj)->Width,
                            _screen(obj)->Height, p.gap, t->side, &x, &y);
    w = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)_screen(obj), WA_Left, (ULONG)x, WA_Top, (ULONG)y, WA_Width, (ULONG)p.w, WA_Height, (ULONG)p.h, WA_Borderless, TRUE,
                       WA_Activate, FALSE, WA_SmartRefresh, TRUE, WA_NoCareRefresh, TRUE, WA_RMBTrap, TRUE, TAG_DONE);
    if (w == NULL) {
        return 0;
    }
    g_tip.win = w;
    rp = w->RPort;
    SetAPen(rp, aa_pen(aa, p.corner));
    RectFill(rp, 0, 0, p.w - 1, p.h - 1);
    aa_round_fill(rp, 0, 0, p.w, p.h, p.radius, aa_pen(aa, p.face));
    aa_text(aa, obj, rp, p.pad_x, p.pad_y, t->text, p.text, p.face);
    if (g_shows++ == 0) {
        fprintf(stderr, "selecta mui: tip shown side=%d w=%d h=%d backend=%s\n", side, p.w, p.h, aa->backend == AA_TC ? "TC" : aa->backend == AA_PAL ? "PAL" : "PLAIN");
    }
    return 0;
}

uint32_t aatip_shows(void) {
    return g_shows;
}
