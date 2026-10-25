/* Spectrum viz plugin (plan DUI-P4-11): one bar per band of the pump-side spectrum (43 Hz to 16 kHz, log spaced). */
#include "viz/surface.h"

static uint32_t render(const sel_viz_frame* f, sel_surface* s, const sel_viz_colors* c) {
    uint32_t b, prims = 0;
    int32_t bw, gap = 2;
    vs_clear(s, c->background);
    if (f == NULL || f->seq == 0u || s->w < SEL_VIZ_BANDS || s->h < 4u) {
        return 0;
    }
    bw = ((int32_t)s->w - gap * (int32_t)(SEL_VIZ_BANDS - 1u)) / (int32_t)SEL_VIZ_BANDS;
    if (bw < 1) {
        bw = 1;
        gap = 0;
    }
    for (b = 0; b < SEL_VIZ_BANDS; b++) {
        int32_t x = (int32_t)b * (bw + gap);
        int32_t hgt = f->band[b] * ((int32_t)s->h - 2) / 32768;
        vs_rect(s, x, (int32_t)s->h - 1, bw, 1, c->grid);
        vs_rect(s, x, (int32_t)s->h - 1 - hgt, bw, hgt, c->primary);
        prims += 2u;
    }
    return prims;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    return vs_descriptor("Spectrum", "viz_spectrum", render);
}
