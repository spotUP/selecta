/* I1039: the CoreAudio output's device handling, driven through the plugin's own entry (SEL_PLUGIN_ENTRY, the hooks
 * OUT_OPEN, OUT_PUMP, OUT_DEVICES, OUT_LATENCY_QUERY) with a fake device layer instead of a sound card. The test
 * includes coreaudio.c so it can install the fake ca_backend; everything above the device layer (pref polling, the
 * default-follow flag, the stop/bind/start sequence, the latency refresh) is the shipped code.
 * Proves, with a call-count sentinel on the fake bind, that:
 *   - the device list reaches the caller with AirPlay marked,
 *   - a pref pick re-binds the running output to that device and the real latency of that device is reported,
 *   - an empty pref follows a system default change mid-play, and a picked device does not,
 *   - an unchanged pref and an unchanged default re-bind nothing. */
#include "../plugins/outputs/coreaudio/macos/coreaudio.c"

#include <stdlib.h>

static int g_fail;
#define CHECK(c)                                                                  \
    do {                                                                          \
        if (!(c)) {                                                               \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);          \
            g_fail++;                                                             \
        }                                                                         \
    } while (0)

enum { FAKE_N = 3 };
static const sel_out_device g_devs[FAKE_N] = {
    {"builtin-uid", "Built-in Speakers", SEL_OUT_TRANSPORT_BUILTIN, 1u},
    {"homepod-uid", "Living Room HomePod", SEL_OUT_TRANSPORT_AIRPLAY, 0u},
    {"usb-uid", "USB DAC", SEL_OUT_TRANSPORT_USB, 0u},
};

static struct {
    char bound[SEL_OUT_DEVICE_UID_MAX]; /* the device the last bind pointed at ("" = system default) */
    uint32_t bind_calls, starts, stops, watching;
    ca_out* watcher;
} g_fake;

static char g_pref[SEL_OUT_DEVICE_UID_MAX];

static uint32_t fake_list(sel_out_device* out, uint32_t max) {
    uint32_t i;
    for (i = 0; i < FAKE_N && i < max; i++) {
        out[i] = g_devs[i];
    }
    return i;
}
static int fake_bind(ca_out* o, const char* uid) {
    (void)o;
    snprintf(g_fake.bound, sizeof g_fake.bound, "%s", uid);
    g_fake.bind_calls++;
    return 1;
}
static int fake_start(ca_out* o) {
    (void)o;
    g_fake.starts++;
    return 1;
}
static void fake_stop(ca_out* o) {
    (void)o;
    g_fake.stops++;
}
static void fake_watch(ca_out* o, int on) {
    g_fake.watcher = on ? o : NULL;
    g_fake.watching += on ? 1u : 0u;
}
/* The HomePod buffers far more than the built-in speakers: a bind to it must show in the reported latency. */
static uint32_t fake_latency(ca_out* o) {
    (void)o;
    return strcmp(g_fake.bound, "homepod-uid") == 0 ? 187u : 12u;
}
/* The AirPlay route path's renderer, faked (I1039): a synchronizer clock that advances 256 frames per idle step once
 * started, and counters for every call. */
static struct {
    uint32_t creates, destroys, enqueues, starts, live;
    int64_t clock, last_start, queued_to;
    int running;
} g_avf;
static void* fake_av_create(uint32_t rate) {
    (void)rate;
    g_avf.creates++;
    g_avf.live++;
    g_avf.running = 0;
    return &g_avf;
}
static void fake_av_destroy(void* av) {
    (void)av;
    g_avf.destroys++;
    g_avf.live--;
}
static int fake_av_ready(void* av) {
    (void)av;
    return 1;
}
static int fake_av_enqueue(void* av, const int16_t* pcm, uint32_t frames, int64_t pts) {
    (void)av;
    (void)pcm;
    g_avf.enqueues++;
    g_avf.queued_to = pts + frames;
    return 1;
}
static int64_t fake_av_played(void* av) {
    (void)av;
    return g_avf.clock;
}
static void fake_av_start(void* av, int64_t pts) {
    (void)av;
    g_avf.starts++;
    g_avf.last_start = pts;
    g_avf.clock = pts;
    g_avf.running = 1;
}
static const ca_av_backend g_fake_av = {fake_av_create, fake_av_destroy, fake_av_ready, fake_av_enqueue, fake_av_played,
                                        fake_av_start};

/* One device callback per idle step: 256 frames. */
static void fake_idle(ca_out* o) {
    __atomic_add_fetch(&o->frames, 256u, __ATOMIC_RELAXED);
    __atomic_add_fetch(&o->callbacks, 1u, __ATOMIC_RELAXED);
    if (g_avf.running) {
        g_avf.clock += 256;
    }
}
static const ca_backend g_fake_be = {fake_list, fake_bind, fake_start, fake_stop, fake_watch, fake_latency, fake_idle};

