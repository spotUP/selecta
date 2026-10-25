/* Levels viz plugin (plan DUI-P4-11): one bar per sounding voice (a render replayer has no voices: one bar per stereo
 * side, from the ring peak). Height = VU level, peak tick in accent. */
#include "viz/surface.h"

static int32_t ring_peak(const sel_viz_frame* f, uint32_t side) {
    uint32_t i;
    int32_t m = 0;
    for (i = 0; i < SEL_VIZ_PCM_FRAMES; i++) {
        int32_t a = f->pcm[i * 2u + side];
        a = a < 0 ? -a : a;
        m = a > m ? a : m;
    }
    return m > 32767 ? 32767 : m;
}

static uint32_t render(const sel_viz_frame* f, sel_surface* s, const sel_viz_colors* c) {
    uint32_t v, n, prims = 0;
    int32_t bw, gap = 2;
    vs_clear(s, c->background);
    if (f == NULL || f->seq == 0u || s->w < 8u || s->h < 4u) {
        return 0;
    }
    n = f->voice_count > 0u ? f->voice_count : 2u;
    if (n > SEL_VIZ_MAX_VOICES) {
        n = SEL_VIZ_MAX_VOICES;
    }
    bw = ((int32_t)s->w - gap * (int32_t)(n - 1u)) / (int32_t)n;
    if (bw < 1) {
        bw = 1;
        gap = 0;
    }
    for (v = 0; v < n; v++) {
        int32_t x = (int32_t)v * (bw + gap);
        int32_t level = f->voice_count > 0u ? f->vu[v] : ring_peak(f, v);
        int32_t peak = f->voice_count > 0u ? f->peak[v] : level;
        int32_t hgt = level * ((int32_t)s->h - 2) / 32768;
        int32_t ph = peak * ((int32_t)s->h - 2) / 32768;
        vs_rect(s, x, 0, bw, (int32_t)s->h, c->grid);
        vs_rect(s, x, (int32_t)s->h - hgt, bw, hgt, c->primary);
        vs_rect(s, x, (int32_t)s->h - ph - 1, bw, 1, c->accent);
        prims += 3u;
    }
    return prims;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    return vs_descriptor("Levels", "viz_levels", render);
}
