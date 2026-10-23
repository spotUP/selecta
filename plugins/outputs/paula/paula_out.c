#include "paula_out.h"

#define PAULA_BLOCK 512u

typedef struct paula_out {
    const sel_host* host;
    const sel_paula_out_kind* kind;
    void* bus_ctx;
    sel_paula paula;
    uint32_t frames;
    sel_pcm_pull pull;
    void* pull_ctx;
    int16_t* buf;
    sel_tag stats[6];
} paula_out;

void* sel_paula_out_open(const sel_paula_out_kind* kind, const sel_host* host, uint32_t rate, uint32_t channels) {
    paula_out* s;
    sel_paula_bus bus;
    (void)rate;
    if (channels != 2) {
        return NULL;
    }
    s = (paula_out*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    s->buf = (int16_t*)host->mem_alloc(host, PAULA_BLOCK * 2u * sizeof(int16_t), SEL_MEM_FAST);
    if (s->buf == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    s->host = host;
    s->kind = kind;
    s->pull = NULL;
    s->pull_ctx = NULL;
    s->frames = 0;
    bus.write = NULL;
    bus.set_lc = NULL;
    bus.ctx = NULL;
    s->bus_ctx = kind->bus_open(host, &bus);
    if (s->bus_ctx == NULL) {
        host->mem_free(host, s->buf);
        host->mem_free(host, s);
        return NULL;
    }
    sel_paula_init(&s->paula, host, &bus);
    return s;
}

static void out_close(void* state) {
    paula_out* s = (paula_out*)state;
    if (s) {
        const sel_host* host = s->host;
        sel_paula_shutdown(&s->paula);
        s->kind->bus_close(host, s->bus_ctx);
        host->mem_free(host, s->buf);
        host->mem_free(host, s);
    }
}

static void out_set_source(void* state, sel_pcm_pull fn, void* ctx) {
    paula_out* s = (paula_out*)state;
    s->pull = fn;
    s->pull_ctx = ctx;
}

/* Voice replayers drive the registers from the engine's tick; pulling is what advances it. */
static uint32_t out_pump(void* state, uint32_t frames) {
    paula_out* s = (paula_out*)state;
    uint32_t done = 0;
    if (s->pull == NULL) {
        return 0;
    }
    while (done < frames) {
        uint32_t want = frames - done, got;
        if (want > PAULA_BLOCK) {
            want = PAULA_BLOCK;
        }
        got = s->pull(s->pull_ctx, s->buf, want);
        if (got == 0) {
            break;
        }
        done += got;
    }
    s->frames += done;
    return done;
}

static const sel_tag* out_stats(void* state) {
    paula_out* s = (paula_out*)state;
    uint32_t n = 0;
    s->stats[n].tag = SEL_OUT_STAT_FRAMES;
    s->stats[n++].value = s->frames;
    if (s->kind->bus_stats != NULL) {
        n += s->kind->bus_stats(s->bus_ctx, &s->stats[n]);
    }
    s->stats[n].tag = SEL_TAG_END;
    s->stats[n].value = 0;
    return s->stats;
}

static const sel_voices* out_voice_backend(void* state) {
    return sel_paula_api(&((paula_out*)state)->paula);
}

static const char g_voices_marker = 1;

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* sel_paula_out_describe(const sel_paula_out_kind* kind, sel_out_open_fn open, sel_tag* hooks, sel_tag* desc) {
    uint32_t n = 0;
    set(&hooks[n++], SEL_OUT_NAME, (uintptr_t)kind->name);
    set(&hooks[n++], SEL_OUT_RATES, (uintptr_t) "any");
    set(&hooks[n++], SEL_OUT_CHANNELS, 2);
    set(&hooks[n++], SEL_OUT_LATENCY_MS, 0);
    set(&hooks[n++], SEL_OUT_OPEN, SEL_TAG_FN(open));
    set(&hooks[n++], SEL_OUT_CLOSE, SEL_TAG_FN(out_close));
    set(&hooks[n++], SEL_OUT_SET_SOURCE, SEL_TAG_FN(out_set_source));
    set(&hooks[n++], SEL_OUT_PUMP, SEL_TAG_FN(out_pump));
    set(&hooks[n++], SEL_OUT_STATS, SEL_TAG_FN(out_stats));
    set(&hooks[n++], SEL_OUT_VOICES, (uintptr_t)&g_voices_marker);
    set(&hooks[n++], SEL_OUT_VOICE_BACKEND, SEL_TAG_FN(out_voice_backend));
    set(&hooks[n++], SEL_OUT_VOICE_MAX, SEL_PAULA_VOICES);
    if (kind->hardware) {
        set(&hooks[n++], SEL_OUT_PAULA_HW, 1);
    }
    set(&hooks[n++], SEL_TAG_END, 0);

    set(&desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT);
    set(&desc[1], SEL_PLUGIN_ID, (uintptr_t)kind->id);
    set(&desc[2], SEL_PLUGIN_VERSION, 1);
    set(&desc[3], SEL_PLUGIN_TAGS, (uintptr_t)hooks);
    set(&desc[4], SEL_TAG_END, 0);
    return desc;
}
