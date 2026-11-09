/* I731 (plan 2026-10-31-I731-mui-flat-tooltips.md section 2.1): pure plan, placement and hover state of the flat MUI tooltip.
 * No Amiga calls, so the host test reaches them. The Amiga module (../mui_aatip.c) draws and times; every colour and number is
 * picked here. */
#include <stddef.h>

#include "mui_aa.h"

static uint32_t g_tip_calls;

uint32_t mui_aa_tip_calls(void) {
    return g_tip_calls;
}

void mui_aa_tip_plan(const uint32_t* token, int font_px, int text_w, int text_h, mui_aa_tip_t* out) {
    g_tip_calls++;
    out->face = token[SEL_GUI_TOKEN_SURFACE_PRESSED];
    out->text = token[SEL_GUI_TOKEN_TEXT];
    out->corner = token[SEL_GUI_TOKEN_SURFACE];
    if (font_px <= 0) {
        out->pad_x = 6;
        out->pad_y = 3;
        out->radius = 4;
        out->gap = 2;
    } else {
        out->pad_x = (int)token[SEL_GUI_TOKEN_SPACING];
        out->pad_y = out->pad_x / 2;
        out->radius = (int)token[SEL_GUI_TOKEN_RADIUS_SMALL];
        out->gap = 4;
    }
    out->w = text_w + 2 * out->pad_x;
    out->h = text_h + 2 * out->pad_y;
    if (out->radius > out->h / 2) {
        out->radius = out->h / 2;
    }
}

int mui_aa_tip_place(int ax, int ay, int aw, int ah, int w, int h, int scr_w, int scr_h, int gap, int pref, int* x, int* y) {
    int side = pref, px = 0, py = 0, k;
    for (k = 0; k < 2; k++) {
        int fits;
        if (side == MUI_AA_TIP_ABOVE || side == MUI_AA_TIP_BELOW) {
            px = ax + (aw - w) / 2;
            py = side == MUI_AA_TIP_ABOVE ? ay - gap - h : ay + ah + gap;
            fits = py >= 0 && py + h <= scr_h;
        } else {
            py = ay + (ah - h) / 2;
            px = side == MUI_AA_TIP_RIGHT ? ax + aw + gap : ax - gap - w;
            fits = px >= 0 && px + w <= scr_w;
        }
        if (fits || k == 1) {
            break;
        }
        side = side == MUI_AA_TIP_ABOVE ? MUI_AA_TIP_BELOW : side == MUI_AA_TIP_BELOW ? MUI_AA_TIP_ABOVE : side == MUI_AA_TIP_RIGHT ? MUI_AA_TIP_LEFT : MUI_AA_TIP_RIGHT;
    }
    if (px > scr_w - w) {
        px = scr_w - w;
    }
    if (py > scr_h - h) {
        py = scr_h - h;
    }
    if (px < 0) {
        px = 0;
    }
    if (py < 0) {
        py = 0;
    }
    *x = px;
    *y = py;
    return side;
}

int mui_aa_tip_step(int state, int ev, int* actions) {
    int next = state, act = MUI_AA_TIP_DO_NONE;
    switch (state) {
    case MUI_AA_TIP_IDLE:
        if (ev == MUI_AA_TIP_EV_ENTER) {
            next = MUI_AA_TIP_PENDING;
            act = MUI_AA_TIP_DO_ARM;
        } else if (ev == MUI_AA_TIP_EV_TIMER) {
            act = MUI_AA_TIP_DO_DISARM;
        }
        break;
    case MUI_AA_TIP_PENDING:
        if (ev == MUI_AA_TIP_EV_TIMER) {
            next = MUI_AA_TIP_SHOWN;
            act = MUI_AA_TIP_DO_DISARM | MUI_AA_TIP_DO_SHOW;
        } else if (ev == MUI_AA_TIP_EV_LEAVE || ev == MUI_AA_TIP_EV_INACTIVE || ev == MUI_AA_TIP_EV_GONE) {
            next = MUI_AA_TIP_IDLE;
            act = MUI_AA_TIP_DO_DISARM;
        } else if (ev == MUI_AA_TIP_EV_PRESS || ev == MUI_AA_TIP_EV_KEY) {
            next = MUI_AA_TIP_MUTED;
            act = MUI_AA_TIP_DO_DISARM;
        }
        break;
    case MUI_AA_TIP_SHOWN:
        if (ev == MUI_AA_TIP_EV_TIMER) {
            act = MUI_AA_TIP_DO_DISARM;
        } else if (ev == MUI_AA_TIP_EV_LEAVE || ev == MUI_AA_TIP_EV_INACTIVE || ev == MUI_AA_TIP_EV_GONE) {
            next = MUI_AA_TIP_IDLE;
            act = MUI_AA_TIP_DO_HIDE;
        } else if (ev == MUI_AA_TIP_EV_PRESS || ev == MUI_AA_TIP_EV_KEY) {
            next = MUI_AA_TIP_MUTED;
            act = MUI_AA_TIP_DO_HIDE;
        }
        break;
    default: /* MUTED */
        if (ev == MUI_AA_TIP_EV_TIMER) {
            act = MUI_AA_TIP_DO_DISARM;
        } else if (ev == MUI_AA_TIP_EV_LEAVE || ev == MUI_AA_TIP_EV_INACTIVE || ev == MUI_AA_TIP_EV_GONE) {
            next = MUI_AA_TIP_IDLE;
        }
        break;
    }
    if (actions != NULL) {
        *actions |= act;
    }
    return next;
}
