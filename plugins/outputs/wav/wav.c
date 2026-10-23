/* WAV output (design doc section 10, plan Phase 3): a sink with no clock of
 * its own that writes RIFF PCM16 little endian, stereo. The path comes from
 * the host config key "wav.path". OUT_PUMP pulls frames from the engine and
 * writes them; the header sizes are patched at close. C stdio only. The
 * stdio buffer is host memory handed over with setvbuf at open, so writing
 * never allocates. No floating point. */
#include <selecta/output.h>

#include <stdio.h>

#define WAV_BLOCK 512u
#define WAV_HEADER 44u
#define WAV_IOBUF 16384u

typedef struct wav_out {
    const sel_host* host;
    sel_pcm_pull pull;
    void* pull_ctx;
    FILE* fp;
    char* iobuf;
    int16_t* pcm;
    unsigned char* bytes;
    uint32_t rate;
    uint32_t frames;
    sel_tag stats[2];
} wav_out;

static void put32(unsigned char* p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

static void put16(unsigned char* p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
}

static void copy4(unsigned char* p, const char* s) {
    p[0] = (unsigned char)s[0];
    p[1] = (unsigned char)s[1];
    p[2] = (unsigned char)s[2];
    p[3] = (unsigned char)s[3];
}

/* Header for `data_bytes` of sample data, 2 channels, 16 bit. */
static void make_header(unsigned char* h, uint32_t rate, uint32_t data_bytes) {
    copy4(h, "RIFF");
    put32(h + 4, 36u + data_bytes);
    copy4(h + 8, "WAVE");
    copy4(h + 12, "fmt ");
    put32(h + 16, 16u);
    put16(h + 20, 1u); /* PCM */
    put16(h + 22, 2u);
    put32(h + 24, rate);
    put32(h + 28, rate * 4u);
    put16(h + 32, 4u);
    put16(h + 34, 16u);
    copy4(h + 36, "data");
    put32(h + 40, data_bytes);
}

static void wav_release(wav_out* o) {
    const sel_host* host = o->host;
    if (o->fp) {
        fclose(o->fp);
    }
    host->mem_free(host, o->iobuf);
    host->mem_free(host, o->pcm);
    host->mem_free(host, o->bytes);
    host->mem_free(host, o);
}

static void* wav_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    wav_out* o;
    const char* path;
    unsigned char header[WAV_HEADER];
    if (channels != 2 || rate == 0 || host->config_get == NULL) {
        return NULL;
    }
    path = host->config_get(host, "wav.path", NULL);
    if (path == NULL) {
        return NULL;
    }
    o = (wav_out*)host->mem_alloc(host, sizeof *o, SEL_MEM_FAST);
    if (o == NULL) {
        return NULL;
    }
    o->host = host;
    o->pull = NULL;
    o->pull_ctx = NULL;
    o->fp = NULL;
    o->rate = rate;
    o->frames = 0;
    o->iobuf = (char*)host->mem_alloc(host, WAV_IOBUF, SEL_MEM_FAST);
    o->pcm = (int16_t*)host->mem_alloc(host, WAV_BLOCK * 2u * sizeof(int16_t), SEL_MEM_FAST);
    o->bytes = (unsigned char*)host->mem_alloc(host, WAV_BLOCK * 4u, SEL_MEM_FAST);
    if (o->iobuf == NULL || o->pcm == NULL || o->bytes == NULL) {
        wav_release(o);
        return NULL;
    }
    o->fp = fopen(path, "wb");
    if (o->fp == NULL || setvbuf(o->fp, o->iobuf, _IOFBF, WAV_IOBUF) != 0) {
        wav_release(o);
        return NULL;
    }
    make_header(header, rate, 0);
    if (fwrite(header, 1, WAV_HEADER, o->fp) != WAV_HEADER) {
        wav_release(o);
        return NULL;
    }
    return o;
}

static void wav_close(void* state) {
    wav_out* o = (wav_out*)state;
    unsigned char header[WAV_HEADER];
    if (o == NULL) {
        return;
    }
    make_header(header, o->rate, o->frames * 4u);
    if (fseek(o->fp, 0, SEEK_SET) == 0) {
        (void)fwrite(header, 1, WAV_HEADER, o->fp);
    }
    wav_release(o);
}

static void wav_set_source(void* state, sel_pcm_pull fn, void* ctx) {
    wav_out* o = (wav_out*)state;
    o->pull = fn;
    o->pull_ctx = ctx;
}

static uint32_t wav_pump(void* state, uint32_t frames) {
    wav_out* o = (wav_out*)state;
    uint32_t done = 0;
    if (o->pull == NULL) {
        return 0;
    }
    while (done < frames) {
        uint32_t want = frames - done;
        uint32_t got;
        uint32_t i;
        if (want > WAV_BLOCK) {
            want = WAV_BLOCK;
        }
        got = o->pull(o->pull_ctx, o->pcm, want);
        if (got == 0) {
            break;
        }
        for (i = 0; i < got * 2u; i++) {
            put16(o->bytes + i * 2u, (uint32_t)(uint16_t)o->pcm[i]);
        }
        if (fwrite(o->bytes, 4, got, o->fp) != got) {
            break;
        }
        done += got;
    }
    o->frames += done;
    return done;
}

static const sel_tag* wav_stats(void* state) {
    wav_out* o = (wav_out*)state;
    o->stats[0].tag = SEL_OUT_STAT_FRAMES;
    o->stats[0].value = o->frames;
    o->stats[1].tag = SEL_TAG_END;
    o->stats[1].value = 0;
    return o->stats;
}

/* Filled at run time: a function pointer cast to an integer is not a
 * constant expression in C99. */
static sel_tag g_hooks[12];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_OUT_NAME, (uintptr_t) "WAV file");
    set(&g_hooks[1], SEL_OUT_RATES, (uintptr_t) "any");
    set(&g_hooks[2], SEL_OUT_CHANNELS, 2);
    set(&g_hooks[3], SEL_OUT_LATENCY_MS, 0);
    set(&g_hooks[4], SEL_OUT_OPEN, SEL_TAG_FN(wav_open));
    set(&g_hooks[5], SEL_OUT_CLOSE, SEL_TAG_FN(wav_close));
    set(&g_hooks[6], SEL_OUT_SET_SOURCE, SEL_TAG_FN(wav_set_source));
    set(&g_hooks[7], SEL_OUT_PUMP, SEL_TAG_FN(wav_pump));
    set(&g_hooks[8], SEL_OUT_STATS, SEL_TAG_FN(wav_stats));
    set(&g_hooks[9], SEL_OUT_CLOCKLESS, 1);
    set(&g_hooks[10], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "wav");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
