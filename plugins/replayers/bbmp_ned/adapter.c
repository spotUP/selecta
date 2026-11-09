/* bbmp ned replayer (row I1223c, old I710o): a RENDER-kind adapter over bbmp's csrc/retro ned engine (third_party/bbmp_ned, GPL-3.0-or-later),
 * NerdTracker II songs (.ned, NES): the engine puts NT2's own 6502 replay routine, the song and its DPCM samples into an NSF image in memory
 * and has Game Music Emu (selecta_gme) play it. The replay routine (Bananmos' "replay 22", giftware, non-commercial use only) is never in the
 * repo or the pack: it is two declared system files (RP_SYSFILE, I710ga), nt2replay.bin and nt2replay.lbl, fetched from the hippoplayer.se
 * mirror. The host checks their sha256 and hands the stored paths through config_get "sysfile.nt2replay.bin" / "sysfile.nt2replay.lbl"; without
 * them the host does not call open (system_file_missing). The song is rendered whole at RP_START. The song starts with "NED: ". */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int bned_render(const uint8_t* data, size_t size, uint32_t rate, int check_only, int16_t** pcm, uint64_t* frames, long* duration_ms,
                char* title, size_t titlen, char* err, size_t errn, const char* bin_path, const char* lbl_path);

typedef struct bned_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    char bin[1024]; /* config_get sysfile paths, "" when the host gave none */
    char lbl[1024];
    char title[64];
    sel_tag meta[4];
} bned_state;

static int ext_is_ned(const char* name) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 5u && name[nl - 4u] == '.' && (name[nl - 3u] | 0x20) == 'n' && (name[nl - 2u] | 0x20) == 'e' && (name[nl - 1u] | 0x20) == 'd';
}

static int has_magic(const uint8_t* data, size_t size) {
    return size >= 68u && memcmp(data, "NED: ", 5) == 0;
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    return ext_is_ned(name) && has_magic(data, size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bned_state* s = (bned_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        free(s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bned_state* s;
    char err[256];
    char title[64];
    const char* bin = host->config_get != NULL ? host->config_get(host, "sysfile.nt2replay.bin", NULL) : NULL;
    const char* lbl = host->config_get != NULL ? host->config_get(host, "sysfile.nt2replay.lbl", NULL) : NULL;
    title[0] = '\0';
    if (src->stream != NULL || src->data == NULL || !has_magic(src->data, src->size) ||
        !bned_render(src->data, src->size, 44100u, 1, NULL, NULL, NULL, title, sizeof title, err, sizeof err, bin, lbl)) {
        return NULL;
    }
    s = (bned_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    if (bin != NULL && strlen(bin) < sizeof s->bin) {
        strcpy(s->bin, bin);
    }
    if (lbl != NULL && strlen(lbl) < sizeof s->lbl) {
        strcpy(s->lbl, lbl);
    }
    strcpy(s->title, title);
    s->data = (uint8_t*)malloc(src->size);
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

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bned_state* s = (bned_state*)state;
    char err[256];
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->frames = s->pos = 0;
    s->rate = rate;
    if (!bned_render(s->data, s->size, rate, 0, &s->pcm, &s->frames, NULL, NULL, 0, err, sizeof err, s->bin[0] != '\0' ? s->bin : NULL,
                     s->lbl[0] != '\0' ? s->lbl : NULL) ||
        s->frames == 0u) {
        free(s->pcm);
        s->pcm = NULL;
        s->frames = 0;
        return 0;
    }
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bned_state* s = (bned_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bned_state* s = (bned_state*)state;
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
    bned_state* s = (bned_state*)state;
    uint32_t n = 0;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "NerdTracker II";
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

/* NerdTracker II's replay routine, assembled from nt2-22.asm (nesdev.org/nt2/download/nedplay.zip) with cc65 2.19 as bbmp's build-retro.sh does
 * (recipe in third_party/bbmp_ned/VERSION.txt). Giftware: free for non-commercial use only; the hint says so. Mirror only (the vendor's
 * download is the source zip, not the assembled blob). Two entries, the host resolves both. */
static const sel_tag g_sysfile_bin[] = {
    {SEL_RP_SYSFILE_NAME, (uintptr_t) "nt2replay.bin"},
    {SEL_RP_SYSFILE_LABEL, (uintptr_t) "NerdTracker II replay routine"},
    {SEL_RP_SYSFILE_HINT, (uintptr_t) "NerdTracker II's NES replay routine by Bananmos (nesdev.org/nt2), assembled from nt2-22.asm. Giftware: free for non-commercial use only."},
    {SEL_RP_SYSFILE_SHA256, (uintptr_t) "8ac3e74346e8374bf9ad522bdae0a21cf1f248c254b563acf4ea363b63be0d21"},
    {SEL_RP_SYSFILE_SIZE, 3038},
    {SEL_RP_SYSFILE_URLS, (uintptr_t) "https://hippoplayer.se/sysfiles/nt2replay.bin"},
    {SEL_TAG_END, 0}};
static const sel_tag g_sysfile_lbl[] = {
    {SEL_RP_SYSFILE_NAME, (uintptr_t) "nt2replay.lbl"},
    {SEL_RP_SYSFILE_LABEL, (uintptr_t) "NerdTracker II replay labels"},
    {SEL_RP_SYSFILE_HINT, (uintptr_t) "The linker label file that goes with nt2replay.bin (same source, same terms: non-commercial use only)."},
    {SEL_RP_SYSFILE_SHA256, (uintptr_t) "daca7169d17c5390a01eeff155011dc68c6fa49692f5904b4b86f5c250b83fe7"},
    {SEL_RP_SYSFILE_SIZE, 3404},
    {SEL_RP_SYSFILE_URLS, (uintptr_t) "https://hippoplayer.se/sysfiles/nt2replay.lbl"},
    {SEL_TAG_END, 0}};
static sel_tag g_hooks[15];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_ned");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "ned");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_PRIORITY, 10);
    set(&g_hooks[12], SEL_RP_SYSFILE, (uintptr_t)g_sysfile_bin);
    set(&g_hooks[13], SEL_RP_SYSFILE, (uintptr_t)g_sysfile_lbl);
    set(&g_hooks[14], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_ned");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
