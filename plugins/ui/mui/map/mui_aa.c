#include "mui_aa.h"

#include <string.h>

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
    } else if (role == MUI_AA_ROLE_TOGGLE) {
        /* AACheck as the desktop d_checkbox: checked = ACCENT box with an ACCENT_TEXT check, unchecked = SURFACE box (hover shows the hover token). */
        f = state == MUI_AA_SELECTED ? token[SEL_GUI_TOKEN_ACCENT] : mui_aa_state_rgb(token, token[SEL_GUI_TOKEN_SURFACE], state);
        t = token[SEL_GUI_TOKEN_ACCENT_TEXT];
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
    out->check_px = small ? 12 : 16;
}

int mui_aa_check_plan(const uint32_t* token, const mui_aa_metrics_t* m, int state, mui_aa_check_op_t* out) {
    int r = (int)token[SEL_GUI_TOKEN_RADIUS_SMALL], line = m->check_px >= 16 ? 2 : 1, n = 0;
    uint32_t face, ink;
    if (m->check_px < 16) {
        r /= 2;
    }
    mui_aa_button_rgb(token, MUI_AA_ROLE_TOGGLE, state, &face, &ink);
    out[n].inset = 0, out[n].radius = 0, out[n++].rgb = token[SEL_GUI_TOKEN_SURFACE];
    if (state == MUI_AA_SELECTED) {
        out[n].inset = 0, out[n].radius = r, out[n++].rgb = face;
        return n;
    }
    out[n].inset = 0, out[n].radius = r, out[n++].rgb = state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED] : token[SEL_GUI_TOKEN_TEXT_MUTED];
    out[n].inset = line, out[n].radius = r > line ? r - line : 0, out[n++].rgb = face;
    return n;
}

int mui_aa_tone_token(int tone) {
    return tone == MUI_AA_TONE_MUTED ? SEL_GUI_TOKEN_TEXT_MUTED : SEL_GUI_TOKEN_TEXT;
}

int mui_aa_tone_font_px(int tone, int base_px) {
    return tone == MUI_AA_TONE_TITLE && base_px > 0 ? base_px + MUI_AA_TITLE_EXTRA_PX : base_px;
}

int mui_aa_align_x(int align, int w, int cw) {
    int x = align == MUI_AA_ALIGN_CENTRE ? (w - cw) / 2 : align == MUI_AA_ALIGN_RIGHT ? w - cw : 0;
    return x > 0 ? x : 0;
}

int mui_aa_btn_content_x(int align, int w, int cw, int pad_x) {
    return pad_x + mui_aa_align_x(align, w - 2 * pad_x, cw);
}

int mui_aa_text_fit(const char* s, int max_w, mui_aa_width_fn width, void* ctx, char* out, int cap) {
    int len = (int)strlen(s), n, dots_w;
    static const char dots[] = "...";
    if (cap <= 0) {
        return 0;
    }
    if (width(ctx, s, len) <= max_w) {
        n = len < cap - 1 ? len : cap - 1;
        memcpy(out, s, (size_t)n);
        out[n] = '\0';
        return n;
    }
    dots_w = width(ctx, dots, 3);
    out[0] = '\0';
    if (dots_w > max_w || cap < 4) {
        return 0;
    }
    for (n = len - 1; n > 0; n--) {
        if (n > cap - 4) {
            continue;
        }
        if (width(ctx, s, n) + dots_w <= max_w) {
            break;
        }
    }
    memcpy(out, s, (size_t)n);
    memcpy(out + n, dots, 4);
    return n + 3;
}

int mui_aa_panel_token(int role) {
    return role == MUI_AA_PANEL_SURFACE ? SEL_GUI_TOKEN_SURFACE : role == MUI_AA_PANEL_RAISED ? SEL_GUI_TOKEN_SURFACE_RAISED : SEL_GUI_TOKEN_PANEL;
}

uint32_t mui_aa_panel_rgb(const uint32_t* token, int role) {
    return token[mui_aa_panel_token(role)];
}

int mui_aa_panel_radius(const uint32_t* token, int font_px) {
    int r = (int)token[SEL_GUI_TOKEN_RADIUS_LARGE];
    return font_px <= 0 ? r / 2 : r;
}

int mui_aa_panel_spacing(const uint32_t* token) {
    return (int)token[SEL_GUI_TOKEN_SPACING];
}

static uint32_t g_slider_calls;

uint32_t mui_aa_slider_calls(void) {
    return g_slider_calls;
}

void mui_aa_slider_metrics(int font_px, mui_aa_slider_t* out) {
    g_slider_calls++;
    out->track_h = font_px <= 0 ? 2 : 4;
    out->radius = out->track_h / 2;
    out->min_w = 80;
    out->height = font_px <= 0 ? 12 : font_px < 15 ? 16 : font_px < 17 ? 18 : 20; /* hit height; only the track and fill are drawn */
}

