/* mdxmini replayer (row I699, design 2026-10-26-online-sources section 2a): a RENDER-kind adapter over the mdxmini
 * library (third_party/mdxmini, GPL-2.0-or-later; this plugin is a GPL module, see LICENSE). An .mdx song is the Sharp
 * X68000's MML for its YM2151 plus PCM8 channels. The library opens the song by file name: it reads the .mdx itself
 * and looks for the .pdx sample bank the song names in the same directory as the .mdx (so the song needs a path; a
 * song without one is refused). Its PCM output is 16-bit interleaved stereo at the rate set before open, so the
 * adapter opens once at RP_OPEN to validate and read length and title, and again at RP_START with the host rate.
 * The song plays its loop once and then ends. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "mdxmini.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#define MDX_PLAY_LOOPS 1
#define MDX_OPEN_RATE 44100
#define MDX_SKIP_CHUNK 1024

typedef struct mdx_state {
    const sel_host* host;
    char* path;
    t_mdxmini mini;
    int is_open;
    int ended;
    uint32_t rate;
    uint32_t length_ms;
    char title[256];
    sel_tag meta[8];
    char format[8];
} mdx_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    size_t i, lim = size < 1024u ? size : 1024u;
    (void)name;
    /* No magic: the file starts with the title, which ends in CR LF EOF (0x0D 0x0A 0x1A). */
    for (i = 0; i + 2u < lim; i++) {
        if (data[i] == 0x0Du && data[i + 1u] == 0x0Au && data[i + 2u] == 0x1Au) {
            return SEL_PROBE_UNSURE;
        }
    }
    return SEL_PROBE_NO;
}

static void close_song(mdx_state* s) {
    if (s->is_open) {
        mdx_close(&s->mini);
        s->is_open = 0;
    }
}

static int open_song(mdx_state* s, uint32_t rate) {
    mdx_set_rate((int)rate);
    memset(&s->mini, 0, sizeof s->mini);
    if (mdx_open(&s->mini, s->path, NULL) != 0) {
        return 0;
    }
    s->is_open = 1;
    s->ended = 0;
    if (s->mini.channels != 2) { /* the library's default is stereo; anything else is not handled */
        close_song(s);
        return 0;
    }
    mdx_set_max_loop(&s->mini, MDX_PLAY_LOOPS);
    return 1;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    mdx_state* s;
    char raw[1100];
    size_t i, n = 0;
    if (src->stream != NULL || src->data == NULL || src->path == NULL) {
        return NULL;
    }
    s = (mdx_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->path = (char*)host->mem_alloc(host, strlen(src->path) + 1u, SEL_MEM_FAST);
    if (s->path == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    strcpy(s->path, src->path);
    if (!open_song(s, MDX_OPEN_RATE)) {
        host->mem_free(host, s->path);
        host->mem_free(host, s);
        return NULL;
    }
    s->length_ms = (uint32_t)mdx_get_length(&s->mini) * 1000u;
    raw[0] = '\0';
    mdx_get_title(&s->mini, raw);
    /* Titles are Shift-JIS; keep the ASCII part, which is what a UTF-8 UI can show as is. */
    for (i = 0; raw[i] != '\0' && n + 1u < sizeof s->title; i++) {
        unsigned char c = (unsigned char)raw[i];
        if (c >= 0x20u && c < 0x7Fu) {
            s->title[n++] = (char)c;
        }
    }
    while (n > 0u && s->title[n - 1u] == ' ') {
        n--;
    }
    s->title[n] = '\0';
    close_song(s);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    mdx_state* s = (mdx_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        close_song(s);
        host->mem_free(host, s->path);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    mdx_state* s = (mdx_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    close_song(s);
    s->rate = rate;
    return open_song(s, rate);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    mdx_state* s = (mdx_state*)state;
    if (!s->is_open || s->ended) {
        return 0;
    }
    if (!mdx_calc_sample(&s->mini, out, (int)frames)) {
        s->ended = 1;
    }
    return frames;
}

static int rp_seek(void* state, uint32_t ms) {
    mdx_state* s = (mdx_state*)state;
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    short skip[MDX_SKIP_CHUNK * 2];
    close_song(s);
    if (!open_song(s, s->rate)) {
        return 0;
    }
    /* mdx_calc_log walks the song's frames without rendering sound */
    while (left > 0u && !s->ended) {
        int k = left < MDX_SKIP_CHUNK ? (int)left : MDX_SKIP_CHUNK;
        if (!mdx_calc_log(&s->mini, skip, k)) {
            s->ended = 1;
        }
        left -= (uint64_t)k;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    mdx_state* s = (mdx_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    strcpy(s->format, "MDX");
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)s->format;
    if (s->length_ms > 0u) {
        s->meta[n].tag = SEL_META_DURATION_MS;
        s->meta[n++].value = (uintptr_t)s->length_ms;
    }
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

/* RP_CONTENT_COMPANIONS (I1127b): the PDX sample set a .mdx names after its title (mdxfile.c: title ends at 0x0D 0x0A
 * 0x1A, then a NUL-terminated name; ".pdx" is appended when missing). Same walk, with bounds checks over the head. The
 * local open keeps mdxmini's own lookup (third_party is not touched); this feeds only the library cache. */
static unsigned rp_content_companions(const char* name, const uint8_t* data, size_t size, char out[][SEL_RP_COMPANION_NAME_MAX],
                                      unsigned max, const sel_host* host) {
    size_t nl = strlen(name), p = 0, n = 0;
    (void)host;
    if (nl < 4u || name[nl - 4u] != '.' || (name[nl - 3u] | 0x20) != 'm' || (name[nl - 2u] | 0x20) != 'd' || (name[nl - 1u] | 0x20) != 'x') {
        return 0;
    }
    if (data == NULL) {
        return 1;
    }
    if (max == 0u) {
        return 0;
    }
    while (p + 3u <= size && !(data[p] == 0x0D && data[p + 1u] == 0x0A && data[p + 2u] == 0x1A)) {
        p++;
    }
    if (p + 3u > size) {
        return 0;
    }
    p += 3u;
    while (p + n < size && data[p + n] != 0u && n < SEL_RP_COMPANION_NAME_MAX - 5u) {
        out[0][n] = (char)data[p + n];
        n++;
    }
    if (n == 0u || p + n >= size || data[p + n] != 0u) {
        return 0; /* no name, or an unterminated / over-long one */
    }
    out[0][n] = '\0';
    if (n < 4u || strcasecmp(out[0] + n - 4u, ".pdx") != 0) {
        memcpy(out[0] + n, ".pdx", 5u);
    }
    return 1;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "mdxmini");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "mdx");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_CONTENT_COMPANIONS, SEL_TAG_FN(rp_content_companions));
    set(&g_hooks[12], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "mdxmini");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
