/* Organya replayer (row I703): a RENDER-kind adapter over organya.h (third_party/organya, ISC-style licence) for
 * Studio Pixel's Organya songs (.org, Cave Story). The song loops between its repeat markers; the adapter plays the
 * intro and one pass of the loop (up to repeat_end) plus a short tail, then ends.
 * Sound bank: Organya needs a wavetable (100 melody waves) and 42 drum samples. Cave Story's wave100.wdb has no open
 * licence and is not vendored (bbmp downloads it too). Like the other render adapters (eupmini banks, mdxmini PDX) the
 * adapter reads `wave100.wdb` from the directory of src->path, so a local song with the bank beside it sounds like the
 * original. Without it a synthetic bank (sine, triangle, square, saw and harmonic waves; noise-burst drums) is used so
 * the song is still audible; the timbres differ from the game. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <organya.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ORG_MIN_SONG_SIZE (6u + 12u + 16u * 6u)
#define ORG_MIN_SOUNDBANK_SIZE (100u * 0x100u + 42u * 4u)
#define ORG_TAIL_SECONDS 3u
#define ORG_MAX_SECONDS 1200u
#define ORG_BANK_MAX 0x400000L
#define ORG_DRUM_SAMPLES 4096u

typedef struct org_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    uint8_t* bank; /* host buffer: wave100.wdb or the synthetic bank */
    size_t bank_size;
    int bank_is_file;
    organya_context ctx;
    int ctx_live;
    uint32_t rate;
    uint64_t frames;
    uint64_t end_frames;
    int ended;
    sel_tag meta[3];
} org_state;

static int ext_is_org(const char* name) {
    const char* dot;
    if (name == NULL || (dot = strrchr(name, '.')) == NULL || strlen(dot) != 4u) {
        return 0;
    }
    return (dot[1] | 0x20) == 'o' && (dot[2] | 0x20) == 'r' && (dot[3] | 0x20) == 'g';
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (size >= 6u && memcmp(data, "Org-0", 5u) == 0 && data[5] >= '1' && data[5] <= '3') {
        return SEL_PROBE_YES;
    }
    return (ext_is_org(name) && size >= 6u && memcmp(data, "Org-", 4u) == 0) ? SEL_PROBE_UNSURE : SEL_PROBE_NO;
}

/* Reads `<dir of path>wave100.wdb` (also WAVE100.WDB). Returns the host buffer or NULL. */
static uint8_t* read_bank(const sel_host* host, const char* path, size_t* size) {
    static const char* const names[2] = {"wave100.wdb", "WAVE100.WDB"};
    char full[1100];
    size_t dir = 0, k;
    const char* slash = path != NULL ? strrchr(path, '/') : NULL;
    *size = 0;
    if (slash != NULL) {
        dir = (size_t)(slash - path) + 1u;
    }
    if (dir + 12u > sizeof full) {
        return NULL;
    }
    for (k = 0; k < 2u; k++) {
        FILE* f;
        long len;
        uint8_t* buf;
        memcpy(full, path, dir);
        strcpy(full + dir, names[k]);
        f = fopen(full, "rb");
        if (f == NULL) {
            continue;
        }
        len = (fseek(f, 0, SEEK_END) == 0) ? ftell(f) : -1;
        buf = (len >= (long)ORG_MIN_SOUNDBANK_SIZE && len <= ORG_BANK_MAX && fseek(f, 0, SEEK_SET) == 0)
                  ? (uint8_t*)host->mem_alloc(host, (size_t)len, SEL_MEM_FAST) : NULL;
        if (buf != NULL && fread(buf, 1, (size_t)len, f) != (size_t)len) {
            host->mem_free(host, buf);
            buf = NULL;
        }
        fclose(f);
        if (buf != NULL) {
            *size = (size_t)len;
            return buf;
        }
    }
    return NULL;
}

static int8_t clamp8(double v) {
    return (int8_t)(v > 127.0 ? 127 : (v < -128.0 ? -128 : (int)v));
}

/* The synthetic bank in the .wdb layout: 100 waves of 256 signed bytes, then 42 drums as u32 length + signed bytes. */
static uint8_t* make_bank(const sel_host* host, size_t* size) {
    size_t total = (size_t)ORG_WAVETABLE_COUNT * 0x100u + (size_t)ORG_PERCUSSION_COUNT * (4u + ORG_DRUM_SAMPLES);
    uint8_t* b = (uint8_t*)host->mem_alloc(host, total, SEL_MEM_FAST);
    uint8_t* p;
    uint32_t seed = 12345u;
    int w, i, h, d;
    if (b == NULL) {
        return NULL;
    }
    for (w = 0; w < ORG_WAVETABLE_COUNT; w++) {
        uint8_t* o = b + (size_t)w * 0x100u;
        int kind = (w / 6) % 5;
        for (i = 0; i < 256; i++) {
            double ph = (double)i / 256.0 * 6.283185307179586, v = 0.0;
            if (kind == 0) {
                v = sin(ph) * 80.0;
            } else if (kind == 1) {
                v = (i < 64 ? i * 2 : (i < 192 ? 127 - (i - 64) * 2 : -128 + (i - 192) * 2)) * 0.8;
            } else if (kind == 2) {
                v = i < 128 + (w % 6) * 16 ? 56.0 : -56.0;
            } else if (kind == 3) {
                v = (double)i / 2.0 - 64.0;
            } else {
                int hn = 1 + w % 8;
                for (h = 1; h <= hn; h++) {
                    v += sin(ph * h) / h;
                }
                v = v * 60.0 / hn;
            }
            o[i] = (uint8_t)clamp8(v);
        }
    }
    p = b + (size_t)ORG_WAVETABLE_COUNT * 0x100u;
    for (d = 0; d < ORG_PERCUSSION_COUNT; d++) {
        p[0] = (uint8_t)(ORG_DRUM_SAMPLES & 0xFFu);
        p[1] = (uint8_t)((ORG_DRUM_SAMPLES >> 8) & 0xFFu);
        p[2] = 0;
        p[3] = 0;
        p += 4;
        for (i = 0; i < (int)ORG_DRUM_SAMPLES; i++) {
            double env = 1.0 - (double)i / (double)ORG_DRUM_SAMPLES;
            double tone = sin((double)i * (0.03 + 0.01 * (d % 7)) * (1.0 - 0.5 * env));
            double noise;
            seed = seed * 1664525u + 1013904223u;
            noise = ((double)((seed >> 16) & 0xFFu) - 128.0) / 128.0;
            p[i] = (uint8_t)clamp8((d % 3 == 0 ? tone : (d % 3 == 1 ? noise : 0.5 * tone + 0.5 * noise)) * env * env * 110.0);
        }
        p += ORG_DRUM_SAMPLES;
    }
    *size = total;
    return b;
}

