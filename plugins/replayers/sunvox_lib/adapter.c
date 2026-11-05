/* SunVox library replayer (row I706b): a RENDER-kind adapter over the official precompiled SunVox Library (libsunvox,
 * Alexander Zolotov, MIT licence with a "Powered by SunVox" credit requirement). Desktop only: the library is a closed
 * binary build, so Amiga and web keep the BSD 2009 engine plugin (plugins/replayers/sunvox, I706a). This module wins
 * over that plugin (SEL_RP_PRIORITY 20 against 10) and reads songs of every SunVox version, including the ones the
 * 2009 engine refuses.
 *
 * The library is loaded with dlopen, never linked: it sits in "sunvox_lib/" beside this module (the app plugin folder,
 * or any folder on SELECTA_PLUGIN_PATH), or at the path in $SELECTA_SUNVOX_LIB. Without it the probe says NO and the
 * BSD plugin keeps playing. The library keeps global state: one song at a time (a second open fails). It runs in its
 * offline mode (no audio device): we pull samples with sv_audio_callback. */
#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <dlfcn.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SV_INIT_FLAG_NO_DEBUG_OUTPUT (1 << 0)
#define SV_INIT_FLAG_OFFLINE (1 << 1)
#define SV_INIT_FLAG_AUDIO_INT16 (1 << 2)
#define SV_INIT_FLAG_ONE_THREAD (1 << 4)
#define SVL_MAX_SECONDS 1200u

#ifdef __APPLE__
#define SVL_LIB_NAME "sunvox.dylib"
#else
#define SVL_LIB_NAME "sunvox.so"
#endif

typedef struct svl_api {
    void* dl;
    int (*init)(const char*, int, int, uint32_t);
    int (*deinit)(void);
    int (*open_slot)(int);
    int (*close_slot)(int);
    int (*load_from_memory)(int, void*, uint32_t);
    int (*play_from_beginning)(int);
    int (*set_autostop)(int, int);
    int (*end_of_song)(int);
    int (*audio_callback)(void*, int, int, uint32_t);
    uint32_t (*get_song_length_frames)(int);
    uint32_t (*get_ticks)(void);
    int (*rewind)(int, int);
    const char* (*get_song_name)(int);
} svl_api;

static svl_api g_api;
static int g_api_tried;
static int g_busy;

static void* find_sym(void* dl, const char* name) {
    return dlsym(dl, name);
}

#define SVL_SYM(field, name)                                                                                          \
    do {                                                                                                              \
        void* sym_ = find_sym(g_api.dl, name);                                                                        \
        if (sym_ == NULL) {                                                                                           \
            dlclose(g_api.dl);                                                                                        \
            memset(&g_api, 0, sizeof g_api);                                                                          \
            return 0;                                                                                                 \
        }                                                                                                             \
        memcpy(&g_api.field, &sym_, sizeof sym_);                                                                     \
    } while (0)

static int try_lib(const char* path) {
    g_api.dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (g_api.dl == NULL) {
        return 0;
    }
    SVL_SYM(init, "sv_init");
    SVL_SYM(deinit, "sv_deinit");
    SVL_SYM(open_slot, "sv_open_slot");
    SVL_SYM(close_slot, "sv_close_slot");
    SVL_SYM(load_from_memory, "sv_load_from_memory");
    SVL_SYM(play_from_beginning, "sv_play_from_beginning");
    SVL_SYM(set_autostop, "sv_set_autostop");
    SVL_SYM(end_of_song, "sv_end_of_song");
    SVL_SYM(audio_callback, "sv_audio_callback");
    SVL_SYM(get_song_length_frames, "sv_get_song_length_frames");
    SVL_SYM(get_ticks, "sv_get_ticks");
    SVL_SYM(rewind, "sv_rewind");
    SVL_SYM(get_song_name, "sv_get_song_name");
    return 1;
}

/* The library file next to this module: <module folder>/sunvox_lib/sunvox.dylib. */
static int load_api(void) {
    Dl_info di;
    const char* env;
    char path[PATH_MAX];
    char* slash;
    if (g_api.dl != NULL) {
        return 1;
    }
    if (g_api_tried) {
        return 0;
    }
    g_api_tried = 1;
    env = getenv("SELECTA_SUNVOX_LIB");
    if (env != NULL && env[0] != '\0' && try_lib(env)) {
        return 1;
    }
    memset(&di, 0, sizeof di);
    if (dladdr((void*)&load_api, &di) == 0 || di.dli_fname == NULL || strlen(di.dli_fname) >= sizeof path - 32u) {
        return 0;
    }
    memcpy(path, di.dli_fname, strlen(di.dli_fname) + 1u);
    slash = strrchr(path, '/');
    if (slash == NULL) {
        return 0;
    }
    snprintf(slash + 1, sizeof path - (size_t)(slash + 1 - path), "sunvox_lib/%s", SVL_LIB_NAME);
    return try_lib(path);
}

