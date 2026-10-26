/* YM replayer (design doc "Format coverage rule", row I428): a RENDER-kind adapter over StSound (third_party/stsound,
 * MIT, the player bbmp uses) for Atari ST / Amstrad CPC / ZX Spectrum YM2149 chip music: YM2! YM3! YM3b YM4! YM5!
 * YM6! (register dumps), MIX1 (digi-mix) and YMT1/YMT2 (tracker). StSound unpacks LH5-packed files itself (the usual
 * form of a .ym), so the probe accepts "-lh5-" at offset 2 as well. StSound renders mono int16 at the rate it was
 * created with; the adapter duplicates it to stereo. The song ends once (loop mode off); its length comes from the
 * frame count in the header. */
#include <StSoundLibrary.h>

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

#define YM_TEXT 256u
#define YM_CHUNK 1024u

typedef struct ym_state {
    const sel_host* host;
    YMMUSIC* music;
    const uint8_t* data;
    size_t size;
    uint32_t rate;
    uint32_t length_ms;
    uint64_t played;
    char title[YM_TEXT], author[YM_TEXT], format[YM_TEXT], message[YM_TEXT];
    sel_tag meta[8];
} ym_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size < 8u) {
        return SEL_PROBE_NO;
    }
    if (memcmp(data + 2, "-lh5-", 5) == 0) {
        return SEL_PROBE_YES;
    }
    if (data[0] == 'Y' && data[1] == 'M' && data[3] == '!' && data[2] >= '2' && data[2] <= '6') {
        return SEL_PROBE_YES;
    }
    if (memcmp(data, "YM3b", 4) == 0 || memcmp(data, "MIX1", 4) == 0 || memcmp(data, "YMT1", 4) == 0 ||
        memcmp(data, "YMT2", 4) == 0) {
        return SEL_PROBE_YES;
    }
    return SEL_PROBE_NO;
}

static void copy_text(char* dst, const char* src) {
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, YM_TEXT - 1u);
    dst[YM_TEXT - 1u] = '\0';
}

/* Creates a music object at `rate`, loads the file and sets the info fields; NULL when StSound refuses the file. */
static YMMUSIC* load_music(ym_state* s, uint32_t rate) {
    ymMusicInfo_t info;
    YMMUSIC* m = ymMusicCreateWithRate((ymint)rate);
    if (m == NULL) {
        return NULL;
    }
    if (!ymMusicLoadMemory(m, (void*)s->data, (ymu32)s->size)) {
        ymMusicDestroy(m);
        return NULL;
    }
    ymMusicSetLoopMode(m, YMFALSE);
    memset(&info, 0, sizeof info);
    ymMusicGetInfo(m, &info);
    s->length_ms = info.musicTimeInMs > 0 ? (uint32_t)info.musicTimeInMs : 0u;
    copy_text(s->title, info.pSongName);
    copy_text(s->author, info.pSongAuthor);
    copy_text(s->format, info.pSongType);
    copy_text(s->message, info.pSongComment);
    return m;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    ym_state* s;
    if (src->stream != NULL || src->data == NULL) {
        return NULL;
    }
    s = (ym_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    s->rate = 44100u;
    s->music = load_music(s, s->rate);
    if (s->music == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    ym_state* s = (ym_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        ymMusicDestroy(s->music);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    ym_state* s = (ym_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    /* StSound fixes the rate at creation; every start rebuilds the player from the same bytes. */
    ymMusicDestroy(s->music);
    s->music = load_music(s, rate);
    if (s->music == NULL) {
        return 0;
    }
    s->rate = rate;
    s->played = 0;
    ymMusicPlay(s->music);
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    ym_state* s = (ym_state*)state;
    ymsample mono[YM_CHUNK];
    uint32_t done = 0;
    while (done < frames) {
        uint32_t n = frames - done < YM_CHUNK ? frames - done : YM_CHUNK;
        uint32_t i;
        if (ymMusicIsOver(s->music)) {
            break;
        }
        ymMusicCompute(s->music, mono, (ymint)n);
        for (i = 0; i < n; i++) {
            out[(done + i) * 2u] = mono[i];
            out[(done + i) * 2u + 1u] = mono[i];
        }
        done += n;
    }
    s->played += done;
    return done;
}

static int rp_seek(void* state, uint32_t ms) {
    ym_state* s = (ym_state*)state;
    ymMusicSeek(s->music, (ymu32)ms);
    s->played = (uint64_t)ms * s->rate / 1000u;
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    ym_state* s = (ym_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    if (s->author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->author;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)(s->format[0] != '\0' ? s->format : "YM");
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)s->length_ms;
    if (s->message[0] != '\0') {
        s->meta[n].tag = SEL_META_MESSAGE;
        s->meta[n++].value = (uintptr_t)s->message;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "ym");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "ym");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "ym");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
