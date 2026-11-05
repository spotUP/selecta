/* QSF replayer (row I704, bbmp backend "Highly Quixotic"): a RENDER-kind adapter over the Highly Quixotic QSound core
 * (third_party/highly_quixotic, GPL-3.0; this plugin is a GPL module) and psflib (third_party/psflib, MIT). A .qsf song
 * is a PSF container (version 0x41) holding the Capcom CPS1/CPS2 sound board's Z80 program ("Z80" sections), its
 * QSound sample ROM ("SMP" sections) and the Kabuki key ("KEY"). A .miniqsf names its big ROM set in a `_lib` tag
 * (a .qsflib file); psflib opens it from the directory of the song, with the same stdio path the song itself was read
 * from, so the host's `src->path` is the one input (a song without a path is refused). The Z80 runs the game's sound
 * driver; the song is that driver started with the command bytes of the track. The core makes stereo 16-bit at 24038 Hz,
 * which the adapter resamples (linear) to the host rate. Length and fade come from the tags (default 3 minutes plus a
 * 10 second fade when the file has no length tag), the song ends after them. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "psflib.h"
#include "qsound.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define QSF_NATIVE_RATE 24038u
#define QSF_VERSION 0x41
#define QSF_NATIVE_CHUNK 256u
#define QSF_SKIP_CHUNK 1024u
#define QSF_DEFAULT_LENGTH_MS 180000u
#define QSF_DEFAULT_FADE_MS 10000u

typedef struct qsf_state {
    const sel_host* host;
    uint8_t* z80_rom;
    size_t z80_size;
    uint8_t* sample_rom;
    size_t sample_size;
    uint8_t key[11];
    int has_key;
    uint8_t* chip; /* core state, rebuilt at every start */
    uint32_t rate;
    uint32_t length_ms, fade_ms; /* from the tags; 0 = tag absent */
    int has_length;
    uint64_t frames_done, frames_total, fade_frames;
    /* resampler: two neighbouring native frames and the fractional position between them */
    int16_t cur[2], next[2];
    double frac, step;
    int16_t native[QSF_NATIVE_CHUNK * 2];
    uint32_t native_pos, native_have;
    char title[256], artist[256], game[256], format[8];
    int year;
    sel_tag meta[8];
} qsf_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size >= 4u && data[0] == 'P' && data[1] == 'S' && data[2] == 'F' && data[3] == QSF_VERSION) {
        return SEL_PROBE_YES;
    }
    return SEL_PROBE_NO;
}

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ---- psflib stdio callbacks ---- */
static void* io_fopen(void* ctx, const char* path) {
    (void)ctx;
    return fopen(path, "rb");
}
static size_t io_fread(void* buf, size_t size, size_t count, void* h) { return fread(buf, size, count, (FILE*)h); }
static int io_fseek(void* h, int64_t off, int whence) { return fseeko((FILE*)h, (off_t)off, whence) == 0 ? 0 : -1; }
static int io_fclose(void* h) { return fclose((FILE*)h); }
static long io_ftell(void* h) { return ftell((FILE*)h); }

/* Merge one section into a growing ROM image (sections of a _lib chain arrive deepest first, later ones overwrite). */
static int rom_put(uint8_t** rom, size_t* have, size_t offset, const uint8_t* src, size_t size) {
    size_t need = offset + size;
    if (need < offset || need > ((size_t)1 << 28)) {
        return -1;
    }
    if (*have < need) {
        uint8_t* grown = (uint8_t*)realloc(*rom, need);
        if (grown == NULL) {
            return -1;
        }
        memset(grown + *have, 0, need - *have);
        *rom = grown;
        *have = need;
    }
    memcpy(*rom + offset, src, size);
    return 0;
}