void mui_aa_slider_rgb(const uint32_t* token, int state, uint32_t* track, uint32_t* fill) {
    g_slider_calls++;
    if (track != NULL) {
        *track = token[SEL_GUI_TOKEN_TRACK];
    }
    if (fill != NULL) {
        *fill = state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED] : token[SEL_GUI_TOKEN_ACCENT];
    }
}

int mui_aa_slider_fill_w(int w, long value, long lo, long hi) {
    long span = hi - lo;
    g_slider_calls++;
    if (span <= 0 || w <= 0 || value <= lo) {
        return 0;
    }
    return value >= hi ? w : (int)(((long)w * (value - lo) + span / 2) / span);
}

long mui_aa_slider_value_at(int w, int x, long lo, long hi) {
    long span = hi - lo, v;
    g_slider_calls++;
    if (w <= 0 || span <= 0 || x <= 0) {
        return lo;
    }
    if (x >= w) {
        return hi;
    }
    v = lo + ((long)x * span + w / 2) / w;
    return v > hi ? hi : v;
}

int mui_aa_slider_plan(const uint32_t* token, const mui_aa_slider_t* m, int state, int w, int h, long value, long lo, long hi, mui_aa_slider_op_t* out) {
    int n = 0, ty = (h - m->track_h) / 2;
    uint32_t track, fill;
    int fw = mui_aa_slider_fill_w(w, value, lo, hi);
    mui_aa_slider_rgb(token, state, &track, &fill);
    out[n].kind = MUI_AA_SLIDER_ERASE;
    out[n].x = 0;
    out[n].y = 0;
    out[n].w = w;
    out[n].h = h;
    out[n].radius = 0;
    out[n++].rgb = 0;
    out[n].kind = MUI_AA_SLIDER_TRACK;
    out[n].x = 0;
    out[n].y = ty;
    out[n].w = w;
    out[n].h = m->track_h;
    out[n].radius = m->radius;
    out[n++].rgb = track;
    if (fw > 0) {
        out[n].kind = MUI_AA_SLIDER_FILL;
        out[n].x = 0;
        out[n].y = ty;
        out[n].w = fw;
        out[n].h = m->track_h;
        out[n].radius = m->radius;
        out[n++].rgb = fill;
    }
    return n;
}

int mui_aa_progress_fill_w(int w, long value, long max) {
    g_slider_calls++;
    if (max <= 0 || w <= 0 || value <= 0) {
        return 0;
    }
    return value >= max ? w : (int)(((long)w * value + max / 2) / max);
}

static uint32_t g_cycle_calls;

uint32_t mui_aa_cycle_calls(void) {
    return g_cycle_calls;
}

void mui_aa_cycle_metrics(int font_px, mui_aa_cycle_t* out) {
    mui_aa_metrics_t b;
    g_cycle_calls++;
    mui_aa_metrics(font_px, &b);
    out->height = b.height;
    out->pad_x = b.pad_x;
    out->radius = b.radius;
    out->chev_w = font_px <= 0 ? 6 : 8;
    out->chev_h = out->chev_w / 2;
    out->gap = font_px <= 0 ? 6 : 8;
}

void mui_aa_cycle_rgb(const uint32_t* token, int state, uint32_t* face, uint32_t* text, uint32_t* chevron, uint32_t* popup_bg, uint32_t* row_hover) {
    g_cycle_calls++;
    if (face != NULL) {
        *face = mui_aa_state_rgb(token, token[SEL_GUI_TOKEN_SURFACE_RAISED], state);
    }
    if (text != NULL) {
        *text = mui_aa_text_rgb(token, token[SEL_GUI_TOKEN_TEXT], state);
    }
    if (chevron != NULL) {
        *chevron = state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED] : token[SEL_GUI_TOKEN_TEXT_MUTED];
    }
    if (popup_bg != NULL) {
        *popup_bg = token[SEL_GUI_TOKEN_SURFACE];
    }
    if (row_hover != NULL) {
        *row_hover = token[SEL_GUI_TOKEN_SURFACE_HOVER];
    }
}

int mui_aa_cycle_min_w(const mui_aa_cycle_t* m, int widest_text_w) {
    g_cycle_calls++;
    return 2 * m->pad_x + widest_text_w + m->gap + m->chev_w;
}

void mui_aa_cycle_chevron_row(const mui_aa_cycle_t* m, int r, int* x, int* w) {
    g_cycle_calls++;
    *x = r;
    *w = m->chev_w - 2 * r;
    if (*w < 1) {
        *w = 1;
    }
}

int mui_aa_cycle_step(int active, int n, int dir) {
    g_cycle_calls++;
    if (n <= 0) {
        return 0;
    }
    return ((active + dir) % n + n) % n;
}

int mui_aa_cycle_valid(int v, int n) {
    g_cycle_calls++;
    return v >= 0 && v < n;
}

int mui_aa_text_maxw(int fixed, int defw) {
    return fixed ? defw : MUI_AA_TEXT_UNBOUNDED;
}
