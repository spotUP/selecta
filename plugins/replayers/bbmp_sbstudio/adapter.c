/* bbmp sbstudio replayer (row I710v): a RENDER-kind adapter over libpac 0.9.0 (third_party/libpac, ISC) for SBStudio songs (.pac).
 * libpac keeps its state in globals (one open song at a time) and mixes at any rate from 4000 to 48000 Hz, 16 bit stereo, so the
 * song streams straight from pac_read at the host rate; above 48000 Hz it renders at 48000 and the adapter interpolates linearly.
 * pac_open reads the file by path, so the song's path is kept. Songs written for MIDI instruments alone (no samples) are refused. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pacP.h"

enum { kMaxRate = 48000, kChunk = 2048 };

typedef struct bbsbs_state {
    const sel_host* host;
    char path[1024];
    struct pac_module* mod;
    int inited;
    uint32_t rate;
    uint32_t src_rate;
    uint64_t pos; /* output frames delivered */
    double phase; /* source position of the next output frame, relative to src0 */
    int16_t src0[2], src1[2];
    int have;
    long duration_ms;
    char title[PAC_NAME_MAX + 1];
    sel_tag meta[4];
} bbsbs_state;

static int ext_is_pac(const char* name) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 5u && name[nl - 4u] == '.' && (name[nl - 3u] | 0x20) == 'p' && (name[nl - 2u] | 0x20) == 'a' &&
           (name[nl - 1u] | 0x20) == 'c';
}

/* Content probe: the file starts with "PACG" (the extension alone is not enough, ".pac" is used by other formats). */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return size >= 16u && memcmp(data, "PACG", 4) == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void drop_module(bbsbs_state* s) {
    if (s->mod != NULL) {
        pac_close(s->mod);
        s->mod = NULL;
    }
    if (s->inited) {
        pac_exit();
        s->inited = 0;
    }
}

static int load_module(bbsbs_state* s, uint32_t rate) {
    int i, sounds = 0;
    drop_module(s);
    if (pac_init((long)rate, 16, 2) != 0) {
        return 0;
    }
    s->inited = 1;
    pac_enable(PAC_VOLUME_REDUCTION);
    s->mod = pac_open(s->path);
    if (s->mod == NULL) {
        drop_module(s);
        return 0;
    }
    for (i = 0; i < s->mod->soundcnt; i++) {
        if (s->mod->sound[i].sample != NULL && s->mod->sound[i].length > 0) {
            sounds++;
        }
    }
    if (sounds == 0) {
        drop_module(s);
        return 0;
    }
    return 1;
}

static void rp_close(void* state) {
    bbsbs_state* s = (bbsbs_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        drop_module(s);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbsbs_state* s;
    size_t n;
    if (src->stream != NULL || src->data == NULL || src->path == NULL || strlen(src->path) >= sizeof s->path || src->size < 16u ||
        !rp_probe(src->data, src->size, src->path) || !ext_is_pac(src->path)) {
        return NULL;
    }
    s = (bbsbs_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    strcpy(s->path, src->path);
    if (!load_module(s, 44100u)) {
        host->mem_free(host, s);
        return NULL;
    }
    s->duration_ms = pac_length(s->mod) > 0 ? (long)((double)pac_length(s->mod) * 1000.0 / 44100.0) : 0;
    for (n = 0; n < PAC_NAME_MAX && s->mod->name[n] != '\0'; n++) {
        s->title[n] = s->mod->name[n];
    }
    while (n > 0u && (s->title[n - 1u] == ' ' || s->title[n - 1u] == '\t')) {
        n--;
    }
    s->title[n] = '\0';
    drop_module(s);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbsbs_state* s = (bbsbs_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    s->src_rate = rate > (uint32_t)kMaxRate ? (uint32_t)kMaxRate : rate;
    if (!load_module(s, s->src_rate)) {
        return 0;
    }
    s->pos = 0;
    s->phase = 0.0;
    s->have = 0;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbsbs_state* s = (bbsbs_state*)state;
    uint32_t done = 0;
    if (s->mod == NULL) {
        return 0;
    }
    if (s->rate == s->src_rate) {
        while (done < frames) {
            uint32_t want = frames - done;
            long got;
            if (want > (uint32_t)kChunk) {
                want = (uint32_t)kChunk;
            }
            got = pac_read(s->mod, out + (size_t)done * 2u, (long)want * 4) / 4;
            if (got <= 0) {
                break;
            }
            done += (uint32_t)got;
        }
    } else {
        const double step = (double)s->src_rate / (double)s->rate;
        while (done < frames) {
            int c;
            while (s->have < 2 || s->phase >= 1.0) {
                int16_t f[2];
                long got = pac_read(s->mod, f, 4L) / 4;
                if (got <= 0) {
                    return done;
                }
                s->src0[0] = s->src1[0];
                s->src0[1] = s->src1[1];
                s->src1[0] = f[0];
                s->src1[1] = f[1];
                if (s->have < 2) {
                    s->have++;
                } else {
                    s->phase -= 1.0;
                }
            }
            for (c = 0; c < 2; c++) {
                out[(size_t)done * 2u + (size_t)c] =
                    (int16_t)((double)s->src0[c] * (1.0 - s->phase) + (double)s->src1[c] * s->phase);
            }
            s->phase += step;
            done++;
        }
    }
    s->pos += done;
    return done;
}

static int rp_seek(void* state, uint32_t ms) {
    bbsbs_state* s = (bbsbs_state*)state;
    long frame;
    if (s->mod == NULL) {
        return 0;
    }
    frame = (long)((uint64_t)ms * s->src_rate / 1000u);
    if (frame > pac_length(s->mod)) {
        frame = pac_length(s->mod);
    }
    if (pac_seek(s->mod, frame, SEEK_SET) < 0) {
        return 0;
    }
    s->have = 0;
    s->phase = 0.0;
    s->pos = (uint64_t)ms * s->rate / 1000u;
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    bbsbs_state* s = (bbsbs_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "SBStudio";
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)s->duration_ms;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_sbstudio");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "pac");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_sbstudio");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
