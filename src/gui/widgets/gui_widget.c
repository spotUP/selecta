/* Widget drawing of the desktop UI (DUI-P3-1). Integer only. Every colour, radius and gap comes from the style. */
#include <selecta/gui/gui_widget.h>

#include <stddef.h>

typedef struct ctx {
    sel_gui_canvas* c;
    const sel_gui_style* s;
    sel_gui_widget_stats* st;
} ctx;

static const char* const k_kind_names[] = {
#define X(id, name) name,
    SEL_GUI_WIDGETS(X)
#undef X
};

const char* sel_gui_widget_kind_name(uint32_t kind) {
    return kind < SEL_GUI_WIDGET_KIND_COUNT ? k_kind_names[kind] : "unknown";
}

int32_t sel_gui_icon_draw(sel_gui_canvas* c, const sel_gui_icons* ic, uint32_t id, sel_gui_rect box, int32_t px,
                          uint32_t argb) {
    uint32_t i, pick = 0;
    int32_t n;
    if (ic == NULL || id >= ic->count || ic->size_n == 0) {
        return 0;
    }
    for (i = 0; i < ic->size_n; i++) {
        if (ic->sizes[i] <= px) {
            pick = i;
        }
    }
    n = ic->sizes[pick];
    sel_gui_blit_mask(c, box.x + (box.w - n) / 2, box.y + (box.h - n) / 2,
                      ic->alpha[pick] + (size_t)id * (size_t)n * (size_t)n, n, n, n, argb);
    return n;
}