static int load_cb(void* ctx, const uint8_t* exe, size_t exe_size, const uint8_t* reserved, size_t reserved_size) {
    qsf_state* s = (qsf_state*)ctx;
    size_t pos = 0;
    (void)reserved;
    (void)reserved_size;
    while (pos + 11u <= exe_size) {
        const uint8_t* name = exe + pos;
        uint32_t offset = le32(exe + pos + 3), size = le32(exe + pos + 7);
        pos += 11u;
        if (size > exe_size - pos) {
            return -1;
        }
        if (memcmp(name, "KEY", 3) == 0) {
            if (size <= sizeof s->key) {
                memset(s->key, 0, sizeof s->key);
                memcpy(s->key, exe + pos, size);
                s->has_key = 1;
            }
        } else if (memcmp(name, "Z80", 3) == 0) {
            if (rom_put(&s->z80_rom, &s->z80_size, offset, exe + pos, size) != 0) {
                return -1;
            }
        } else if (memcmp(name, "SMP", 3) == 0) {
            if (rom_put(&s->sample_rom, &s->sample_size, offset, exe + pos, size) != 0) {
                return -1;
            }
        }
        pos += size;
    }
    return 0;
}

/* "mm:ss.fff", "ss.fff" or "h:mm:ss" to milliseconds. */
static uint32_t parse_time(const char* v) {
    double total = 0.0;
    const char* p = v;
    for (;;) {
        char* end;
        double part = strtod(p, &end);
        if (end == p) {
            break;
        }
        total = total * 60.0 + part;
        if (*end != ':') {
            break;
        }
        p = end + 1;
    }
    return total <= 0.0 ? 0u : (uint32_t)(total * 1000.0 + 0.5);
}

static void copy_tag(char* dst, size_t cap, const char* v) {
    size_t n = strlen(v);
    if (n >= cap) {
        n = cap - 1u;
    }
    memcpy(dst, v, n);
    dst[n] = '\0';
}

static int info_cb(void* ctx, const char* name, const char* value) {
    qsf_state* s = (qsf_state*)ctx;
    if (strcasecmp(name, "length") == 0) {
        s->length_ms = parse_time(value);
        s->has_length = 1;
    } else if (strcasecmp(name, "fade") == 0) {
        s->fade_ms = parse_time(value);
    } else if (strcasecmp(name, "title") == 0) {
        copy_tag(s->title, sizeof s->title, value);
    } else if (strcasecmp(name, "artist") == 0) {
        copy_tag(s->artist, sizeof s->artist, value);
    } else if (strcasecmp(name, "game") == 0) {
        copy_tag(s->game, sizeof s->game, value);
    } else if (strcasecmp(name, "year") == 0) {
        s->year = atoi(value);
    }
    return 0;
}

