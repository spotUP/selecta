/* SNDH replayer (row I427b, design 2026-10-30-plugin-loader-sdk D5): a RENDER-kind adapter over sc68 (third_party/sc68,
 * GPL-3) for Atari ST SNDH chip music, ICE!-packed or plain, and sc68 files. Built only as a MODULE under
 * SELECTA_GPL_PLUGINS, so the MIT core never links it. It includes include/selecta and sc68's public header only.
 * sc68 emulates the 68000, YM2149, MFP and STE hardware and renders stereo int16 at the rate it was created with.
 * Library init is global and happens once, on the first open; it never reads or writes a config file. */
/* Vendored header declares sc68_msg_t with an empty parameter list; not ours to change. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <sc68/sc68.h>
#pragma GCC diagnostic pop

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

#define SNDH_TEXT 256u
#define SNDH_CHUNK 1024u

typedef struct sndh_state {
    const sel_host* host;
    sc68_t* sc;
    const uint8_t* data;
    size_t size;
    uint32_t rate;
    uint32_t length_ms;
    uint32_t tracks;
    char title[SNDH_TEXT], author[SNDH_TEXT], format[SNDH_TEXT];
    sel_tag meta[6];
} sndh_state;

static int g_lib_ready;

static int has_ext(const char* name, const char* ext) {
    size_t n, e = strlen(ext);
    if (name == NULL) {
        return 0;
    }
    n = strlen(name);
    if (n < e + 1u || name[n - e - 1u] != '.') {
        return 0;
    }
    for (n -= e; *ext != '\0'; ext++, n++) {
        char c = name[n];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != *ext) {
            return 0;
        }
    }
    return 1;
}

/* Plain SNDH carries "SNDH" in its header; the packed form starts with "ICE!" and is only claimed by extension. */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    size_t i;
    if (size < 16u) {
        return SEL_PROBE_NO;
    }
    if (memcmp(data, "SC68 Music-file", 15) == 0) {
        return SEL_PROBE_YES;
    }
    if (memcmp(data, "ICE!", 4) == 0) {
        return has_ext(name, "sndh") || has_ext(name, "sc68") ? SEL_PROBE_YES : SEL_PROBE_NO;
    }
    for (i = 0; i + 4u <= size && i < 64u; i++) {
        if (memcmp(data + i, "SNDH", 4) == 0) {
            return SEL_PROBE_YES;
        }
    }
    return SEL_PROBE_NO;
}

static void copy_text(char* dst, const char* src) {
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, SNDH_TEXT - 1u);
    dst[SNDH_TEXT - 1u] = '\0';
}

static int lib_init(void) {
    sc68_init_t init;
    if (g_lib_ready) {
        return 1;
    }
    memset(&init, 0, sizeof init);
    init.flags.no_load_config = 1;
    init.flags.no_save_config = 1;
    if (sc68_init(&init) != 0) {
        return 0;
    }
    g_lib_ready = 1;
    return 1;
}

/* Creates an instance at `rate` and loads the file; NULL when sc68 refuses it. */
static sc68_t* load_music(sndh_state* s, uint32_t rate) {
    sc68_create_t cr;
    sc68_music_info_t mi;
    sc68_t* sc;
    memset(&cr, 0, sizeof cr);
    cr.sampling_rate = rate;
    cr.name = "selecta";
    sc = sc68_create(&cr);
    if (sc == NULL) {
        return NULL;
    }
    if (sc68_load_mem(sc, s->data, (int)s->size) != 0) {
        sc68_destroy(sc);
        return NULL;
    }
    memset(&mi, 0, sizeof mi);
    if (sc68_music_info(sc, &mi, SC68_DSK_TRACK, 0) == 0) {
        s->tracks = mi.tracks > 0 ? (uint32_t)mi.tracks : 1u;
        s->length_ms = mi.dsk.time_ms;
        copy_text(s->title, mi.album);
        copy_text(s->author, mi.artist);
        copy_text(s->format, mi.format);
    }
    return sc;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    sndh_state* s;
    if (src->stream != NULL || src->data == NULL || !lib_init()) {
        return NULL;
    }
    s = (sndh_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    s->rate = 44100u;
    s->tracks = 1u;
    s->sc = load_music(s, s->rate);
    if (s->sc == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    info->voice_count = 0;
    info->subsong_count = s->tracks;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    sndh_state* s = (sndh_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        sc68_destroy(s->sc);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    sndh_state* s = (sndh_state*)state;
    (void)voices;
    if (rate == 0u || subsong >= s->tracks) {
        return 0;
    }
    /* sc68 fixes the rate at creation; every start rebuilds the instance from the same bytes. */
    sc68_destroy(s->sc);
    s->sc = load_music(s, rate);
    if (s->sc == NULL) {
        return 0;
    }
    s->rate = rate;
    return sc68_play(s->sc, (int)subsong + 1, SC68_INF_LOOP) >= 0;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    sndh_state* s = (sndh_state*)state;
    uint32_t done = 0;
    while (done < frames) {
        int n = (int)(frames - done < SNDH_CHUNK ? frames - done : SNDH_CHUNK);
        int code = sc68_process(s->sc, out + (size_t)done * 2u, &n);
        if (code == SC68_ERROR) {
            break;
        }
        done += (uint32_t)n;
        if ((code & SC68_END) != 0 || (n == 0 && (code & SC68_CHANGE) == 0)) {
            break;
        }
    }
    return done;
}

static const sel_tag* rp_metadata(void* state) {
    sndh_state* s = (sndh_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    if (s->author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->author;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)(s->format[0] != '\0' ? s->format : "SNDH");
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)s->length_ms;
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

static sel_tag g_hooks[11];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* sel_plugin_entry(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "sc68");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sndh,sc68");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[10], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "sc68");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
