/* Null output (design doc section 10, plan Phase 5): a sink with no clock
 * of its own. OUT_PUMP pulls frames from the engine and drops them, so
 * tests can drive the whole render path with no audio device. */
#include <selecta/output.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <time.h>
#endif

#define NULL_BLOCK 512u

typedef struct null_out {
    const sel_host* host;
    sel_pcm_pull pull;
    void* pull_ctx;
    int16_t* buf;
    uint32_t rate;
    int paced; /* I1082a: SELECTA_NULL_PACED sleeps the pump for the real time of the frames it drops */
    uint32_t frames;
    uint32_t peak; /* largest absolute sample pulled (STAT_PEAK) */
    sel_tag stats[3];
} null_out;

static int g_paced; /* set by the plugin entry from SELECTA_NULL_PACED */

static void* null_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    null_out* o;
    if (channels != 2) {
        return NULL;
    }
    o = (null_out*)host->mem_alloc(host, sizeof *o, SEL_MEM_FAST);
    if (o == NULL) {
        return NULL;
    }
    o->buf = (int16_t*)host->mem_alloc(host, NULL_BLOCK * 2u * sizeof(int16_t), SEL_MEM_FAST);
    if (o->buf == NULL) {
        host->mem_free(host, o);
        return NULL;
    }
    o->host = host;
    o->pull = NULL;
    o->pull_ctx = NULL;
    o->rate = rate ? rate : 44100u;
    o->paced = g_paced;
    o->frames = 0;
    o->peak = 0;
    return o;
}

static void null_close(void* state) {
    null_out* o = (null_out*)state;
    if (o) {
        const sel_host* host = o->host;
        host->mem_free(host, o->buf);
        host->mem_free(host, o);
    }
}

static void null_set_source(void* state, sel_pcm_pull fn, void* ctx) {
    null_out* o = (null_out*)state;
    o->pull = fn;
    o->pull_ctx = ctx;
}

static uint32_t null_pump(void* state, uint32_t frames) {
    null_out* o = (null_out*)state;
    uint32_t done = 0;
    uint32_t i;
    if (o->pull == NULL) {
        return 0;
    }
    while (done < frames) {
        uint32_t want = frames - done;
        uint32_t got;
        if (want > NULL_BLOCK) {
            want = NULL_BLOCK;
        }
        got = o->pull(o->pull_ctx, o->buf, want);
        if (got == 0) {
            break;
        }
        for (i = 0; i < got * 2u; i++) {
            int32_t v = o->buf[i];
            uint32_t m = (uint32_t)(v < 0 ? -v : v);
            if (m > o->peak) {
                o->peak = m;
            }
        }
        done += got;
    }
    o->frames += done;
#if defined(__unix__) || defined(__APPLE__)
    if (o->paced && done > 0) { /* a clocked, silent sink: the caller waits the duration of the audio like a device would */
        struct timespec ts;
        uint64_t ns = (uint64_t)done * 1000000000ull / o->rate;
        ts.tv_sec = (time_t)(ns / 1000000000ull);
        ts.tv_nsec = (long)(ns % 1000000000ull);
        nanosleep(&ts, NULL);
    }
#endif
    return done;
}

static const sel_tag* null_stats(void* state) {
    null_out* o = (null_out*)state;
    o->stats[0].tag = SEL_OUT_STAT_FRAMES;
    o->stats[0].value = o->frames;
    o->stats[1].tag = SEL_OUT_STAT_PEAK;
    o->stats[1].value = o->peak;
    o->stats[2].tag = SEL_TAG_END;
    o->stats[2].value = 0;
    return o->stats;
}

/* The headless sink has no device buffer, so it reports 0 ms by default. SELECTA_NULL_LATENCY_MS sets a distinct value
 * so a test can see the engine take the queried latency instead of the nominal one (I1042, I1085). */
static uint32_t null_latency_query(void* state) {
    (void)state;
    const char* e = getenv("SELECTA_NULL_LATENCY_MS");
    return e ? (uint32_t)strtoul(e, NULL, 10) : 0u;
}

/* A fixed device list: the null output is the headless test sink, so the dui tests get a device picker with known rows
 * (system default first, one AirPlay speaker) without a sound card (I1039). */
static uint32_t null_devices(sel_out_device* out, uint32_t max) {
    static const struct {
        const char* uid;
        const char* name;
        uint32_t transport;
    } fake[] = {{"null-builtin", "Built-in Speakers", SEL_OUT_TRANSPORT_BUILTIN},
                {"null-homepod", "Living Room HomePod", SEL_OUT_TRANSPORT_AIRPLAY}};
    uint32_t i;
    for (i = 0; i < 2u && i < max; i++) {
        memset(&out[i], 0, sizeof out[i]);
        snprintf(out[i].uid, sizeof out[i].uid, "%s", fake[i].uid);
        snprintf(out[i].name, sizeof out[i].name, "%s", fake[i].name);
        out[i].transport = fake[i].transport;
        out[i].is_default = i == 0;
    }
    return i;
}

/* Filled at run time: a function pointer cast to an integer is not a
 * constant expression in C99. */
static sel_tag g_hooks[13];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_OUT_NAME, (uintptr_t) "Null");
    set(&g_hooks[1], SEL_OUT_RATES, (uintptr_t) "any");
    set(&g_hooks[2], SEL_OUT_CHANNELS, 2);
    set(&g_hooks[3], SEL_OUT_LATENCY_MS, 0);
    set(&g_hooks[4], SEL_OUT_OPEN, SEL_TAG_FN(null_open));
    set(&g_hooks[5], SEL_OUT_CLOSE, SEL_TAG_FN(null_close));
    set(&g_hooks[6], SEL_OUT_SET_SOURCE, SEL_TAG_FN(null_set_source));
    set(&g_hooks[7], SEL_OUT_PUMP, SEL_TAG_FN(null_pump));
    set(&g_hooks[8], SEL_OUT_STATS, SEL_TAG_FN(null_stats));
    {
        const char* pc = getenv("SELECTA_NULL_PACED"); /* I1082a: paced silent output for reachability tests of a clocked shell */
        g_paced = pc != NULL && pc[0] != '\0' && pc[0] != '0';
    }
    set(&g_hooks[9], SEL_OUT_CLOCKLESS, g_paced ? 0 : 1);
    set(&g_hooks[10], SEL_OUT_DEVICES, SEL_TAG_FN(null_devices));
    set(&g_hooks[11], SEL_OUT_LATENCY_QUERY, SEL_TAG_FN(null_latency_query));
    set(&g_hooks[12], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "null");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
