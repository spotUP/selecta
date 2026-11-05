/* SNSF replayer (row I705a, bbmp backend "bsnes"): a RENDER-kind adapter over the vendored bsnes core
 * (third_party/bsnes, GPL-3.0; this plugin is a GPL module) and psflib (third_party/psflib, MIT). A .snsf song is a PSF
 * container (version 0x23): the exe section is {u32 ROM offset, u32 size, data} (later sections of a .snsflib chain
 * overwrite earlier ones), the reserved section optionally carries battery RAM as {u32 type, u32 size, data} records
 * (type 0 = SRAM). The exe layout is checked against real Modland sets (I705b: Wolfenstein 3D HiROM, Blackthorne
 * LoROM; a lib carries 4 trailing bytes after its one record, a .minisnsf is a one byte patch record); the reserved
 * SRAM records are not seen in those sets. The song is a whole cartridge
 * ROM: the 65816 starts it, it uploads its sound driver to the SPC700 and the S-DSP makes the audio. bsnes resamples
 * the DSP output (about 32040 Hz) itself to the host rate. Picture output is off and the picture processor is bsnes'
 * fast PPU. Length and fade come from the tags (default 3 minutes plus a 10 second fade). Open needs `src->path` for
 * the `_lib` lookup, like the qsf module. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "psflib.h"
#include "bsnes_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define SNSF_VERSION 0x23
#define SNSF_SKIP_CHUNK 1024u
#define SNSF_DEFAULT_LENGTH_MS 180000u
#define SNSF_DEFAULT_FADE_MS 10000u

typedef struct snsf_state {
    const sel_host* host;
    uint8_t* rom; /* merged cartridge image */
    size_t rom_size;
    uint8_t* sram;
    size_t sram_size;
    int running;
    uint32_t rate;
    uint32_t length_ms, fade_ms; /* from the tags; 0 = tag absent */
    int has_length;
    uint64_t frames_done, frames_total, fade_frames;
    char title[256], artist[256], game[256], format[8];
    int year;
    sel_tag meta[8];
} snsf_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size >= 4u && data[0] == 'P' && data[1] == 'S' && data[2] == 'F' && data[3] == SNSF_VERSION) {
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
    snsf_state* s = (snsf_state*)ctx;
    size_t pos = 0;
    if (exe_size >= 8u) {
        uint32_t offset = le32(exe), size = le32(exe + 4);
        /* {offset, size, data}; a size that does not fit means the rest of the section is the data */
        if (size > exe_size - 8u) {
            size = (uint32_t)(exe_size - 8u);
        }
        if (rom_put(&s->rom, &s->rom_size, offset, exe + 8, size) != 0) {
            return -1;
        }
    }
    while (pos + 8u <= reserved_size) {
        uint32_t type = le32(reserved + pos), size = le32(reserved + pos + 4);
        pos += 8u;
        if (size > reserved_size - pos) {
            return -1;
        }
        if (type == 0u && rom_put(&s->sram, &s->sram_size, 0, reserved + pos, size) != 0) {
            return -1;
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
    snsf_state* s = (snsf_state*)ctx;
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

static void free_state(snsf_state* s) {
    const sel_host* host = s->host;
    if (s->running) {
        snsf_bridge_close();
    }
    free(s->rom);
    free(s->sram);
    host->mem_free(host, s);
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    snsf_state* s;
    psf_file_callbacks cb;
    if (src->stream != NULL || src->path == NULL) {
        return NULL;
    }
    s = (snsf_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
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
    if (psf_load(src->path, &cb, SNSF_VERSION, load_cb, s, info_cb, s, 0, NULL, NULL) <= 0 || s->rom == NULL) {
        free_state(s);
        return NULL;
    }
    if (!s->has_length) {
        s->length_ms = SNSF_DEFAULT_LENGTH_MS;
        s->fade_ms = SNSF_DEFAULT_FADE_MS;
    }
    strcpy(s->format, "SNSF");
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    if (state != NULL) {
        free_state((snsf_state*)state);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    snsf_state* s = (snsf_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    {
        /* the board table needs a power-of-two image of at least 32 KiB */
        size_t size = 0x8000u;
        uint8_t* image;
        int rc;
        while (size < s->rom_size) {
            size <<= 1;
        }
        image = (uint8_t*)calloc(1, size);
        if (image == NULL) {
            return 0;
        }
        memcpy(image, s->rom, s->rom_size);
        rc = snsf_bridge_open(image, size, s->sram, s->sram_size, (double)rate);
        free(image);
        if (rc != 0) {
            s->running = 0;
            return 0;
        }
        s->running = 1;
    }
    s->rate = rate;
    s->frames_done = 0;
    s->frames_total = ((uint64_t)s->length_ms + s->fade_ms) * rate / 1000u;
    s->fade_frames = (uint64_t)s->fade_ms * rate / 1000u;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    snsf_state* s = (snsf_state*)state;
    uint32_t i;
    if (!s->running || s->frames_done >= s->frames_total) {
        return 0;
    }
    if ((uint64_t)frames > s->frames_total - s->frames_done) {
        frames = (uint32_t)(s->frames_total - s->frames_done);
    }
    snsf_bridge_render(out, frames);
    for (i = 0; i < frames; i++) {
        uint64_t pos = s->frames_done + i;
        if (s->fade_frames > 0u && pos + s->fade_frames > s->frames_total) {
            double gain = (double)(s->frames_total - pos) / (double)s->fade_frames;
            out[i * 2u] = (int16_t)((double)out[i * 2u] * gain);
            out[i * 2u + 1u] = (int16_t)((double)out[i * 2u + 1u] * gain);
        }
    }
    s->frames_done += frames;
    return frames;
}

static int rp_seek(void* state, uint32_t ms) {
    snsf_state* s = (snsf_state*)state;
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    int16_t skip[SNSF_SKIP_CHUNK * 2u];
    if (!rp_start(s, 0, s->rate, NULL)) {
        return 0;
    }
    while (left > 0u) {
        uint32_t k = left < SNSF_SKIP_CHUNK ? (uint32_t)left : SNSF_SKIP_CHUNK;
        uint32_t got = rp_render(s, skip, k);
        if (got == 0u) {
            break;
        }
        left -= got;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    snsf_state* s = (snsf_state*)state;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "snsf");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "snsf,minisnsf");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "snsf");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
