/* bbmp piece replayer (row I710s): a RENDER-kind adapter over bbmp's csrc/piece/piece.cpp (third_party/bbmp_piece,
 * GPL-3.0-or-later), Aquaplus P/ECE music driver songs (.pmd). The extension is shared with P.M.D. (PC-98, ext .m/.m2/.mz in
 * 98fmplayer, which never claims .pmd), so the probe needs the extension AND the song's structure: byte 0 is 0, byte 1 the part
 * count 1..16, the part, drum, title and comment offsets lie inside the file past the header and the title ends in a NUL.
 * The song is rendered whole at RP_START. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct pcsong;
struct pcsong* pc_open(const uint8_t* data, uint32_t size);
uint32_t pc_duration_ms(const struct pcsong* f);
const char* pc_title(const struct pcsong* f);
int pc_render(struct pcsong* f, uint32_t rate, int16_t** pcm, uint64_t* frames);
void pc_close(struct pcsong* f);

typedef struct bbpc_state {
    const sel_host* host;
    struct pcsong* song;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[5];
} bbpc_state;

static unsigned rd16(const uint8_t* d, size_t o) {
    return (unsigned)d[o] | ((unsigned)d[o + 1] << 8);
}

static int ext_is_pmd(const char* name) {
    size_t n = name != NULL ? strlen(name) : 0u;
    return n >= 4u && name[n - 4] == '.' && (name[n - 3] | 0x20) == 'p' && (name[n - 2] | 0x20) == 'm' && (name[n - 1] | 0x20) == 'd';
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    unsigned np, i, hdr, tp, parts = 0;
    if (!ext_is_pmd(name) || size < 20u || size > 0xFFFFu || data[0] != 0u || data[1] < 1u || data[1] > 16u) {
        return SEL_PROBE_NO;
    }
    np = data[1];
    hdr = 2u + 2u * (np + 3u);
    if (hdr > size) {
        return SEL_PROBE_NO;
    }
    for (i = 0; i < np; i++) {
        unsigned o = rd16(data, 2u + 2u * i);
        if (o != 0u && (o < hdr || o >= size)) {
            return SEL_PROBE_NO;
        }
        parts += o != 0u;
    }
    tp = rd16(data, 2u + 2u * np + 2u);
    if (parts == 0u || tp < hdr || tp >= size || memchr(data + tp, 0, size - tp) == NULL) {
        return SEL_PROBE_NO;
    }
    return SEL_PROBE_YES;
}

static void rp_close(void* state) {
    bbpc_state* s = (bbpc_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        pc_close(s->song);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbpc_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < 20u) {
        return NULL;
    }
    s = (bbpc_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->song = pc_open(src->data, (uint32_t)src->size);
    if (s->song == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbpc_state* s = (bbpc_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    return pc_render(s->song, rate, &s->pcm, &s->frames);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbpc_state* s = (bbpc_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbpc_state* s = (bbpc_state*)state;
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
    bbpc_state* s = (bbpc_state*)state;
    uint32_t n = 0;
    const char* title = pc_title(s->song);
    if (title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)title;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "P/ECE music driver";
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)pc_duration_ms(s->song);
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_piece");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "pmd");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_piece");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
