/* Crossfeed DSP (design doc section 5, plan M1.3 I100): for headphones. Each
 * channel receives the other channel delayed by a few frames, passed through
 * a one-pole low-pass, at a lower level, so hard-panned tracker music stops
 * sounding like two separate ears. Integer Q15 only; the engine hands in the
 * coefficients through DSP_PARAM (the tables are generated at build time and
 * a plugin sees include/ only). Mono input passes unchanged: direct + bleed
 * is 1.0 and the low-pass has unity gain at DC. */
#include <selecta/dsp.h>

#define XF_RING 64u /* power of two above SEL_DSP_CROSSFEED_MAX_DELAY */

typedef struct crossfeed {
    const sel_host* host;
    int32_t delay;
    int32_t lowpass; /* Q15 */
    int32_t bleed;   /* Q15 */
    int32_t direct;  /* Q15 */
    uint32_t pos;
    int16_t ring[2][XF_RING];
    int32_t lp[2]; /* low-pass state, Q8 */
} crossfeed;

static void* xf_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    crossfeed* x;
    uint32_t i;
    (void)rate;
    if (channels != 2) {
        return NULL;
    }
    x = (crossfeed*)host->mem_alloc(host, sizeof *x, SEL_MEM_FAST);
    if (x == NULL) {
        return NULL;
    }
    x->host = host;
    x->delay = 0;
    x->lowpass = 32768;
    x->bleed = 0;
    x->direct = 32768;
    x->pos = 0;
    x->lp[0] = x->lp[1] = 0;
    for (i = 0; i < XF_RING; i++) {
        x->ring[0][i] = x->ring[1][i] = 0;
    }
    return x;
}

static void xf_close(void* state) {
    crossfeed* x = (crossfeed*)state;
    if (x) {
        x->host->mem_free(x->host, x);
    }
}

static void xf_param(void* state, uint32_t id, int32_t value) {
    crossfeed* x = (crossfeed*)state;
    switch (id) {
    case SEL_DSP_CROSSFEED_P_DELAY:
        x->delay = value < 0 ? 0 : (value > SEL_DSP_CROSSFEED_MAX_DELAY ? SEL_DSP_CROSSFEED_MAX_DELAY : value);
        break;
    case SEL_DSP_CROSSFEED_P_LOWPASS:
        x->lowpass = value;
        break;
    case SEL_DSP_CROSSFEED_P_BLEED:
        x->bleed = value;
        break;
    case SEL_DSP_CROSSFEED_P_DIRECT:
        x->direct = value;
        break;
    default:
        break;
    }
}

static int16_t xf_clamp(int32_t v) {
    return (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
}

static void xf_process(void* state, int16_t* stereo, uint32_t frames) {
    crossfeed* x = (crossfeed*)state;
    uint32_t n;
    for (n = 0; n < frames; n++) {
        int32_t in[2];
        int32_t out[2];
        uint32_t rd = (x->pos - (uint32_t)x->delay) & (XF_RING - 1u);
        uint32_t ch;
        in[0] = stereo[n * 2u];
        in[1] = stereo[n * 2u + 1u];
        x->ring[0][x->pos] = (int16_t)in[0];
        x->ring[1][x->pos] = (int16_t)in[1];
        for (ch = 0; ch < 2u; ch++) {
            /* the other ear's signal, delayed, through the one-pole low-pass */
            int32_t other = x->ring[1u - ch][rd];
            int32_t target = other * 256;
            int32_t diff = target - x->lp[ch];
            int32_t bleed;
            x->lp[ch] += (int32_t)(((int64_t)diff * x->lowpass) >> 15);
            bleed = (x->lp[ch] + 128) >> 8;
            out[ch] = (int32_t)(((int64_t)x->direct * in[ch] + (int64_t)x->bleed * bleed + 16384) >> 15);
        }
        stereo[n * 2u] = xf_clamp(out[0]);
        stereo[n * 2u + 1u] = xf_clamp(out[1]);
        x->pos = (x->pos + 1u) & (XF_RING - 1u);
    }
}

/* Filled at run time: a function pointer cast to an integer is not a
 * constant expression in C99. */
static sel_tag g_hooks[6];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_DSP_NAME, (uintptr_t) "Crossfeed");
    set(&g_hooks[1], SEL_DSP_OPEN, SEL_TAG_FN(xf_open));
    set(&g_hooks[2], SEL_DSP_PROCESS, SEL_TAG_FN(xf_process));
    set(&g_hooks[3], SEL_DSP_CLOSE, SEL_TAG_FN(xf_close));
    set(&g_hooks[4], SEL_DSP_PARAM, SEL_TAG_FN(xf_param));
    set(&g_hooks[5], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_DSP);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "crossfeed");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
