#include "ahi_out.h"

#include <string.h>

typedef struct ahi_out {
    const sel_host* host;
    const sel_ahi_backend* be;
    void* dev;
    uint32_t rate;
    uint32_t buf_frames;
    uint32_t cur;  /* buffer being filled */
    uint32_t have; /* frames in pcm[cur] */
    uint32_t frames;
    uint32_t bufs;
    uint32_t underruns;
    int16_t* pcm[2];
    sel_pcm_pull pull;
    void* pull_ctx;
    sel_tag stats[6];
} ahi_out;

void* sel_ahi_out_open(const sel_ahi_backend* be, const sel_host* host, uint32_t rate, uint32_t channels) {
    ahi_out* s;
    uint32_t cpu;
    if (channels != 2) {
        return NULL;
    }
    s = (ahi_out*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    cpu = sel_ahi_host_cpu(host);
    s->host = host;
    s->be = be;
    s->rate = rate != 0u ? rate : sel_ahi_default_rate(cpu);
    s->buf_frames = sel_ahi_pick_buffer(cpu, host->config_get != NULL ? host->config_get(host, "output.buffer", "0") : "0");
    s->pcm[0] = (int16_t*)host->mem_alloc(host, s->buf_frames * 4u * sizeof(int16_t), SEL_MEM_FAST);
    s->pcm[1] = s->pcm[0] != NULL ? s->pcm[0] + s->buf_frames * 2u : NULL;
    s->dev = s->pcm[0] != NULL ? be->open(host, s->rate, s->buf_frames) : NULL;
    if (s->dev == NULL) {
        if (s->pcm[0] != NULL) {
            host->mem_free(host, s->pcm[0]);
        }
        host->mem_free(host, s);
        return NULL;
    }
    return s;
}

static void submit(ahi_out* s, uint32_t frames) {
    if (s->bufs > 0u && s->be->finished(s->dev, s->cur ^ 1u)) {
        s->underruns++; /* the other buffer ran out before this one arrived */
    }
    s->be->submit(s->dev, s->cur, s->pcm[s->cur], frames);
    s->bufs++;
    s->frames += frames;
    s->cur ^= 1u;
    s->have = 0;
    s->be->wait(s->dev, s->cur); /* the buffer to fill next must be free: this paces the pump */
}

static void out_close(void* state) {
    ahi_out* s = (ahi_out*)state;
    if (s) {
        const sel_host* host = s->host;
        if (s->have > 0) { /* flush the partial final buffer, padded with silence */
            memset(s->pcm[s->cur] + s->have * 2u, 0, (s->buf_frames - s->have) * 2u * sizeof(int16_t));
            submit(s, s->have);
        }
        s->be->wait(s->dev, 0);
        s->be->wait(s->dev, 1);
        s->be->close(host, s->dev);
        host->mem_free(host, s->pcm[0]);
        host->mem_free(host, s);
    }
}

static void out_set_source(void* state, sel_pcm_pull fn, void* ctx) {
    ahi_out* s = (ahi_out*)state;
    s->pull = fn;
    s->pull_ctx = ctx;
}

static uint32_t out_pump(void* state, uint32_t frames) {
    ahi_out* s = (ahi_out*)state;
    uint32_t done = 0;
    if (s->pull == NULL) {
        return 0;
    }
    while (done < frames) {
        uint32_t want = frames - done, got;
        if (want > s->buf_frames - s->have) {
            want = s->buf_frames - s->have;
        }
        got = s->pull(s->pull_ctx, s->pcm[s->cur] + s->have * 2u, want);
        if (got == 0) {
            break;
        }
        s->have += got;
        done += got;
        if (s->have == s->buf_frames) {
            submit(s, s->buf_frames);
        }
    }
    return done;
}

static const sel_tag* out_stats(void* state) {
    ahi_out* s = (ahi_out*)state;
    uint32_t n = 0;
    s->stats[n].tag = SEL_OUT_STAT_FRAMES;
    s->stats[n++].value = s->frames + s->have;
    s->stats[n].tag = SEL_OUT_STAT_AHI_BUFS;
    s->stats[n++].value = s->bufs;
    s->stats[n].tag = SEL_OUT_STAT_AHI_UNDERRUNS;
    s->stats[n++].value = s->underruns;
    s->stats[n].tag = SEL_OUT_STAT_AHI_BUFFER;
    s->stats[n++].value = s->buf_frames;
    s->stats[n].tag = SEL_OUT_STAT_AHI_RATE;
    s->stats[n++].value = s->rate;
    s->stats[n].tag = SEL_TAG_END;
    s->stats[n].value = 0;
    return s->stats;
}

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

static uint32_t out_rate_for(const sel_tag* machine) {
    return sel_ahi_default_rate((uint32_t)sel_tag_find(machine, SEL_MACHINE_CPU, SEL_CPU_UNKNOWN));
}

const sel_tag* sel_ahi_out_describe(const sel_ahi_backend* be, sel_out_open_fn open, sel_tag* hooks, sel_tag* desc) {
    uint32_t n = 0;
    set(&hooks[n++], SEL_OUT_NAME, (uintptr_t)be->name);
    set(&hooks[n++], SEL_OUT_RATES, (uintptr_t) "any");
    set(&hooks[n++], SEL_OUT_CHANNELS, 2);
    set(&hooks[n++], SEL_OUT_LATENCY_MS, 0);
    set(&hooks[n++], SEL_OUT_OPEN, SEL_TAG_FN(open));
    set(&hooks[n++], SEL_OUT_CLOSE, SEL_TAG_FN(out_close));
    set(&hooks[n++], SEL_OUT_SET_SOURCE, SEL_TAG_FN(out_set_source));
    set(&hooks[n++], SEL_OUT_PUMP, SEL_TAG_FN(out_pump));
    set(&hooks[n++], SEL_OUT_STATS, SEL_TAG_FN(out_stats));
    set(&hooks[n++], SEL_OUT_RATE_FOR, SEL_TAG_FN(out_rate_for));
    set(&hooks[n++], SEL_TAG_END, 0);

    set(&desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT);
    set(&desc[1], SEL_PLUGIN_ID, (uintptr_t)be->id);
    set(&desc[2], SEL_PLUGIN_VERSION, 1);
    set(&desc[3], SEL_PLUGIN_TAGS, (uintptr_t)hooks);
    set(&desc[4], SEL_TAG_END, 0);
    return desc;
}
