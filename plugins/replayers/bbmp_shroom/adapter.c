/* bbmp shroom replayer (row I710aa): a RENDER-kind adapter over bbmp's csrc/retro shroom engine (third_party/bbmp_shroom, GPL-3.0-or-later),
 * which plays Mario Paint music editor songs (.sho, magic "shro"). The instrument sounds are Nintendo data and are never shipped or downloaded:
 * the user supplies Mariopants' notes.wav as the declared system file mariopants-notes.wav (RP_SYSFILE, I710ga, no URLs). Without it the host
 * does not call open (system_file_missing). The glue converts the wav to the engine's note table at load time. The song is rendered at
 * RP_START into one buffer that rp_render copies out. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int bbshroom_render(const uint8_t* data, size_t size, uint32_t rate, int check_only, int16_t** pcm, uint64_t* frames, long* duration_ms,
                    char* err, size_t errn, const char* notes_path);

typedef struct bbsh_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    int tracks;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    char disk[1024]; /* config_get sysfile path, "" when the host gave none */
    sel_tag meta[3];
} bbsh_state;

static int ext_is_sho(const char* name) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 5u && name[nl - 4u] == '.' && (name[nl - 3u] | 0x20) == 's' && (name[nl - 2u] | 0x20) == 'h' && (name[nl - 1u] | 0x20) == 'o';
}

static int has_magic(const uint8_t* data, size_t size) {
    return size >= 7u && memcmp(data, "shro", 4) == 0;
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    return ext_is_sho(name) && has_magic(data, size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbsh_state* s = (bbsh_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        free(s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbsh_state* s;
    char err[256];
    const char* disk = host->config_get != NULL ? host->config_get(host, "sysfile.mariopants-notes.wav", NULL) : NULL;
    if (src->stream != NULL || src->data == NULL || !has_magic(src->data, src->size) ||
        !bbshroom_render(src->data, src->size, 44100u, 1, NULL, NULL, NULL, err, sizeof err, disk)) {
        return NULL;
    }
    s = (bbsh_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    if (disk != NULL && strlen(disk) < sizeof s->disk) {
        strcpy(s->disk, disk);
    }
    s->data = (uint8_t*)malloc(src->size);
    if (s->data == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    memcpy(s->data, src->data, src->size);
    s->size = src->size;
    s->tracks = 1;
    info->voice_count = 0;
    info->subsong_count = (uint32_t)s->tracks;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbsh_state* s = (bbsh_state*)state;
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
    if (!bbshroom_render(s->data, s->size, rate, 0, &s->pcm, &s->frames, &dur, err, sizeof err, s->disk[0] != '\0' ? s->disk : NULL) ||
        s->frames == 0u) {
        free(s->pcm);
        s->pcm = NULL;
        s->frames = 0;
        return 0;
    }
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbsh_state* s = (bbsh_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbsh_state* s = (bbsh_state*)state;
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
    bbsh_state* s = (bbsh_state*)state;
    uint32_t n = 0;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "Shroom (Mario Paint)";
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

/* User-supplied: Nintendo's sounds, so no URLS (the host never fetches). SHA256 = Mariopants 1.1 data/notes.wav (26936466 bytes), see
 * third_party/bbmp_shroom/VERSION.txt. */
static const sel_tag g_sysfile[] = {
    {SEL_RP_SYSFILE_NAME, (uintptr_t) "mariopants-notes.wav"},
    {SEL_RP_SYSFILE_LABEL, (uintptr_t) "Mario Paint instrument recordings"},
    {SEL_RP_SYSFILE_HINT, (uintptr_t) "Pick data/notes.wav of Mariopants 1.1 by Brad Smith (rainwarrior.ca): the Mario Paint instruments, 32000 Hz mono."},
    {SEL_RP_SYSFILE_SHA256, (uintptr_t) "039f642ea239970f41ed6fe3431f27c156346312ee72184723611951b855b129"},
    {SEL_TAG_END, 0}};
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_shroom");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sho");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_PRIORITY, 10);
    set(&g_hooks[12], SEL_RP_SYSFILE, (uintptr_t)g_sysfile);
    set(&g_hooks[13], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_shroom");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
