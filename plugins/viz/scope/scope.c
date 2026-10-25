/* Oscilloscope viz plugin (plan DUI-P4-11): the post-mix ring as a stereo trace, left in primary, right in secondary,
 * centre line in grid. Pixel values come from sel_viz_colors; integer only. */
#include "viz/surface.h"

static uint32_t render(const sel_viz_frame* f, sel_surface* s, const sel_viz_colors* c) {
    uint32_t x, prims = 0;
    int32_t mid = (int32_t)s->h / 2, half = (int32_t)s->h / 2 - 1;
    int32_t py[2];
    py[0] = mid;
    py[1] = mid;
    vs_clear(s, c->background);
    vs_line(s, 0, mid, (int32_t)s->w - 1, mid, c->grid);
    if (f == NULL || f->seq == 0u || s->w < 2u || s->h < 4u) {
        return 0;
    }
    for (x = 0; x < s->w; x++) {
        uint32_t i = x * (SEL_VIZ_PCM_FRAMES - 1u) / (s->w - 1u);
        uint32_t ch;
        for (ch = 0; ch < 2u; ch++) {
            int32_t y = mid - (int32_t)f->pcm[i * 2u + ch] * half / 32768;
            if (x > 0u) {
                vs_line(s, (int32_t)x - 1, py[ch], (int32_t)x, y, ch == 0u ? c->primary : c->secondary);
                prims++;
            }
            py[ch] = y;
        }
    }
    return prims;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    return vs_descriptor("Oscilloscope", "viz_scope", render);
}
