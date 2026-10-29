/* Viz hosting and frame budget of the MUI UI (MUI-P5-3, MUI-P5-5); see mui_viz.h. */
#include "mui_viz.h"

#include "mui_map.h"

#include <stdlib.h>
#include <string.h>

void mui_viz_palette_from_tokens(mui_viz* v, const uint32_t* tokens) {
    v->palette[MUI_VIZ_BACKGROUND] = 0xFF000000u | tokens[SEL_GUI_TOKEN_SURFACE];
    v->palette[MUI_VIZ_GRID] = 0xFF000000u | tokens[SEL_GUI_TOKEN_SURFACE_RAISED];
    v->palette[MUI_VIZ_PRIMARY] = 0xFF000000u | tokens[SEL_GUI_TOKEN_ACCENT];
    v->palette[MUI_VIZ_SECONDARY] = 0xFF000000u | tokens[SEL_GUI_TOKEN_TEXT_MUTED];
    v->palette[MUI_VIZ_ACCENT] = 0xFF000000u | tokens[SEL_GUI_TOKEN_TEXT];
}

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

static const char* const k_viz_ids[MUI_VIZ_PLUGINS] = {"viz_scope", "viz_levels", "viz_spectrum"};

uint32_t mui_vizset_load(mui_vizset* s, mui_find_plugin_fn find, const char* pref) {
    uint32_t i;
    memset(s, 0, sizeof *s);
    for (i = 0; find != NULL && i < MUI_VIZ_PLUGINS; i++) {
        const sel_tag* desc = find(SEL_PLUGIN_KIND_VIZ, k_viz_ids[i]);
        mui_viz probe;
        const sel_tag* hooks;
        memset(&probe, 0, sizeof probe);
        if (desc == NULL || !mui_viz_bind(&probe, desc)) {
            continue;
        }
        hooks = (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0);
        s->id[s->n] = k_viz_ids[i];
        s->fn[s->n] = probe.fn;
        s->name_[s->n] = (const char*)sel_tag_find(hooks, SEL_VIZ_NAME, 0);
        if (s->name_[s->n] == NULL) {
            s->name_[s->n] = k_viz_ids[i];
        }
        s->n++;
    }
    s->name_[s->n] = NULL;
    if (pref != NULL) {
        (void)mui_vizset_select(s, pref);
    }
    return s->n;
}

int mui_vizset_select(mui_vizset* s, const char* id) {
    uint32_t i;
    for (i = 0; id != NULL && i < s->n; i++) {
        if (strcmp(s->id[i], id) == 0) {
            s->sel = i;
            return (int)i;
        }
    }
    return -1;
}

const char* mui_vizset_line(const mui_vizset* s) {
    return s->n > 0u ? mui_menu_line(s->id[s->sel]) : NULL;
}

int mui_vizset_tick(mui_vizset* s, const sel_viz_frame* f, const sel_uim_budget* b, uint32_t now_ms) {
    if (s->n == 0u || f == NULL || f->seq == 0u || f->seq == s->seq) {
        return 0;
    }
    if (!mui_gate_due(&s->gate, mui_budget_interval_ms(b, MUI_REDRAW_VIZ), now_ms)) {
        return 0;
    }
    s->seq = f->seq;
    s->allowed++;
    return 1;
}

int mui_vizset_step(mui_vizset* s, mui_viz* v, const sel_viz_frame* f, const sel_uim_budget* b, uint32_t now_ms, uint32_t w,
                    uint32_t h, uint32_t* dst) {
    if (w == 0u || h == 0u || dst == NULL || !mui_vizset_tick(s, f, b, now_ms)) {
        return 0;
    }
    v->fn = s->fn[s->sel];
    if (!mui_viz_fit(v, w, h)) {
        return 0;
    }
    (void)mui_viz_render(v, f);
    mui_viz_expand(v, dst);
    return 1;
}
