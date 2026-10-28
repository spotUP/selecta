#include "mui_aa.h"

static int isqrt(int v) {
    int r = 0;
    while ((r + 1) * (r + 1) <= v) {
        r++;
    }
    return r;
}

int mui_aa_round_inset(int w, int h, int r, int y) {
    int dy2, half;
    if (w <= 0 || h <= 0 || y < 0 || y >= h) {
        return 0;
    }
    if (r > h / 2) {
        r = h / 2;
    }
    if (r > w / 2) {
        r = w / 2;
    }
    if (r <= 0) {
        return 0;
    }
    if (y >= h - r) {
        y = h - 1 - y; /* mirror the bottom rows onto the top */
    }
    if (y >= r) {
        return 0;
    }
    /* Row centre at distance (r - y - 0.5) above the circle centre, doubled to stay integer: d = 2r - 2y - 1. */
    dy2 = 2 * r - 2 * y - 1;
    half = isqrt(4 * r * r - dy2 * dy2); /* twice the half chord */
    return r - (half + 1) / 2;
}

int mui_aa_ramp_index(int coverage, int steps) {
    if (steps < 2) {
        return 0;
    }
    if (coverage < 0) {
        coverage = 0;
    }
    if (coverage > 255) {
        coverage = 255;
    }
    return (coverage * (steps - 1) + 127) / 255;
}

uint32_t mui_aa_ramp_rgb(uint32_t fg, uint32_t bg, int steps, int i) {
    uint32_t out = 0;
    int sh;
    if (steps < 2) {
        return bg;
    }
    if (i < 0) {
        i = 0;
    }
    if (i > steps - 1) {
        i = steps - 1;
    }
    for (sh = 16; sh >= 0; sh -= 8) {
        int f = (int)((fg >> sh) & 0xFFu);
        int b = (int)((bg >> sh) & 0xFFu);
        out |= (uint32_t)(b + (f - b) * i / (steps - 1)) << sh;
    }
    return out;
}

uint32_t mui_aa_pressed_rgb(uint32_t face) {
    uint32_t out = 0;
    int sh;
    uint32_t lum = ((face >> 16) & 0xFFu) * 2126u / 10u + ((face >> 8) & 0xFFu) * 7152u / 10u + (face & 0xFFu) * 722u / 10u;
    int light = lum > 128000u; /* lum runs 0..255000 */
    for (sh = 16; sh >= 0; sh -= 8) {
        int c = (int)((face >> sh) & 0xFFu);
        c = light ? c * 4 / 5 : c + (255 - c) / 5;
        out |= (uint32_t)c << sh;
    }
    return out;
}

static uint32_t g_state_calls;

uint32_t mui_aa_state_calls(void) {
    return g_state_calls;
}

uint32_t mui_aa_state_rgb(const uint32_t* token, uint32_t face, int state) {
    g_state_calls++;
    switch (state) {
    case MUI_AA_HOVER:
        return token[SEL_GUI_TOKEN_SURFACE_HOVER];
    case MUI_AA_PRESSED:
    case MUI_AA_SELECTED:
        return token[SEL_GUI_TOKEN_SURFACE_PRESSED];
    default:
        return face;
    }
}

uint32_t mui_aa_text_rgb(const uint32_t* token, uint32_t text, int state) {
    g_state_calls++;
    return state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED] : text;
}

void mui_aa_button_rgb(const uint32_t* token, int role, int state, uint32_t* face, uint32_t* text) {
    uint32_t f, t;
    if (role == MUI_AA_ROLE_ACCENT) {
        f = token[SEL_GUI_TOKEN_ACCENT];
        t = token[SEL_GUI_TOKEN_ACCENT_TEXT];
        /* The accent face keeps its colour on hover and press (no accent hover token); the press shows as the pressed token. */
        f = state == MUI_AA_PRESSED || state == MUI_AA_SELECTED ? mui_aa_state_rgb(token, f, state) : f;
    } else if (role == MUI_AA_ROLE_GHOST) {
        /* Ghost sits on the panel (MUI_TH_PANEL = SURFACE in mui_theme.c); only a selected door is raised. */
        f = token[SEL_GUI_TOKEN_SURFACE];
        t = token[SEL_GUI_TOKEN_TEXT];
        f = state == MUI_AA_SELECTED ? token[SEL_GUI_TOKEN_SURFACE_RAISED] : mui_aa_state_rgb(token, f, state == MUI_AA_DISABLED ? MUI_AA_NORMAL : state);
    } else {
        f = mui_aa_state_rgb(token, token[SEL_GUI_TOKEN_SURFACE_RAISED], state);
        t = token[SEL_GUI_TOKEN_TEXT];
    }
    *face = f;
    *text = mui_aa_text_rgb(token, t, state);
}

int mui_aa_ramp_pens(int kind, const uint32_t* token, uint32_t fg, uint32_t bg, uint32_t* out) {
    int i;
    if (kind == MUI_AA_RAMP_AGA) {
        out[0] = bg;
        out[1] = token[SEL_GUI_TOKEN_TEXT_DISABLED];
        out[2] = token[SEL_GUI_TOKEN_TEXT_MUTED];
        out[3] = token[SEL_GUI_TOKEN_TEXT];
        return 4;
    }
    if (kind == MUI_AA_RAMP_RTG) {
        for (i = 0; i < MUI_AA_RAMP_MAX; i++) {
            out[i] = mui_aa_ramp_rgb(fg, bg, MUI_AA_RAMP_MAX, i);
        }
        return MUI_AA_RAMP_MAX;
    }
    return 0;
}

int mui_aa_ramp_slot(int kind, int coverage) {
    if (coverage < 0) {
        coverage = 0;
    }
    if (coverage > 255) {
        coverage = 255;
    }
    if (kind == MUI_AA_RAMP_AGA) {
        return coverage / 64;
    }
    return kind == MUI_AA_RAMP_RTG ? mui_aa_ramp_index(coverage, MUI_AA_RAMP_MAX) : 0;
}

int mui_aa_font_px(int screen_rows, int screen_cols) {
    (void)screen_cols;
    return screen_rows <= 256 ? 0 : screen_rows <= 600 ? 13 : screen_rows <= 800 ? 15 : 17;
}

int mui_aa_tier(int screen_rows) {
    return screen_rows <= 256 ? MUI_AA_TIER_S : screen_rows <= 600 ? MUI_AA_TIER_M : screen_rows <= 800 ? MUI_AA_TIER_L : MUI_AA_TIER_XL;
}

void mui_aa_metrics(int font_px, mui_aa_metrics_t* out) {
    int small = font_px <= 0;
    out->font_px = small ? 8 : font_px;
    out->pad_x = small ? 6 : 10;
    out->pad_y = small ? 3 : 6;
    out->icon_px = small ? 16 : 24;
    out->icon_gap = 6;
    out->height = small ? out->font_px + 2 * out->pad_y : (out->font_px > out->icon_px ? out->font_px : out->icon_px) + 2 * out->pad_y;
    out->radius = small ? 4 : 6;
}