typedef struct svl_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    uint32_t rate;
    uint64_t frames;
    uint64_t total;
    int live; /* the library holds this song */
    int ended;
    char title[40];
    sel_tag meta[3];
} svl_state;

static void release_engine(svl_state* s) {
    if (s->live) {
        g_api.close_slot(0);
        g_api.deinit();
        s->live = 0;
        g_busy = 0;
    }
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size < 16u || memcmp(data, "SVOX", 4) != 0 || !load_api()) {
        return SEL_PROBE_NO;
    }
    return SEL_PROBE_YES;
}

static void rp_close(void* state) {
    svl_state* s = (svl_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        release_engine(s);
        host->mem_free(host, s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    svl_state* s;
    if (src->stream != NULL || src->data == NULL || src->size > 0x7FFFFFFFu || src->size < 16u ||
        memcmp(src->data, "SVOX", 4) != 0 || !load_api()) {
        return NULL;
    }
    s = (svl_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = (uint8_t*)host->mem_alloc(host, src->size, SEL_MEM_FAST);
    if (s->data == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    memcpy(s->data, src->data, src->size);
    s->size = src->size;
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int restart(svl_state* s) {
    const char* t;
    size_t i, n = 0;
    release_engine(s);
    s->frames = 0;
    s->ended = 0;
    s->title[0] = '\0';
    if (g_busy) {
        return 0;
    }
    if (g_api.init(NULL, (int)s->rate, 2, SV_INIT_FLAG_NO_DEBUG_OUTPUT | SV_INIT_FLAG_OFFLINE | SV_INIT_FLAG_AUDIO_INT16 |
                                              SV_INIT_FLAG_ONE_THREAD) < 0) {
        return 0;
    }
    g_busy = 1;
    s->live = 1;
    if (g_api.open_slot(0) < 0 || g_api.load_from_memory(0, s->data, (uint32_t)s->size) < 0) {
        release_engine(s);
        return 0;
    }
    g_api.set_autostop(0, 1);
    g_api.play_from_beginning(0);
    s->total = g_api.get_song_length_frames(0);
    t = g_api.get_song_name(0);
    for (i = 0; t != NULL && t[i] != '\0' && n < sizeof s->title - 1u; i++) {
        if ((unsigned char)t[i] >= 0x20u && (unsigned char)t[i] <= 0x7Eu) {
            s->title[n++] = t[i];
        }
    }
    s->title[n] = '\0';
    return 1;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    svl_state* s = (svl_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    return restart(s);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    svl_state* s = (svl_state*)state;
    uint64_t left;
    uint32_t n;
    if (!s->live || s->ended) {
        return 0;
    }
    left = s->total > s->frames ? s->total - s->frames : 0u;
    n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        g_api.audio_callback(out, (int)n, 0, g_api.get_ticks());
    }
    s->frames += n;
    if (n < frames || s->frames >= (uint64_t)s->rate * SVL_MAX_SECONDS || g_api.end_of_song(0)) {
        s->ended = 1;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    svl_state* s = (svl_state*)state;
    int16_t scratch[2048 * 2];
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    if (!s->live || !restart(s)) {
        return 0;
    }
    /* Restart and mix forward, discarding the output: exact for every song, and a seek is rare. */
    while (left > 0u && !s->ended) {
        uint32_t want = left > 2048u ? 2048u : (uint32_t)left;
        uint32_t got = rp_render(s, scratch, want);
        if (got == 0u) {
            break;
        }
        left -= got;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    svl_state* s = (svl_state*)state;
    size_t k = 0;
    s->meta[k].tag = SEL_META_FORMAT;
    s->meta[k++].value = (uintptr_t) "SunVox";
    if (s->title[0] != '\0') {
        s->meta[k].tag = SEL_META_TITLE;
        s->meta[k++].value = (uintptr_t)s->title;
    }
    s->meta[k].tag = SEL_TAG_END;
    s->meta[k].value = 0;
    return s->meta;
}

static sel_tag g_hooks[14];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "sunvox_lib");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sunvox");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_PRIORITY, 20);
    set(&g_hooks[12], SEL_RP_NEEDS_FLOAT, 1);
    set(&g_hooks[13], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "sunvox_lib");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