static void free_state(qsf_state* s) {
    const sel_host* host = s->host;
    free(s->z80_rom);
    free(s->sample_rom);
    free(s->chip);
    host->mem_free(host, s);
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    qsf_state* s;
    psf_file_callbacks cb;
    static int core_ready;
    if (src->stream != NULL || src->path == NULL) {
        return NULL;
    }
    if (!core_ready) {
        if (qsound_init() < 0) {
            return NULL;
        }
        core_ready = 1;
    }
    s = (qsf_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    cb.path_separators = "/\\";
    cb.context = NULL;
    cb.fopen = io_fopen;
    cb.fread = io_fread;
    cb.fseek = io_fseek;
    cb.fclose = io_fclose;
    cb.ftell = io_ftell;
    if (psf_load(src->path, &cb, QSF_VERSION, load_cb, s, info_cb, s, 0, NULL, NULL) <= 0 || s->z80_rom == NULL ||
        s->sample_rom == NULL) {
        free_state(s);
        return NULL;
    }
    if (!s->has_length) {
        s->length_ms = QSF_DEFAULT_LENGTH_MS;
        s->fade_ms = QSF_DEFAULT_FADE_MS;
    }
    s->chip = (uint8_t*)malloc(qsound_get_state_size());
    if (s->chip == NULL) {
        free_state(s);
        return NULL;
    }
    strcpy(s->format, "QSF");
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    if (state != NULL) {
        free_state((qsf_state*)state);
    }
}

static void pull_native(qsf_state* s, int16_t* out) {
    if (s->native_pos >= s->native_have) {
        uint32_t n = QSF_NATIVE_CHUNK;
        qsound_execute(s->chip, 0x7fffffff, s->native, &n);
        s->native_have = n;
        s->native_pos = 0;
        if (n == 0u) {
            out[0] = out[1] = 0;
            return;
        }
    }
    out[0] = s->native[s->native_pos * 2u];
    out[1] = s->native[s->native_pos * 2u + 1u];
    s->native_pos++;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    qsf_state* s = (qsf_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    qsound_clear_state(s->chip);
    if (s->has_key) {
        qsound_set_kabuki_key(s->chip, le32(s->key), le32(s->key + 4), (uint16_t)(s->key[8] | (s->key[9] << 8)),
                              s->key[10]);
    } else {
        qsound_set_kabuki_key(s->chip, 0, 0, 0, 0);
    }
    qsound_set_z80_rom(s->chip, s->z80_rom, (uint32_t)s->z80_size);
    qsound_set_sample_rom(s->chip, s->sample_rom, (uint32_t)s->sample_size);
    s->rate = rate;
    s->step = (double)QSF_NATIVE_RATE / (double)rate;
    s->native_pos = s->native_have = 0;
    pull_native(s, s->cur);
    pull_native(s, s->next);
    s->frac = 0.0;
    s->frames_done = 0;
    s->frames_total = ((uint64_t)s->length_ms + s->fade_ms) * rate / 1000u;
    s->fade_frames = (uint64_t)s->fade_ms * rate / 1000u;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    qsf_state* s = (qsf_state*)state;
    uint32_t i;
    if (s->frames_done >= s->frames_total) {
        return 0;
    }
    if ((uint64_t)frames > s->frames_total - s->frames_done) {
        frames = (uint32_t)(s->frames_total - s->frames_done);
    }
    for (i = 0; i < frames; i++) {
        int ch;
        double gain = 1.0;
        uint64_t pos = s->frames_done + i;
        if (s->fade_frames > 0u && pos + s->fade_frames > s->frames_total) {
            gain = (double)(s->frames_total - pos) / (double)s->fade_frames;
        }
        for (ch = 0; ch < 2; ch++) {
            double v = ((double)s->cur[ch] + s->frac * (double)(s->next[ch] - s->cur[ch])) * gain;
            out[i * 2u + (uint32_t)ch] = (int16_t)v;
        }
        s->frac += s->step;
        while (s->frac >= 1.0) {
            s->frac -= 1.0;
            s->cur[0] = s->next[0];
            s->cur[1] = s->next[1];
            pull_native(s, s->next);
        }
    }
    s->frames_done += frames;
    return frames;
}

static int rp_seek(void* state, uint32_t ms) {
    qsf_state* s = (qsf_state*)state;
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    int16_t skip[QSF_SKIP_CHUNK * 2u];
    if (!rp_start(s, 0, s->rate, NULL)) {
        return 0;
    }
    while (left > 0u) {
        uint32_t k = left < QSF_SKIP_CHUNK ? (uint32_t)left : QSF_SKIP_CHUNK;
        uint32_t got = rp_render(s, skip, k);
        if (got == 0u) {
            break;
        }
        left -= got;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    qsf_state* s = (qsf_state*)state;
    uint32_t n = 0;
    uint32_t total = s->length_ms + s->fade_ms;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    if (s->artist[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->artist;
    }
    if (s->game[0] != '\0') {
        s->meta[n].tag = SEL_META_RELEASE;
        s->meta[n++].value = (uintptr_t)s->game;
    }
    if (s->year > 0) {
        s->meta[n].tag = SEL_META_YEAR;
        s->meta[n++].value = (uintptr_t)s->year;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)s->format;
    if (total > 0u) {
        s->meta[n].tag = SEL_META_DURATION_MS;
        s->meta[n++].value = (uintptr_t)total;
    }
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

static sel_tag g_hooks[12];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "qsf");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "qsf,miniqsf");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "qsf");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
