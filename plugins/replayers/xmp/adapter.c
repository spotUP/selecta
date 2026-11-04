/* libxmp replayer (design doc "Format coverage rule", row I697): a RENDER-kind adapter over libxmp, the engine bbmp
 * uses for its xmp backend. Claims only the tracker formats that neither libopenmpt nor UADE/nuade play: Liquid Tracker,
 * Megatracker, Archimedes Tracker, Coconizer and Fuchs Tracker. The render stops when the order list ends; loops are
 * not followed. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <xmp.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define XMP_TEXT 128u
#define XMP_CHUNK_FRAMES 256u
#define XMP_EXTENSIONS "liq,mgt,musx,coc,fuchs"

typedef struct xmp_state {
    const sel_host* host;
    xmp_context ctx;
    int started;
    uint32_t rate;
    char title[XMP_TEXT], format[XMP_TEXT];
    sel_tag meta[4];
} xmp_state;

/* The loaders this plugin stands for, named as libxmp reports them. */
static int claimed_type(const char* type) {
    return strstr(type, "Liquid") != NULL || strstr(type, "Megatracker") != NULL ||
           strstr(type, "Archimedes") != NULL || strstr(type, "Coconizer") != NULL || strstr(type, "Fuchs") != NULL;
}

/* Content probe through libxmp's own loader tests. ProWizard-packed files report the generic "prowizard" type; the
 * open decides whether one is Fuchs Tracker. Everything else must name one of the claimed loaders. */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    struct xmp_test_info ti;
    (void)name;
    if (size == 0u || size > 0x7fffffffu) {
        return SEL_PROBE_NO;
    }
    memset(&ti, 0, sizeof ti);
    if (xmp_test_module_from_memory(data, (long)size, &ti) != 0) {
        return SEL_PROBE_NO;
    }
    if (claimed_type(ti.type)) {
        return SEL_PROBE_YES;
    }
    return strcmp(ti.type, "prowizard") == 0 ? SEL_PROBE_UNSURE : SEL_PROBE_NO;
}

static void copy_text(char* dst, const char* src) {
    strncpy(dst, src != NULL ? src : "", XMP_TEXT - 1u);
    dst[XMP_TEXT - 1u] = '\0';
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    xmp_state* s;
    struct xmp_module_info mi;
    if (src->stream != NULL || src->data == NULL || src->size > 0x7fffffffu) {
        return NULL;
    }
    s = (xmp_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->ctx = xmp_create_context();
    if (s->ctx == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    if (xmp_load_module_from_memory(s->ctx, src->data, (long)src->size) != 0) {
        xmp_free_context(s->ctx);
        host->mem_free(host, s);
        return NULL;
    }
    xmp_get_module_info(s->ctx, &mi);
    if (!claimed_type(mi.mod->type)) {
        xmp_release_module(s->ctx);
        xmp_free_context(s->ctx);
        host->mem_free(host, s);
        return NULL;
    }
    copy_text(s->title, mi.mod->name);
    copy_text(s->format, mi.mod->type);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    xmp_state* s = (xmp_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        if (s->started) {
            xmp_end_player(s->ctx);
        }
        xmp_release_module(s->ctx);
        xmp_free_context(s->ctx);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    xmp_state* s = (xmp_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    if (s->started) {
        xmp_end_player(s->ctx);
        s->started = 0;
    }
    if (xmp_start_player(s->ctx, (int)rate, 0) != 0) {
        return 0;
    }
    s->started = 1;
    s->rate = rate;
    return 1;
}

/* Stereo 16-bit frames. The chunk the song ends inside is dropped: at most XMP_CHUNK_FRAMES frames. */
static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    xmp_state* s = (xmp_state*)state;
    uint32_t done = 0;
    while (done < frames) {
        uint32_t n = frames - done < XMP_CHUNK_FRAMES ? frames - done : XMP_CHUNK_FRAMES;
        if (xmp_play_buffer(s->ctx, out + 2u * done, (int)(n * 4u), 1) != 0) {
            break;
        }
        done += n;
    }
    return done;
}

static int rp_seek(void* state, uint32_t ms) {
    xmp_state* s = (xmp_state*)state;
    return xmp_seek_time(s->ctx, (int)ms) >= 0;
}

static const sel_tag* rp_metadata(void* state) {
    xmp_state* s = (xmp_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)s->format;
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "xmp");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) XMP_EXTENSIONS);
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_FORMATS, (uintptr_t) XMP_EXTENSIONS);
    set(&g_hooks[12], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "xmp");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
