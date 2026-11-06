/* bbmp cybertracker replayer (row I710g): a RENDER-kind adapter over bbmp's csrc/retro cybertracker engine (third_party/bbmp_cybertracker,
 * GPL-3.0-or-later, clean-room), which runs the user's own CyberTracker 1.01 disk image on an emulated C64. The selected track is rendered
 * at RP_START through the engine's open()/render() into one buffer that rp_render copies out. The song starts with the 10-byte magic
 * 00 04 "NNTRKMZX"; without the disk image (SELECTA_CYBERTRACKER_DISK, BBMP_CYBERTRACKER_DISK or <config dir>/cybertracker101.d64) open
 * fails and the file is not claimed. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int bbcybertracker_render(const uint8_t* data, size_t size, uint32_t rate, int track, int check_only, int16_t** pcm, uint64_t* frames,
                          long* duration_ms, int* tracks, char* err, size_t errn);

typedef struct bbct_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    int tracks;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[3];
} bbct_state;

static int ext_is_ct(const char* name) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 4u && name[nl - 3u] == '.' && (name[nl - 2u] | 0x20) == 'c' && (name[nl - 1u] | 0x20) == 't';
}

static int has_magic(const uint8_t* data, size_t size) {
    return size >= 13u && data[0] == 0x00u && data[1] == 0x04u && memcmp(data + 2, "NNTRKMZX", 8) == 0;
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    return ext_is_ct(name) && has_magic(data, size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbct_state* s = (bbct_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        free(s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbct_state* s;
    char err[256];
    int tracks = 0;
    if (src->stream != NULL || src->data == NULL || !has_magic(src->data, src->size) ||
        !bbcybertracker_render(src->data, src->size, 44100u, 0, 1, NULL, NULL, NULL, &tracks, err, sizeof err)) {
        return NULL;
    }
    s = (bbct_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = (uint8_t*)malloc(src->size);
    if (s->data == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    memcpy(s->data, src->data, src->size);
    s->size = src->size;
    s->tracks = tracks > 0 ? tracks : 1;
    info->voice_count = 0;
    info->subsong_count = (uint32_t)s->tracks;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbct_state* s = (bbct_state*)state;
    char err[256];
    long dur = 0;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong >= (uint32_t)s->tracks) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->frames = s->pos = 0;
    s->rate = rate;
    if (!bbcybertracker_render(s->data, s->size, rate, (int)subsong, 0, &s->pcm, &s->frames, &dur, NULL, err, sizeof err) ||
        s->frames == 0u) {
        free(s->pcm);
        s->pcm = NULL;
        s->frames = 0;
        return 0;
    }
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbct_state* s = (bbct_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbct_state* s = (bbct_state*)state;
    if (s->pcm == NULL) {
        return 0;
    }
    s->pos = (uint64_t)ms * s->rate / 1000u;
    if (s->pos > s->frames) {
        s->pos = s->frames;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    bbct_state* s = (bbct_state*)state;
    uint32_t n = 0;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "CyberTracker C64";
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_cybertracker");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "ct");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_PRIORITY, 10);
    set(&g_hooks[12], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_cybertracker");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
