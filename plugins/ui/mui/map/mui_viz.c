/* Viz hosting and frame budget of the MUI UI (MUI-P5-3, MUI-P5-5); see mui_viz.h. */
#include "mui_viz.h"

#include <stdlib.h>
#include <string.h>

int mui_viz_bind(mui_viz* v, const sel_tag* desc) {
    const sel_tag* hooks = desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
    uintptr_t fn = hooks != NULL ? sel_tag_find(hooks, SEL_VIZ_FRAME, 0) : 0;
    v->fn = (sel_viz_render_fn)fn;
    return fn != 0;
}

int mui_viz_fit(mui_viz* v, uint32_t w, uint32_t h) {
    uint32_t pitch;
    if (w == 0u || h == 0u) {
        return 0;
    }
    if (v->pixels != NULL && v->surf.w == w && v->surf.h == h) {
        return 1;
    }
    pitch = (w + 3u) & ~3u;
    free(v->pixels);
    v->pixels = (uint8_t*)calloc((size_t)pitch * h, 1u);
    if (v->pixels == NULL) {
        memset(&v->surf, 0, sizeof v->surf);
        return 0;
    }
    v->surf.w = w;
    v->surf.h = h;
    v->surf.pitch = pitch;
    v->surf.fmt = SEL_PIX_INDEX8;
    v->surf.pixels = v->pixels;
    v->surf.palette = v->palette;
    return 1;
}

void mui_viz_release(mui_viz* v) {
    free(v->pixels);
    v->pixels = NULL;
    memset(&v->surf, 0, sizeof v->surf);
}

uint32_t mui_viz_render(mui_viz* v, const sel_viz_frame* f) {
    sel_viz_colors c;
    uint32_t prims;
    if (v->fn == NULL || v->pixels == NULL) {
        return 0u;
    }
    c.background = MUI_VIZ_BACKGROUND;
    c.grid = MUI_VIZ_GRID;
    c.primary = MUI_VIZ_PRIMARY;
    c.secondary = MUI_VIZ_SECONDARY;
    c.accent = MUI_VIZ_ACCENT;
    prims = v->fn(f, &v->surf, &c);
    if (prims != 0u) {
        v->frames++;
    }
    return prims;
}

void mui_viz_expand(const mui_viz* v, uint32_t* dst) {
    uint32_t x, y;
    for (y = 0; y < v->surf.h; y++) {
        const uint8_t* row = v->pixels + (size_t)y * v->surf.pitch;
        for (x = 0; x < v->surf.w; x++) {
            dst[(size_t)y * v->surf.w + x] = v->palette[row[x] < MUI_VIZ_COLORS ? row[x] : 0];
        }
    }
}

uint32_t mui_budget_interval_ms(const sel_uim_budget* b, mui_redraw_kind kind) {
    if (b->format == SEL_UIM_FMT_RGBA32) {
        return 40u;
    }
    return kind == MUI_REDRAW_VIZ ? 100u : 200u;
}

int mui_gate_due(mui_gate* g, uint32_t interval_ms, uint32_t now_ms) {
    if (g->started && (uint32_t)(now_ms - g->last_ms) < interval_ms) {
        return 0;
    }
    g->started = 1;
    g->last_ms = now_ms;
    return 1;
}
