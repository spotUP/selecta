/* EUPPlayer replayer (row I701): a RENDER-kind adapter over eupmini (third_party/eupmini, GPL-2.0) for FM Towns Euphony
 * songs (.eup). The Towns FM/PCM device is emulated at the host rate by the library (shim.cpp is the C ABI). A song plays
 * until the player reports its end; the safety stop is 20 minutes. Companion banks (I1127): the header names an FM voice
 * bank (.FMB, offset 0x6E2) and a PCM set (.PMB, offset 0x6EA), 8 characters each, kept next to the song. Like the
 * other render adapters (mdxmini PDX, kss) the adapter reads them from the directory of src->path: for a local song
 * they are just there. A cached remote song still lacks them: the generic SEL_RP_COMPANIONS rules only see the song's
 * file name, the bank names live in the song bytes (open, row I1127). A missing bank leaves FM parts on the default voice and PCM parts silent. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct eupshim;
struct eupshim* eupshim_open(const uint8_t* song, uint32_t size, uint32_t rate, const uint8_t* fmb, uint32_t fmb_size,
                             const uint8_t* pmb, uint32_t pmb_size);
uint32_t eupshim_render(struct eupshim* s, int16_t* out, uint32_t frames);
void eupshim_close(struct eupshim* s);

#define EUP_HEADER 2048u
#define EUP_MAX_SECONDS 1200u

typedef struct eup_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    uint8_t* fmb;
    uint32_t fmb_size;
    uint8_t* pmb;
    uint32_t pmb_size;
    struct eupshim* shim;
    uint32_t rate;
    uint64_t frames;
    int ended;
    sel_tag meta[2];
} eup_state;

static int ext_is_eup(const char* name) {
    const char* dot;
    if (name == NULL || (dot = strrchr(name, '.')) == NULL || strlen(dot) != 4u) {
        return 0;
    }
    return (dot[1] | 0x20) == 'e' && (dot[2] | 0x20) == 'u' && (dot[3] | 0x20) == 'p';
}

/* .eup has no magic: the extension and a header-sized file are the test. */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)data;
    return (ext_is_eup(name) && size >= EUP_HEADER + 6u) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    eup_state* s = (eup_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        eupshim_close(s->shim);
        host->mem_free(host, s->data);
        host->mem_free(host, s->fmb);
        host->mem_free(host, s->pmb);
        host->mem_free(host, s);
    }
}

/* Reads `<dir of path><8-char header name><ext>` (name trimmed at NUL or space). Tries the name as written, then lower and
 * upper case, because the bank file's case on disk is the archive's, not the header's. Returns the host buffer or NULL. */
static uint8_t* read_bank(const sel_host* host, const char* path, const uint8_t* hdr_name, const char* ext, uint32_t* size) {
    char name[9];
    char full[1100];
    size_t dir = 0, n = 0, pass;
    const char* slash = path != NULL ? strrchr(path, '/') : NULL;
    *size = 0;
    if (slash != NULL) {
        dir = (size_t)(slash - path) + 1u;
    }
    while (n < 8u && hdr_name[n] != 0u && hdr_name[n] != ' ' && hdr_name[n] > 0x20u && hdr_name[n] < 0x7Fu && hdr_name[n] != '/') {
        name[n] = (char)hdr_name[n];
        n++;
    }
    name[n] = '\0';
    if (n == 0u || dir + n + 5u > sizeof full) {
        return NULL;
    }
    for (pass = 0; pass < 3u; pass++) {
        size_t i;
        FILE* f;
        long len;
        uint8_t* buf;
        memcpy(full, path, dir);
        for (i = 0; i < n; i++) {
            char c = name[i];
            full[dir + i] = pass == 1u ? (char)((c >= 'A' && c <= 'Z') ? c | 0x20 : c) : pass == 2u ? (char)((c >= 'a' && c <= 'z') ? c & ~0x20 : c) : c;
        }
        for (i = 0; ext[i] != '\0'; i++) {
            full[dir + n + i] = pass == 2u ? (char)(ext[i] & ~0x20) : ext[i];
        }
        full[dir + n + i] = '\0';
        f = fopen(full, "rb");
        if (f == NULL) {
            continue;
        }
        len = (fseek(f, 0, SEEK_END) == 0) ? ftell(f) : -1;
        buf = (len > 0 && len <= 0x1000000L && fseek(f, 0, SEEK_SET) == 0) ? (uint8_t*)host->mem_alloc(host, (size_t)len, SEL_MEM_FAST) : NULL;
        if (buf != NULL && fread(buf, 1, (size_t)len, f) != (size_t)len) {
            host->mem_free(host, buf);
            buf = NULL;
        }
        fclose(f);
        if (buf != NULL) {
            *size = (uint32_t)len;
            return buf;
        }
    }
    return NULL;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    eup_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < EUP_HEADER + 6u) {
        return NULL;
    }
    s = (eup_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
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
    if (src->path != NULL) {
        s->fmb = read_bank(host, src->path, s->data + 0x6E2, ".fmb", &s->fmb_size);
        s->pmb = read_bank(host, src->path, s->data + 0x6EA, ".pmb", &s->pmb_size);
    }
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int restart(eup_state* s) {
    eupshim_close(s->shim);
    s->shim = eupshim_open(s->data, (uint32_t)s->size, s->rate, s->fmb, s->fmb_size, s->pmb, s->pmb_size);
    s->frames = 0;
    s->ended = 0;
    return s->shim != NULL;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    eup_state* s = (eup_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    return restart(s);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    eup_state* s = (eup_state*)state;
    uint32_t n;
    if (s->shim == NULL || s->ended) {
        return 0;
    }
    n = eupshim_render(s->shim, out, frames);
    s->frames += n;
    if (n < frames || s->frames >= (uint64_t)s->rate * EUP_MAX_SECONDS) {
        s->ended = 1;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    eup_state* s = (eup_state*)state;
    int16_t scratch[2048 * 2];
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    if (s->shim == NULL || !restart(s)) {
        return 0;
    }
    /* No random access: restart the song and run the emulation forward, discarding the output. */
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
    eup_state* s = (eup_state*)state;
    s->meta[0].tag = SEL_META_FORMAT;
    s->meta[0].value = (uintptr_t) "Euphony";
    s->meta[1].tag = SEL_TAG_END;
    s->meta[1].value = 0;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "eupmini");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "eup");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "eupmini");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
