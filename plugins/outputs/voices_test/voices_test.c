/* voices-test output (plan M1.3 I95): the null sink with the SEL_OUT_VOICES
 * capability, standing in for a Paula-direct output. The engine sees a path
 * that owns its voices: mixed_path is 0, so crossfade and DSP answer
 * unsupported_path. OUT_PUMP pulls frames and drops them. */
#include <selecta/output.h>

#define VT_BLOCK 512u

typedef struct vt_out {
    const sel_host* host;
    sel_pcm_pull pull;
    void* pull_ctx;
    int16_t* buf;
    uint32_t frames;
    sel_tag stats[2];
} vt_out;

static void* vt_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    vt_out* o;
    (void)rate;
    if (channels != 2) {
        return NULL;
    }
    o = (vt_out*)host->mem_alloc(host, sizeof *o, SEL_MEM_FAST);
    if (o == NULL) {
        return NULL;
    }
    o->buf = (int16_t*)host->mem_alloc(host, VT_BLOCK * 2u * sizeof(int16_t), SEL_MEM_FAST);
    if (o->buf == NULL) {
        host->mem_free(host, o);
        return NULL;
    }
    o->host = host;
    o->pull = NULL;
    o->pull_ctx = NULL;
    o->frames = 0;
    return o;
}

static void vt_close(void* state) {
    vt_out* o = (vt_out*)state;
    if (o) {
        const sel_host* host = o->host;
        host->mem_free(host, o->buf);
        host->mem_free(host, o);
    }
}

static void vt_set_source(void* state, sel_pcm_pull fn, void* ctx) {
    vt_out* o = (vt_out*)state;
    o->pull = fn;
    o->pull_ctx = ctx;
}

static uint32_t vt_pump(void* state, uint32_t frames) {
    vt_out* o = (vt_out*)state;
    uint32_t done = 0;
    if (o->pull == NULL) {
        return 0;
    }
    while (done < frames) {
        uint32_t want = frames - done;
        uint32_t got;
        if (want > VT_BLOCK) {
            want = VT_BLOCK;
        }
        got = o->pull(o->pull_ctx, o->buf, want);
        if (got == 0) {
            break;
        }
        done += got;
    }
    o->frames += done;
    return done;
}

static const sel_tag* vt_stats(void* state) {
    vt_out* o = (vt_out*)state;
    o->stats[0].tag = SEL_OUT_STAT_FRAMES;
    o->stats[0].value = o->frames;
    o->stats[1].tag = SEL_TAG_END;
    o->stats[1].value = 0;
    return o->stats;
}

/* Filled at run time: a function pointer cast to an integer is not a
 * constant expression in C99. */
static sel_tag g_hooks[12];
static const char g_voices_marker = 1;
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_OUT_NAME, (uintptr_t) "Voices test");
    set(&g_hooks[1], SEL_OUT_RATES, (uintptr_t) "any");
    set(&g_hooks[2], SEL_OUT_CHANNELS, 2);
    set(&g_hooks[3], SEL_OUT_LATENCY_MS, 0);
    set(&g_hooks[4], SEL_OUT_OPEN, SEL_TAG_FN(vt_open));
    set(&g_hooks[5], SEL_OUT_CLOSE, SEL_TAG_FN(vt_close));
    set(&g_hooks[6], SEL_OUT_SET_SOURCE, SEL_TAG_FN(vt_set_source));
    set(&g_hooks[7], SEL_OUT_PUMP, SEL_TAG_FN(vt_pump));
    set(&g_hooks[8], SEL_OUT_STATS, SEL_TAG_FN(vt_stats));
    set(&g_hooks[9], SEL_OUT_VOICES, (uintptr_t)&g_voices_marker);
    set(&g_hooks[10], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "voices-test");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
