#include "p14_out.h"

#include <string.h>

typedef struct p14_out {
    const sel_host* host;
    const sel_p14_out_kind* kind;
    void* bus_ctx;
    sel_p14 sink;
    uint32_t buf_frames;
    uint32_t have; /* frames pulled into pcm, not yet submitted */
    sel_pcm_pull pull;
    void* pull_ctx;
    int16_t* pcm;
    uint32_t max_err;
    char hash_text[9];
    sel_tag stats[6];
} p14_out;

static uint32_t buffer_frames(const sel_host* host) {
    uint32_t n = 0;
    const char* v = host->config_get != NULL ? host->config_get(host, "paula14.buffer", "") : "";
    while (*v >= '0' && *v <= '9') {
        n = n * 10u + (uint32_t)(*v++ - '0');
    }
    return n >= 64u && n <= 8192u ? n : SEL_P14_BUF_FRAMES;
}

void* sel_p14_out_open(const sel_p14_out_kind* kind, const sel_host* host, uint32_t rate, uint32_t channels) {
    p14_out* s;
    sel_paula_bus bus;
    if (channels != 2) {
        return NULL;
    }
    s = (p14_out*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    s->host = host;
    s->kind = kind;
    s->pull = NULL;
    s->pull_ctx = NULL;
    s->have = 0;
    s->buf_frames = buffer_frames(host);
    s->pcm = (int16_t*)host->mem_alloc(host, s->buf_frames * 2u * sizeof(int16_t), SEL_MEM_FAST);
    bus.write = NULL;
    bus.set_lc = NULL;
    bus.ctx = NULL;
    s->bus_ctx = s->pcm != NULL ? kind->bus_open(host, &bus) : NULL;
    if (s->bus_ctx == NULL || sel_p14_open(&s->sink, host, &bus, rate, s->buf_frames, kind->strict_rate) != 0) {
        if (s->bus_ctx != NULL) {
            kind->bus_close(host, s->bus_ctx);
        }
        if (s->pcm != NULL) {
            host->mem_free(host, s->pcm);
        }
        host->mem_free(host, s);
        return NULL;
    }
    s->max_err = sel_p14_max_error(&s->sink.table);
    return s;
}

static void out_close(void* state) {
    p14_out* s = (p14_out*)state;
    if (s) {
        const sel_host* host = s->host;
        if (s->have > 0) { /* flush the partial final buffer, padded with silence */
            memset(s->pcm + s->have * 2u, 0, (s->buf_frames - s->have) * 2u * sizeof(int16_t));
            sel_p14_submit_n(&s->sink, s->pcm, s->have);
            s->have = 0;
        }
        sel_p14_close(&s->sink);
        s->kind->bus_close(host, s->bus_ctx);
        host->mem_free(host, s->pcm);
        host->mem_free(host, s);
    }
}

static void out_set_source(void* state, sel_pcm_pull fn, void* ctx) {
    p14_out* s = (p14_out*)state;
    s->pull = fn;
    s->pull_ctx = ctx;
}

/* Pulls the mixed stream, submitting one chip buffer each time it fills. On the Amiga the audio task calls
 * this once per CIA tick, which paces the swaps. */
static uint32_t out_pump(void* state, uint32_t frames) {
    p14_out* s = (p14_out*)state;
    uint32_t done = 0;
    if (s->pull == NULL) {
        return 0;
    }
    while (done < frames) {
        uint32_t want = frames - done, got;
        if (want > s->buf_frames - s->have) {
            want = s->buf_frames - s->have;
        }
        got = s->pull(s->pull_ctx, s->pcm + s->have * 2u, want);
        if (got == 0) {
            break;
        }
        s->have += got;
        done += got;
        if (s->have == s->buf_frames) {
            sel_p14_submit(&s->sink, s->pcm);
            s->have = 0;
        }
    }
    return done;
}

static const sel_tag* out_stats(void* state) {
    p14_out* s = (p14_out*)state;
    static const char hex[] = "0123456789abcdef";
    uint32_t i, n = 0;
    for (i = 0; i < 8u; i++) {
        s->hash_text[i] = hex[(s->sink.hash >> (28u - 4u * i)) & 15u];
    }
    s->hash_text[8] = 0;
    s->stats[n].tag = SEL_OUT_STAT_FRAMES;
    s->stats[n++].value = s->sink.frames + s->have;
    s->stats[n].tag = SEL_OUT_STAT_P14_FRAMES;
    s->stats[n++].value = s->sink.frames;
    s->stats[n].tag = SEL_OUT_STAT_P14_MAX_ERR;
    s->stats[n++].value = s->max_err;
    s->stats[n].tag = SEL_OUT_STAT_P14_HASH;
    s->stats[n++].value = (uintptr_t)s->hash_text;
    s->stats[n].tag = SEL_OUT_STAT_P14_BUFS;
    s->stats[n++].value = s->sink.bufs;
    s->stats[n].tag = SEL_TAG_END;
    s->stats[n].value = 0;
    return s->stats;
}

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

static uint32_t out_rate_for(const sel_tag* machine) {
    (void)machine;
    return sel_p14_max_rate();
}

const sel_tag* sel_p14_out_describe(const sel_p14_out_kind* kind, sel_out_open_fn open, sel_tag* hooks, sel_tag* desc) {
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
    set(&hooks[n++], SEL_OUT_RATE_FOR, SEL_TAG_FN(out_rate_for));
    set(&hooks[n++], SEL_OUT_MEM_CHIP, 4u * 2u * SEL_P14_BUF_FRAMES);
    set(&hooks[n++], SEL_TAG_END, 0);

    set(&desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT);
    set(&desc[1], SEL_PLUGIN_ID, (uintptr_t)kind->id);
    set(&desc[2], SEL_PLUGIN_VERSION, 1);
    set(&desc[3], SEL_PLUGIN_TAGS, (uintptr_t)hooks);
    set(&desc[4], SEL_TAG_END, 0);
    return desc;
}