static sel_gui_rect rc(int32_t x, int32_t y, int32_t w, int32_t h) {
    sel_gui_rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

static sel_gui_rect grow(sel_gui_rect r, int32_t d) {
    return rc(r.x - d, r.y - d, r.w + 2 * d, r.h + 2 * d);
}

/* Linear mix of two 0xAARRGGBB colours, t in 0..256 (0 = a). */
static uint32_t mix(uint32_t a, uint32_t b, int32_t t) {
    uint32_t out = 0xFF000000u;
    int sh;
    for (sh = 0; sh < 24; sh += 8) {
        int32_t ca = (int32_t)((a >> sh) & 0xFFu), cb = (int32_t)((b >> sh) & 0xFFu);
        out |= (uint32_t)(ca + (cb - ca) * t / 256) << sh;
    }
    return out;
}

static int32_t text_w(const sel_gui_atlas* at, const char* s) {
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return sel_gui_text_width(at, s, n);
}

/* One line of text with its line box centred vertically in `r`, pen at px. */
static void text_at(const ctx* x, const sel_gui_atlas* at, sel_gui_rect r, int32_t px, const char* s, uint32_t col,
                    int32_t max_w) {
    sel_gui_text_draw(x->c, at, px, r.y + (r.h - at->line_height) / 2, s, col, max_w, &x->st->text);
}

static void text_centered(const ctx* x, const sel_gui_atlas* at, sel_gui_rect r, const char* s, uint32_t col) {
    int32_t w = text_w(at, s);
    if (w > r.w) {
        w = r.w;
    }
    text_at(x, at, r, r.x + (r.w - w) / 2, s, col, r.w);
}

static uint32_t fg_of(const ctx* x, const sel_gui_widget* w) {
    return (w->state & SEL_GUI_WS_DISABLED) ? x->s->disabled : x->s->text;
}

static uint32_t control_bg(const ctx* x, const sel_gui_widget* w) {
    if (w->state & SEL_GUI_WS_DISABLED) {
        return x->s->surface;
    }
    if (w->state & SEL_GUI_WS_PRESSED) {
        return x->s->pressed;
    }
    if (w->state & SEL_GUI_WS_HOVER) {
        return x->s->hover;
    }
    return x->s->raised;
}

static void focus_ring(const ctx* x, const sel_gui_widget* w, int32_t radius) {
    if (w->state & SEL_GUI_WS_FOCUS) {
        sel_gui_round_rect(x->c, grow(w->r, 2), radius + 2, x->s->focus);
    }
}

static void icon_in(const ctx* x, int32_t icon, sel_gui_rect box, int32_t px, uint32_t col) {
    if (icon >= 0) {
        sel_gui_icon_draw(x->c, x->s->icons, (uint32_t)icon, box, px, col);
    }
}

static void card(const ctx* x, sel_gui_rect r, int32_t radius, uint32_t col, int shadow) {
    if (shadow && x->s->shadow != NULL) {
        sel_gui_shadow_draw(x->c, x->s->shadow, r);
    }
    sel_gui_round_rect(x->c, r, radius, col);
}

static const char* item_of(const sel_gui_widget* w, uint32_t i) {
    return (w->items != NULL && i < w->item_n) ? w->items[i] : "";
}

static int32_t q16(int32_t total, int32_t value) {
    int64_t v = ((int64_t)total * value) >> 16;
    return v < 0 ? 0 : v > total ? total : (int32_t)v;
}

static int32_t isqrt(int32_t v) {
    int32_t r = 0;
    while ((r + 1) * (r + 1) <= v) {
        r++;
    }
    return r;
}

static void draw_button(const ctx* x, const sel_gui_widget* w, sel_gui_rect r, const char* label, int primary) {
    const sel_gui_style* s = x->s;
    uint32_t bg = primary ? s->accent : control_bg(x, w);
    if (primary && (w->state & SEL_GUI_WS_PRESSED)) {
        bg = mix(s->accent, s->background, 48);
    } else if (primary && (w->state & SEL_GUI_WS_HOVER)) {
        bg = mix(s->accent, s->accent_text, 36);
    }
    sel_gui_round_rect(x->c, r, s->radius_large, bg);
    text_centered(x, s->semibold, r, label, primary ? s->accent_text : fg_of(x, w));
}

static void d_label(const ctx* x, const sel_gui_widget* w) {
    text_at(x, x->s->body, w->r, w->r.x, w->label, fg_of(x, w), w->r.w);
}

static void d_icon(const ctx* x, const sel_gui_widget* w) {
    int32_t px = w->r.w < w->r.h ? w->r.w : w->r.h;
    icon_in(x, w->icon, w->r, px, fg_of(x, w));
}

static void d_icon_button(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int sel = (w->state & SEL_GUI_WS_SELECTED) != 0;
    focus_ring(x, w, w->r.h / 2);
    sel_gui_round_rect(x->c, w->r, w->r.h / 2, sel ? s->accent : control_bg(x, w));
    icon_in(x, w->icon, w->r, 20, sel ? s->accent_text : fg_of(x, w));
}

static void d_text_button(const ctx* x, const sel_gui_widget* w) {
    focus_ring(x, w, x->s->radius_large);
    draw_button(x, w, w->r, w->label, (w->state & SEL_GUI_WS_SELECTED) != 0);
}

static void d_toggle(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int on = (w->state & SEL_GUI_WS_SELECTED) != 0;
    int32_t k = w->r.h - 6;
    focus_ring(x, w, w->r.h / 2);
    sel_gui_round_rect(x->c, w->r, w->r.h / 2, on ? s->accent : s->pressed);
    sel_gui_round_rect(x->c, rc(on ? w->r.x + w->r.w - k - 3 : w->r.x + 3, w->r.y + 3, k, k), k / 2,
                       on ? s->accent_text : s->muted);
}

static void d_slider(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t ty = w->r.y + w->r.h / 2 - 2, fw = q16(w->r.w, w->value);
    int32_t k = (w->state & (SEL_GUI_WS_HOVER | SEL_GUI_WS_PRESSED)) ? 14 : 12;
    sel_gui_round_rect(x->c, rc(w->r.x, ty, w->r.w, 4), 2, s->pressed);
    if (fw > 0) {
        sel_gui_round_rect(x->c, rc(w->r.x, ty, fw, 4), 2, (w->state & SEL_GUI_WS_DISABLED) ? s->disabled : s->accent);
    }
    if (w->state & SEL_GUI_WS_FOCUS) {
        sel_gui_round_rect(x->c, rc(w->r.x + fw - k / 2 - 2, w->r.y + w->r.h / 2 - k / 2 - 2, k + 4, k + 4), k / 2 + 2,
                           s->focus);
    }
    sel_gui_round_rect(x->c, rc(w->r.x + fw - k / 2, w->r.y + w->r.h / 2 - k / 2, k, k), k / 2, fg_of(x, w));
}

static void d_progress(const ctx* x, const sel_gui_widget* w) {
    int32_t fw = q16(w->r.w, w->value);
    sel_gui_round_rect(x->c, w->r, w->r.h / 2, x->s->pressed);
    if (fw > 0) {
        sel_gui_round_rect(x->c, rc(w->r.x, w->r.y, fw < w->r.h ? w->r.h : fw, w->r.h), w->r.h / 2, x->s->accent);
    }
}

static void d_virtual_list(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t rh = s->body->line_height + 2 * s->spacing, y = w->r.y;
    uint32_t i;
    sel_gui_clip_push(x->c, w->r);
    for (i = (uint32_t)w->top; i < w->item_n && y < w->r.y + w->r.h; i++, y += rh) {
        sel_gui_rect row = rc(w->r.x, y, w->r.w, rh);
        int sel = (int32_t)i == w->sel;
        if (sel) {
            sel_gui_round_rect(x->c, row, s->radius, s->hover);
        }
        text_at(x, s->body, row, row.x + s->spacing * 2, w->items[i], sel ? s->text : s->muted,
                row.w - s->spacing * 4);
    }
    sel_gui_clip_pop(x->c);
}

static void d_table_header(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    uint32_t i;
    int32_t cw = w->item_n ? w->r.w / (int32_t)w->item_n : w->r.w;
    for (i = 0; i < w->item_n; i++) {
        sel_gui_rect col = rc(w->r.x + (int32_t)i * cw, w->r.y, cw, w->r.h);
        int cur = (int32_t)i == w->sel;
        int32_t tw = text_w(s->caption, w->items[i]);
        text_at(x, s->caption, col, col.x + s->spacing, w->items[i], cur ? s->text : s->muted, cw - 2 * s->spacing);
        if (cur) {
            icon_in(x, SEL_GUI_ICON_CHEVRON_DOWN, rc(col.x + s->spacing + tw + 2, col.y, 16, col.h), 16, s->text);
        }
    }
    sel_gui_fill_rect(x->c, rc(w->r.x, w->r.y + w->r.h - 1, w->r.w, 1), s->divider);
}

/* Placeholder artwork: a rounded tile with a vertical gradient between two tones of the accent (the real artwork
 * comes from the artwork cache, DUI-P2-4), and an icon in the middle. */
static void d_artwork(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t rad = w->r.h < w->r.w ? w->r.h : w->r.w, i, n = w->r.h;
    uint32_t a = mix(s->accent, s->raised, 40 + (w->top * 53) % 160), b = mix(s->raised, s->background, 90);
    rad = s->radius_large < rad / 2 ? s->radius_large : rad / 2;
    for (i = 0; i < n; i++) {
        int32_t edge = i < rad ? i : (i >= n - rad ? n - 1 - i : rad), inset = 0, dy2;
        if (edge < rad) {
            dy2 = 2 * (rad - edge) - 1;
            inset = rad - (isqrt(4 * rad * rad - dy2 * dy2) + 1) / 2;
        }
        sel_gui_fill_rect(x->c, rc(w->r.x + inset, w->r.y + i, w->r.w - 2 * inset, 1),
                          mix(a, b, i * 256 / (n > 1 ? n - 1 : 1)));
    }
    icon_in(x, w->icon >= 0 ? w->icon : SEL_GUI_ICON_MUSIC, w->r, w->r.h / 3, s->text);
}

static void art_at(const ctx* x, sel_gui_rect r, int32_t tone, int32_t icon) {
    sel_gui_widget a;
    a.kind = SEL_GUI_WIDGET_ARTWORK;
    a.r = r;
    a.state = 0;
    a.label = "";
    a.icon = icon;
    a.value = 0;
    a.extent = 0;
    a.items = NULL;
    a.item_n = 0;
    a.sel = 0;
    a.top = tone;
    d_artwork(x, &a);
}

static void d_tile_grid(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t gap = s->spacing * 2, n = (int32_t)w->item_n, tw = (w->r.w - gap * (n - 1)) / (n ? n : 1), i;
    int32_t pad = s->spacing, art = tw - 2 * pad;
    for (i = 0; i < n; i++) {
        sel_gui_rect t = rc(w->r.x + i * (tw + gap), w->r.y, tw, w->r.h);
        int32_t ty = t.y + pad + art + pad;
        card(x, t, s->radius_large, i == w->sel ? s->hover : s->raised, 0);
        art_at(x, rc(t.x + pad, t.y + pad, art, art), i + 1, SEL_GUI_ICON_MUSIC);
        text_at(x, s->semibold, rc(t.x + pad, ty, art, s->semibold->line_height), t.x + pad, w->items[i], s->text, art);
        text_at(x, s->caption, rc(t.x + pad, ty + s->semibold->line_height + 2, art, s->caption->line_height), t.x + pad,
                w->label, s->muted, art);
    }
}

static void d_cover_flow(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t cx = w->r.x + w->r.w / 2, big = w->r.h - 2 * s->spacing, step = big * 2 / 3, k, side;
    for (k = 2; k >= 1; k--) {
        for (side = -1; side <= 1; side += 2) {
            int32_t sz = big - k * big / 5;
            sel_gui_rect r = rc(cx + side * (k == 1 ? step : step * 3 / 2) - sz / 2, w->r.y + (w->r.h - sz) / 2, sz, sz);
            art_at(x, r, w->sel + side * k + 7, SEL_GUI_ICON_DISC);
            sel_gui_scrim(x->c, r, s->scrim & 0xFFFFFFu, k == 2 ? 150u : 90u);
        }
    }
    card(x, rc(cx - big / 2, w->r.y + s->spacing, big, big), s->radius_large, s->raised, 1);
    art_at(x, rc(cx - big / 2, w->r.y + s->spacing, big, big), w->sel + 7, SEL_GUI_ICON_DISC);
}

static void d_tabs(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t px = w->r.x, gap = s->spacing * 3;
    uint32_t i;
    sel_gui_fill_rect(x->c, rc(w->r.x, w->r.y + w->r.h - 1, w->r.w, 1), s->divider);
    for (i = 0; i < w->item_n; i++) {
        int cur = (int32_t)i == w->sel;
        int32_t tw = text_w(s->semibold, w->items[i]);
        text_at(x, s->semibold, rc(px, w->r.y, tw, w->r.h - 3), px, w->items[i], cur ? s->text : s->muted, tw);
        if (cur) {
            sel_gui_fill_rect(x->c, rc(px, w->r.y + w->r.h - 3, tw, 3), s->accent);
        }
        px += tw + gap;
    }
}

static void d_sidebar_list(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t rh = 36, y = w->r.y;
    uint32_t i;
    for (i = 0; i < w->item_n; i++, y += rh) {
        sel_gui_rect row = rc(w->r.x, y, w->r.w, rh);
        int cur = (int32_t)i == w->sel;
        uint32_t col = cur ? s->text : s->muted;
        if (cur) {
            sel_gui_round_rect(x->c, row, s->radius, s->hover);
        }
        icon_in(x, w->icon + (int32_t)i, rc(row.x + s->spacing, row.y, 24, rh), 24, col);
        text_at(x, cur ? s->semibold : s->body, row, row.x + s->spacing * 3 + 24, w->items[i], col,
                row.w - 24 - s->spacing * 4);
    }
}

static void d_split_pane(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t at = q16(w->r.w, w->value), hx = w->r.x + at - 2, i;
    int32_t pad = s->spacing * 2, rw = w->r.w - at - 2;
    sel_gui_round_rect(x->c, rc(w->r.x, w->r.y, at - 2, w->r.h), s->radius_large, s->surface);
    sel_gui_round_rect(x->c, rc(hx + 4, w->r.y, rw, w->r.h), s->radius_large, s->raised);
    text_at(x, s->semibold, rc(w->r.x + pad, w->r.y + s->spacing, at - 2 * pad, 24), w->r.x + pad, item_of(w, 0), s->text,
            at - 2 * pad);
    text_at(x, s->semibold, rc(hx + 4 + pad, w->r.y + s->spacing, rw - 2 * pad, 24), hx + 4 + pad, item_of(w, 1), s->text,
            rw - 2 * pad);
    for (i = 0; i < 3; i++) {
        sel_gui_fill_rect(x->c, rc(hx + 1, w->r.y + w->r.h / 2 - 10 + i * 8, 2, 4),
                          (w->state & (SEL_GUI_WS_HOVER | SEL_GUI_WS_PRESSED)) ? s->accent : s->muted);
    }
}

static void d_scroll_bar(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t th = q16(w->r.h, w->extent), off;
    if (th < w->r.w) {
        th = w->r.w;
    }
    off = q16(w->r.h - th, w->value);
    sel_gui_round_rect(x->c, w->r, w->r.w / 2, s->surface);
    sel_gui_round_rect(x->c, rc(w->r.x, w->r.y + off, w->r.w, th), w->r.w / 2,
                       (w->state & (SEL_GUI_WS_HOVER | SEL_GUI_WS_PRESSED)) ? s->muted : s->disabled);
}

static void d_text_field(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t tx = w->r.x + s->spacing * 2, tw;
    focus_ring(x, w, s->radius);
    sel_gui_round_rect(x->c, w->r, s->radius, (w->state & SEL_GUI_WS_HOVER) ? s->hover : s->pressed);
    if (w->icon >= 0) {
        icon_in(x, w->icon, rc(tx, w->r.y, 20, w->r.h), 20, s->muted);
        tx += 20 + s->spacing;
    }
    tw = text_w(s->body, w->label);
    text_at(x, s->body, w->r, tx, w->label, fg_of(x, w), w->r.w - (tx - w->r.x) - s->spacing * 2);
    if (w->state & SEL_GUI_WS_FOCUS) {
        sel_gui_fill_rect(x->c, rc(tx + tw + 1, w->r.y + (w->r.h - s->body->line_height) / 2, 1, s->body->line_height),
                          s->accent);
    }
}

static void d_menu(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t rh = s->body->line_height + 2 * s->spacing, y = w->r.y + s->spacing;
    uint32_t i;
    card(x, w->r, s->radius_large, s->raised, 1);
    for (i = 0; i < w->item_n; i++, y += rh) {
        sel_gui_rect row = rc(w->r.x + s->spacing, y, w->r.w - 2 * s->spacing, rh);
        if (w->items[i][0] == '-') {
            sel_gui_fill_rect(x->c, rc(row.x, y + rh / 2, row.w, 1), s->divider);
            continue;
        }
        if ((int32_t)i == w->sel) {
            sel_gui_round_rect(x->c, row, s->radius_small, s->hover);
        }
        text_at(x, s->body, row, row.x + s->spacing * 2, w->items[i], s->text, row.w - 4 * s->spacing);
    }
}

static void d_tooltip(const ctx* x, const sel_gui_widget* w) {
    card(x, w->r, x->s->radius_small, x->s->pressed, 1);
    text_centered(x, x->s->caption, w->r, w->label, x->s->text);
}

static void d_toast(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    sel_gui_rect dot = rc(w->r.x + s->spacing * 2, w->r.y + (w->r.h - 28) / 2, 28, 28);
    card(x, w->r, s->radius_large, s->raised, 1);
    sel_gui_round_rect(x->c, dot, 14, s->success);
    icon_in(x, SEL_GUI_ICON_CHECK, dot, 16, s->background);
    text_at(x, s->body, w->r, dot.x + 28 + s->spacing * 2, w->label, s->text, w->r.w - 28 - s->spacing * 6);
}

static void d_modal(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t pad = s->spacing * 3, bw = 96, bh = 36, by = w->r.y + w->r.h - pad - bh;
    sel_gui_widget b = *w;
    card(x, w->r, s->radius_large, s->raised, 1);
    text_at(x, s->semibold, rc(w->r.x + pad, w->r.y + pad, w->r.w - 2 * pad, 24), w->r.x + pad, w->label, s->text,
            w->r.w - 2 * pad);
    sel_gui_text_wrap(x->c, s->body, rc(w->r.x + pad, w->r.y + pad + 32, w->r.w - 2 * pad, by - w->r.y - pad - 32 - s->spacing),
                      item_of(w, 0), s->muted, 3, &x->st->text);
    b.state = 0;
    draw_button(x, &b, rc(w->r.x + w->r.w - pad - 2 * bw - s->spacing * 2, by, bw, bh), item_of(w, 1), 0);
    b.state = SEL_GUI_WS_SELECTED;
    draw_button(x, &b, rc(w->r.x + w->r.w - pad - bw, by, bw, bh), item_of(w, 2), 1);
}

static void d_scrim(const ctx* x, const sel_gui_widget* w) {
    sel_gui_scrim(x->c, w->r, x->s->scrim & 0xFFFFFFu, (uint32_t)q16(255, w->value));
}

static void d_viz(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t n = 32, gap = 3, bw = (w->r.w - 2 * s->spacing - gap * (n - 1)) / n, avail = w->r.h - 2 * s->spacing, i;
    uint32_t seed = (uint32_t)(w->value >> 4) + 12345u;
    sel_gui_round_rect(x->c, w->r, s->radius_large, s->background);
    for (i = 0; i < n; i++) {
        int32_t bh;
        seed = seed * 1103515245u + 12345u;
        bh = avail * (20 + (int32_t)((seed >> 16) % 80) * (2 * n - i) / (2 * n)) / 100;
        if (bh > avail) {
            bh = avail;
        }
        sel_gui_fill_rect(x->c, rc(w->r.x + s->spacing + i * (bw + gap), w->r.y + w->r.h - s->spacing - bh, bw, bh),
                          mix(s->accent, s->accent_text, (i * 64) / n));
    }
}

static void d_segmented(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t n = (int32_t)w->item_n, sw = n ? (w->r.w - 4) / n : w->r.w, i;
    sel_gui_round_rect(x->c, w->r, s->radius, s->pressed);
    for (i = 0; i < n; i++) {
        sel_gui_rect seg = rc(w->r.x + 2 + i * sw, w->r.y + 2, sw, w->r.h - 4);
        if (i == w->sel) {
            sel_gui_round_rect(x->c, seg, s->radius_small, s->accent);
        }
        text_centered(x, s->semibold, seg, w->items[i], i == w->sel ? s->accent_text : s->muted);
    }
}

static void d_checkbox(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t k = 18;
    sel_gui_rect box = rc(w->r.x, w->r.y + (w->r.h - k) / 2, k, k);
    focus_ring(x, w, s->radius_small);
    if (w->state & SEL_GUI_WS_SELECTED) {
        sel_gui_round_rect(x->c, box, s->radius_small, s->accent);
        icon_in(x, SEL_GUI_ICON_CHECK, box, 16, s->accent_text);
    } else {
        sel_gui_round_rect(x->c, box, s->radius_small, (w->state & SEL_GUI_WS_DISABLED) ? s->disabled : s->muted);
        sel_gui_round_rect(x->c, grow(box, -2), s->radius_small > 2 ? s->radius_small - 2 : 0, s->surface);
    }
    text_at(x, s->body, w->r, w->r.x + k + s->spacing * 2, w->label, fg_of(x, w), w->r.w - k - s->spacing * 2);
}

static void d_radio(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    int32_t k = 18;
    int on = (w->state & SEL_GUI_WS_SELECTED) != 0;
    sel_gui_rect box = rc(w->r.x, w->r.y + (w->r.h - k) / 2, k, k);
    focus_ring(x, w, k / 2);
    sel_gui_round_rect(x->c, box, k / 2, on ? s->accent : ((w->state & SEL_GUI_WS_DISABLED) ? s->disabled : s->muted));
    sel_gui_round_rect(x->c, grow(box, -2), k / 2 - 2, s->surface);
    if (on) {
        sel_gui_round_rect(x->c, grow(box, -5), k / 2 - 5, s->accent);
    }
    text_at(x, s->body, w->r, w->r.x + k + s->spacing * 2, w->label, fg_of(x, w), w->r.w - k - s->spacing * 2);
}

static void d_dropdown(const ctx* x, const sel_gui_widget* w) {
    const sel_gui_style* s = x->s;
    focus_ring(x, w, s->radius);
    sel_gui_round_rect(x->c, w->r, s->radius, control_bg(x, w));
    text_at(x, s->body, w->r, w->r.x + s->spacing * 2, w->label, fg_of(x, w), w->r.w - 24 - s->spacing * 3);
    icon_in(x, SEL_GUI_ICON_CHEVRON_DOWN, rc(w->r.x + w->r.w - 24 - s->spacing, w->r.y, 24, w->r.h), 16, s->muted);
}

int sel_gui_widget_draw(sel_gui_canvas* c, const sel_gui_style* s, const sel_gui_widget* w,
                        sel_gui_widget_stats* st) {
    ctx x;
    x.c = c;
    x.s = s;
    x.st = st;
    switch (w->kind) {
#define CASE(id, fn)          \
    case SEL_GUI_WIDGET_##id: \
        fn(&x, w);            \
        break;
        CASE(LABEL, d_label)
        CASE(ICON, d_icon)
        CASE(ICON_BUTTON, d_icon_button)
        CASE(TEXT_BUTTON, d_text_button)
        CASE(TOGGLE, d_toggle)
        CASE(SLIDER, d_slider)
        CASE(PROGRESS, d_progress)
        CASE(VIRTUAL_LIST, d_virtual_list)
        CASE(TABLE_HEADER, d_table_header)
        CASE(TILE_GRID, d_tile_grid)
        CASE(COVER_FLOW, d_cover_flow)
        CASE(TABS, d_tabs)
        CASE(SIDEBAR_LIST, d_sidebar_list)
        CASE(SPLIT_PANE, d_split_pane)
        CASE(SCROLL_BAR, d_scroll_bar)
        CASE(TEXT_FIELD, d_text_field)
        CASE(MENU, d_menu)
        CASE(TOOLTIP, d_tooltip)
        CASE(TOAST, d_toast)
        CASE(MODAL_DIALOG, d_modal)
        CASE(SCRIM, d_scrim)
        CASE(ARTWORK, d_artwork)
        CASE(VIZ_SURFACE, d_viz)
        CASE(SEGMENTED, d_segmented)
        CASE(CHECKBOX, d_checkbox)
        CASE(RADIO, d_radio)
        CASE(DROPDOWN, d_dropdown)
#undef CASE
    default:
        return -1;
    }
    st->draws[w->kind]++;
    return 0;
}
