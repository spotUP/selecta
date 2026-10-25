/* Pixel helpers of the viz plugins (plan DUI-P4-11): clear, rectangle and line on a sel_surface in either pixel format,
 * plus the plugin descriptor boilerplate. Integer only; every call clips to the surface. Header-only so each viz plugin
 * stays one translation unit. */
#ifndef SELECTA_SHARED_VIZ_SURFACE_H
#define SELECTA_SHARED_VIZ_SURFACE_H

#include <selecta/plugin.h>
#include <selecta/viz.h>

static inline void vs_put(sel_surface* s, int32_t x, int32_t y, uint32_t px) {
    if (x < 0 || y < 0 || (uint32_t)x >= s->w || (uint32_t)y >= s->h) {
        return;
    }
    if (s->fmt == SEL_PIX_INDEX8) {
        ((uint8_t*)s->pixels)[(uint32_t)y * s->pitch + (uint32_t)x] = (uint8_t)px;
    } else {
        ((uint32_t*)(void*)((uint8_t*)s->pixels + (uint32_t)y * s->pitch))[x] = px;
    }
}

static inline void vs_rect(sel_surface* s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t px) {
    int32_t i, j;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            vs_put(s, x + i, y + j, px);
        }
    }
}

static inline void vs_clear(sel_surface* s, uint32_t px) {
    vs_rect(s, 0, 0, (int32_t)s->w, (int32_t)s->h, px);
}

/* Bresenham line. */
static inline void vs_line(sel_surface* s, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t px) {
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;
    for (;;) {
        int32_t e2;
        vs_put(s, x0, y0, px);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* The descriptor every viz plugin returns from its entry: name, render hook, id. */
static sel_tag vs_hooks[3];
static sel_tag vs_desc[5];

static inline const sel_tag* vs_descriptor(const char* name, const char* id, sel_viz_render_fn render) {
    vs_hooks[0].tag = SEL_VIZ_NAME;
    vs_hooks[0].value = (uintptr_t)name;
    vs_hooks[1].tag = SEL_VIZ_FRAME;
    vs_hooks[1].value = SEL_TAG_FN(render);
    vs_hooks[2].tag = SEL_TAG_END;
    vs_hooks[2].value = 0;
    vs_desc[0].tag = SEL_PLUGIN_KIND;
    vs_desc[0].value = SEL_PLUGIN_KIND_VIZ;
    vs_desc[1].tag = SEL_PLUGIN_ID;
    vs_desc[1].value = (uintptr_t)id;
    vs_desc[2].tag = SEL_PLUGIN_VERSION;
    vs_desc[2].value = 1;
    vs_desc[3].tag = SEL_PLUGIN_TAGS;
    vs_desc[3].value = (uintptr_t)vs_hooks;
    vs_desc[4].tag = SEL_TAG_END;
    vs_desc[4].value = 0;
    return vs_desc;
}

#endif /* SELECTA_SHARED_VIZ_SURFACE_H */