static void ctx_free(org_state* s) {
    if (s->ctx_live) {
        organya_context_deinit(&s->ctx); /* unloads the song itself */
        s->ctx_live = 0;
    }
}

static void rp_close(void* state) {
    org_state* s = (org_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        ctx_free(s);
        host->mem_free(host, s->data);
        host->mem_free(host, s->bank);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    org_state* s;
    organya_song probe;
    if (src->stream != NULL || src->data == NULL || src->size < ORG_MIN_SONG_SIZE) {
        return NULL;
    }
    if (organya_song_init(&probe) != ORG_RESULT_SUCCESS) {
        return NULL;
    }
    if (organya_song_read(&probe, src->data, src->size) != ORG_RESULT_SUCCESS) {
        organya_song_deinit(&probe);
        return NULL;
    }
    organya_song_deinit(&probe);
    s = (org_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
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
        s->bank = read_bank(host, src->path, &s->bank_size);
        s->bank_is_file = s->bank != NULL;
    }
    if (s->bank == NULL) {
        s->bank = make_bank(host, &s->bank_size);
    }
    if (s->bank == NULL) {
        rp_close(s);
        return NULL;
    }
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int restart(org_state* s) {
    ctx_free(s);
    if (organya_context_init(&s->ctx) != ORG_RESULT_SUCCESS) {
        return 0;
    }
    s->ctx_live = 1;
    organya_context_set_sample_rate(&s->ctx, s->rate);
    organya_context_set_interpolation(&s->ctx, ORG_INTERPOLATION_CUBIC);
    organya_context_set_output_format(&s->ctx, ORG_OUTPUT_FORMAT_S16, ORG_FALSE);
    if (organya_context_read_soundbank(&s->ctx, s->bank, s->bank_size) != ORG_RESULT_SUCCESS
        || organya_context_read_song(&s->ctx, s->data, s->size) != ORG_RESULT_SUCCESS) {
        ctx_free(s);
        return 0;
    }
    organya_context_seek(&s->ctx, 0);
    s->end_frames = ((uint64_t)(s->ctx.song.repeat_end > 0 ? s->ctx.song.repeat_end : 0) * s->ctx.song.tempo_ms * s->rate) / 1000u
                    + (uint64_t)s->rate * ORG_TAIL_SECONDS;
    if (s->end_frames > (uint64_t)s->rate * ORG_MAX_SECONDS) {
        s->end_frames = (uint64_t)s->rate * ORG_MAX_SECONDS;
    }
    s->frames = 0;
    s->ended = 0;
    return 1;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    org_state* s = (org_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    return restart(s);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    org_state* s = (org_state*)state;
    uint64_t left;
    uint32_t n;
    if (!s->ctx_live || s->ended) {
        return 0;
    }
    left = s->end_frames - s->frames;
    n = (uint32_t)(left < frames ? left : frames);
    n = (uint32_t)organya_context_generate_samples(&s->ctx, out, n);
    s->frames += n;
    if (n < frames) {
        s->ended = 1;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    org_state* s = (org_state*)state;
    int16_t scratch[2048 * 2];
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    if (!s->ctx_live || !restart(s)) {
        return 0;
    }
    /* Voices carry state (envelopes, sample positions): run the song forward and discard the output. */
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
    org_state* s = (org_state*)state;
    s->meta[0].tag = SEL_META_FORMAT;
    s->meta[0].value = (uintptr_t)(s->bank_is_file ? "Organya (wave100.wdb)" : "Organya (synthetic bank)");
    s->meta[1].tag = SEL_TAG_END;
    s->meta[1].value = 0;
    return s->meta;
}

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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "organya");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "org");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_PRIORITY, 10);
    set(&g_hooks[12], SEL_RP_NEEDS_FLOAT, 1);
    /* I1313: RAM peak 932052 B (module hunk plus allocations) rendering org_test.org, measured 2026-11-11 by vamos -l mem_alloc:info, render peak minus a no-player baseline
     * (tools/player_ram.py). Declared = peak +25%, rounded up to 64 KB; the registry hides the player
     * when this is above the RAM free after Selecta has started. */
    set(&g_hooks[13], SEL_RP_MEMORY, 1179648);
    set(&g_hooks[14], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "organya");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