static const char* host_config_get(const sel_host* h, const char* key, const char* def) {
    (void)h;
    return strcmp(key, "output.device") == 0 ? g_pref : def;
}
static void* host_alloc(const sel_host* h, size_t n, uint32_t kind) {
    (void)h;
    (void)kind;
    return malloc(n);
}
static void host_free(const sel_host* h, void* p) {
    (void)h;
    free(p);
}

static uint32_t pull_none(void* ctx, int16_t* out, uint32_t frames) {
    (void)ctx;
    memset(out, 0, (size_t)frames * 4u);
    return frames;
}

static uintptr_t hook(const sel_tag* hooks, uint32_t tag) {
    for (; hooks->tag != SEL_TAG_END; hooks++) {
        if (hooks->tag == tag) {
            return hooks->value;
        }
    }
    return 0;
}

int main(void) {
    sel_host host;
    const sel_tag* desc;
    const sel_tag* hooks;
    sel_out_devices_fn devices;
    sel_out_latency_query_fn latency;
    void* (*open_fn)(const sel_host*, uint32_t, uint32_t);
    void (*close_fn)(void*);
    void (*source_fn)(void*, sel_pcm_pull, void*);
    uint32_t (*pump_fn)(void*, uint32_t);
    const sel_tag* (*stats_fn)(void*);
    sel_out_device list[8];
    ca_out* co;
    int64_t ahead = 44100 * (int64_t)CA_AV_AHEAD_MS / 1000;
    void* out;
    uint32_t n, before;

    memset(&host, 0, sizeof host);
    host.size = sizeof host;
    host.api_version = 1u;
    host.mem_alloc = host_alloc;
    host.mem_free = host_free;
    host.config_get = host_config_get;
    g_be = &g_fake_be;
    g_av = &g_fake_av;

    desc = SEL_PLUGIN_ENTRY(1u);
    hooks = desc != NULL ? (const sel_tag*)hook(desc, SEL_PLUGIN_TAGS) : NULL;
    CHECK(hooks != NULL);
    if (hooks == NULL) {
        return 1;
    }
    devices = (sel_out_devices_fn)hook(hooks, SEL_OUT_DEVICES);
    latency = (sel_out_latency_query_fn)hook(hooks, SEL_OUT_LATENCY_QUERY);
    open_fn = (void* (*)(const sel_host*, uint32_t, uint32_t))hook(hooks, SEL_OUT_OPEN);
    close_fn = (void (*)(void*))hook(hooks, SEL_OUT_CLOSE);
    source_fn = (void (*)(void*, sel_pcm_pull, void*))hook(hooks, SEL_OUT_SET_SOURCE);
    pump_fn = (uint32_t (*)(void*, uint32_t))hook(hooks, SEL_OUT_PUMP);
    stats_fn = (const sel_tag* (*)(void*))hook(hooks, SEL_OUT_STATS);
    CHECK(devices != NULL && latency != NULL && open_fn != NULL && close_fn != NULL && source_fn != NULL && pump_fn != NULL &&
          stats_fn != NULL);
    if (devices == NULL || latency == NULL || open_fn == NULL || pump_fn == NULL || stats_fn == NULL) {
        return 1;
    }

    /* The list: names, uids, the default flag and the AirPlay mark all arrive. */
    n = devices(list, 8u);
    CHECK(n == FAKE_N + 1u);
    /* I1039: the last entry is the AirPlay route entry; picking it moves playback onto the AVFoundation path. */
    CHECK(strcmp(list[FAKE_N].uid, SEL_OUT_DEVICE_AIRPLAY_ROUTE) == 0 && list[FAKE_N].transport == SEL_OUT_TRANSPORT_AIRPLAY);
    CHECK(list[FAKE_N].is_default == 0u);
    CHECK(strcmp(list[0].name, "Built-in Speakers") == 0 && list[0].is_default == 1u);
    CHECK(strcmp(list[1].name, "Living Room HomePod") == 0 && list[1].transport == SEL_OUT_TRANSPORT_AIRPLAY);
    CHECK(list[2].transport != SEL_OUT_TRANSPORT_AIRPLAY);
    CHECK(devices(list, 2u) == 2u); /* the cap is honoured */

    /* Open with an empty pref: bound once to the system default, nominal real latency of the built-in speakers. */
    g_pref[0] = '\0';
    out = open_fn(&host, 44100u, 2u);
    CHECK(out != NULL);
    if (out == NULL) {
        return 1;
    }
    source_fn(out, pull_none, NULL);
    CHECK(g_fake.bind_calls == 1u && g_fake.bound[0] == '\0');
    CHECK(g_fake.watching == 1u && g_fake.watcher != NULL); /* the default is watched */
    CHECK(latency(out) == 12u);
    CHECK(pump_fn(out, 512u) == 512u);
    CHECK(g_fake.starts == 1u && g_fake.bind_calls == 1u); /* nothing re-bound by a quiet pump */

    /* A pick of the HomePod while playing: stop, bind to its uid, start again; the latency is the HomePod's. */
    snprintf(g_pref, sizeof g_pref, "%s", "homepod-uid");
    CHECK(pump_fn(out, 512u) == 512u);
    CHECK(g_fake.bind_calls == 2u && strcmp(g_fake.bound, "homepod-uid") == 0);
    CHECK(g_fake.stops == 1u && g_fake.starts == 2u);
    CHECK(latency(out) == 187u);

    /* The system default moves while the HomePod is picked: the pick wins, nothing re-binds. */
    before = g_fake.bind_calls;
    __atomic_store_n(&g_fake.watcher->default_changed, 1u, __ATOMIC_RELEASE);
    CHECK(pump_fn(out, 256u) == 256u);
    CHECK(g_fake.bind_calls == before && strcmp(g_fake.bound, "homepod-uid") == 0);

    /* Back to System default, then the default moves mid-play: the output follows it. */
    g_pref[0] = '\0';
    CHECK(pump_fn(out, 256u) == 256u);
    CHECK(g_fake.bind_calls == before + 1u && g_fake.bound[0] == '\0');
    CHECK(latency(out) == 12u);
    before = g_fake.bind_calls;
    __atomic_store_n(&g_fake.watcher->default_changed, 1u, __ATOMIC_RELEASE);
    CHECK(pump_fn(out, 256u) == 256u);
    CHECK(g_fake.bind_calls == before + 1u); /* followed the moved default */
    CHECK(pump_fn(out, 256u) == 256u);
    CHECK(g_fake.bind_calls == before + 1u); /* and only once */

    /* I1039: the AirPlay route entry is picked while playing: HAL stops, the renderer is made, latency is the queue. */
    before = g_fake.stops;
    snprintf(g_pref, sizeof g_pref, "%s", SEL_OUT_DEVICE_AIRPLAY_ROUTE);
    CHECK(pump_fn(out, 512u) == 512u);
    co = (ca_out*)out;
    CHECK(g_avf.creates == 1u && g_avf.live == 1u && co->av != NULL);
    CHECK(g_fake.stops == before + 1u);
    CHECK(latency(out) == CA_AV_AHEAD_MS);
    CHECK(g_avf.enqueues >= 1u && g_avf.starts == 1u && g_avf.last_start == 0);
    CHECK(hook(stats_fn(out), SEL_OUT_STAT_AV_BUFFERS) == g_avf.enqueues);

    /* A long pump paces on the synchronizer clock: never more than the queue depth plus one chunk ahead of it. */
    CHECK(pump_fn(out, 30000u) == 30000u);
    CHECK(g_avf.queued_to == co->av_pts);
    CHECK(co->av_pts - g_avf.clock < ahead + (int64_t)CA_AV_CHUNK);
    CHECK(g_avf.clock > 0 && g_avf.starts == 1u); /* the clock ran; no re-anchor while fed */

    /* Underrun: the clock passed everything queued, the next buffer re-anchors the clock at its own time. */
    g_avf.clock = co->av_pts + 5000;
    n = g_avf.starts;
    CHECK(pump_fn(out, 256u) == 256u);
    CHECK(g_avf.starts == n + 1u && g_avf.last_start == co->av_pts - 256);

    /* A HAL device is picked again: the renderer goes, HAL binds and starts. */
    before = g_fake.starts;
    snprintf(g_pref, sizeof g_pref, "%s", "homepod-uid");
    CHECK(pump_fn(out, 256u) == 256u);
    CHECK(g_avf.destroys == 1u && g_avf.live == 0u && co->av == NULL);
    CHECK(strcmp(g_fake.bound, "homepod-uid") == 0 && g_fake.starts == before + 1u);
    CHECK(latency(out) == 187u);

    /* Close in route mode drops the renderer. */
    snprintf(g_pref, sizeof g_pref, "%s", SEL_OUT_DEVICE_AIRPLAY_ROUTE);
    CHECK(pump_fn(out, 256u) == 256u);
    CHECK(g_avf.live == 1u);

    close_fn(out);
    CHECK(g_avf.live == 0u);
    CHECK(g_fake.watcher == NULL); /* the default is no longer watched */

    if (g_fail != 0) {
        fprintf(stderr, "coreaudio_device_test: %d checks failed\n", g_fail);
        return 1;
    }
    printf("coreaudio_device_ok binds=%u\n", (unsigned)g_fake.bind_calls);
    return 0;
}
