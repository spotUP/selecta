/* StoneTracker replayer (row I710ae): .spm song + .sps sample bank, RENDER kind. The authors' no-OS StonePlayer_Hard.bin (not open source, never
 * shipped) runs on Musashi's 68020 inside a minimal Amiga with a software Paula (third_party/stonetracker_host, st_machine). The bin is a declared
 * system file (RP_SYSFILE, I710ga): the host checks its sha256 and hands the stored path through config_get "sysfile.StonePlayer_Hard.bin";
 * without it the host does not call open (system_file_missing, install prompt). The bank sits beside the song (RP_COMPANIONS "*.spm>*.sps");
 * a missing bank fails open, the engine then shows bank_missing (I1127c). Host only: the Amiga build runs the same bin on the real CPU (I710aea).
 * Trace: SELECTA_STONETRACKER_TRACE=1 prints "stonetracker: 68k host ran ..." at close (sentinel for tests/m3/stonetracker_decode.sh). */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "st_machine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ST_MAX_SECONDS 1200u

typedef struct st_state {
    const sel_host* host;
    uint8_t* spm;
    size_t spm_len;
    uint8_t* sps;
    size_t sps_len;
    uint8_t* player;
    size_t player_len;
    uint32_t rate;
    uint64_t rendered;
    int max_pos;
    int done;
    int started;
    sel_tag meta[3];
} st_state;

static int has_ext(const char* name, const char* ext) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    size_t el = strlen(ext);
    size_t i;
    if (nl <= el || name[nl - el - 1u] != '.') {
        return 0;
    }
    for (i = 0; i < el; i++) {
        if ((name[nl - el + i] | 0x20) != ext[i]) {
            return 0;
        }
    }
    return 1;
}

static uint8_t* read_file(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    uint8_t* buf = NULL;
    long n;
    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0 && n < (32L << 20) && fseek(f, 0, SEEK_SET) == 0) {
        buf = (uint8_t*)malloc((size_t)n);
        if (buf != NULL && fread(buf, 1, (size_t)n, f) != (size_t)n) {
            free(buf);
            buf = NULL;
        } else {
            *len = (size_t)n;
        }
    }
    fclose(f);
    return buf;
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)data;
    return has_ext(name, "spm") && size > 64u ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    st_state* s = (st_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        if (s->started) {
            st_machine_stop();
            if (getenv("SELECTA_STONETRACKER_TRACE") != NULL) {
                fprintf(stderr, "stonetracker: 68k host ran frames=%llu max_position=%d crashed=%d\n", (unsigned long long)s->rendered, s->max_pos,
                        st_machine_crashed());
            }
        }
        free(s->spm);
        free(s->sps);
        free(s->player);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    st_state* s;
    char bank[1100];
    size_t pl;
    const char* bin = host->config_get != NULL ? host->config_get(host, "sysfile.StonePlayer_Hard.bin", NULL) : NULL;
    if (src->stream != NULL || src->data == NULL || src->path == NULL || bin == NULL || !has_ext(src->path, "spm")) {
        return NULL;
    }
    pl = strlen(src->path);
    if (pl >= sizeof bank) {
        return NULL;
    }
    s = (st_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    memcpy(bank, src->path, pl - 3u);
    memcpy(bank + pl - 3u, "sps", 4u);
    s->sps = read_file(bank, &s->sps_len);
    s->player = read_file(bin, &s->player_len);
    s->spm = (uint8_t*)malloc(src->size);
    if (s->sps == NULL || s->player == NULL || s->spm == NULL || st_machine_set_player(s->player, s->player_len) != 0) {
        rp_close(s);
        return NULL;
    }
    memcpy(s->spm, src->data, src->size);
    s->spm_len = src->size;
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    st_state* s = (st_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    if (s->started) {
        st_machine_stop();
    }
    s->rate = rate;
    s->rendered = 0;
    s->max_pos = 0;
    s->done = 0;
    s->started = 1;
    if (st_machine_load(s->spm, s->spm_len, s->sps, s->sps_len, 1, (int)rate) != 0) {
        s->started = 0;
        return 0;
    }
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    st_state* s = (st_state*)state;
    float buf[2u * 1024u];
    uint32_t done = 0;
    if (!s->started || s->done) {
        return 0;
    }
    while (done < frames) {
        uint32_t n = frames - done < 1024u ? frames - done : 1024u;
        uint32_t i;
        int pos;
        n = (uint32_t)st_machine_render(buf, (int)n);
        if (n == 0u) {
            s->done = 1;
            break;
        }
        for (i = 0; i < n * 2u; i++) {
            float v = buf[i] * 32767.0f;
            out[done * 2u + i] = (int16_t)(v > 32767.0f ? 32767 : (v < -32768.0f ? -32768 : v));
        }
        done += n;
        s->rendered += n;
        pos = st_machine_get_position();
        if (st_machine_song_ended() || st_machine_crashed() || s->rendered >= (uint64_t)s->rate * ST_MAX_SECONDS) {
            s->done = 1; /* the song ended (st_machine_song_ended), the CPU crashed, or it ran away past the cap */
            break;
        }
        if (pos > s->max_pos) {
            s->max_pos = pos;
        }
    }
    return done;
}

static int rp_seek(void* state, uint32_t ms) {
    st_state* s = (st_state*)state;
    return ms == 0u && s->started && rp_start(s, 0, s->rate, NULL);
}

static const sel_tag* rp_metadata(void* state) {
    st_state* s = (st_state*)state;
    s->meta[0].tag = SEL_META_FORMAT;
    s->meta[0].value = (uintptr_t) "StoneTracker";
    s->meta[1].tag = SEL_TAG_END;
    s->meta[1].value = 0;
    return s->meta;
}

static const char g_companions[] = "*.spm>*.sps";
static const sel_tag g_sysfile[] = {
    {SEL_RP_SYSFILE_NAME, (uintptr_t) "StonePlayer_Hard.bin"},
    {SEL_RP_SYSFILE_LABEL, (uintptr_t) "StoneTracker player"},
    {SEL_RP_SYSFILE_HINT, (uintptr_t) "StonePlayer_Hard.bin is in stonefree2.lha on Aminet (mus/edit). Unpack Players.LHS with StoneEx, pick the .bin."},
    {SEL_RP_SYSFILE_SHA256, (uintptr_t) "16d70adb7f09fdf3fc7786f4b7cc7c988073c160ce9879e86c98b61e89f0c15c"},
    {SEL_RP_SYSFILE_SIZE, 83248},
    {SEL_RP_SYSFILE_URLS, (uintptr_t) "https://aminet.net/mus/edit/stonefree2.lha"},
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "stonetracker");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "spm");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_PRIORITY, 10);
    set(&g_hooks[12], SEL_RP_SYSFILE, (uintptr_t)g_sysfile);
    set(&g_hooks[13], SEL_RP_COMPANIONS, (uintptr_t)g_companions);
    set(&g_hooks[14], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "stonetracker");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
