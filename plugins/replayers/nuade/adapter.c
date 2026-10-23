/* nuade adapter replayer (design doc section 4): bridges nuade's
 * NuadePlayer vtable to the Selecta replayer hooks. One plugin fronts every
 * player compiled into selecta_nuade (the registry walk): probe asks them
 * all, open binds the song to the one that recognises it. */
#include "shim.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

#define META_LEN 128u
#define MESSAGE_LEN 512u
#define LIST_LEN 4096u
#define SAMPLE_CATALOGUE_MAX 64u
#define SAMPLE_SCAN_RATE 44100u
#define SAMPLE_SCAN_LIMIT_SEC 600u
#define SAMPLE_BASE_RATE 8287u

typedef struct nuade_state {
    const sel_host* host;
    const NuadePlayer* player;
    void* pdata;
    NuadeSongSource src;
    NuadeVoices voices;
    char title[META_LEN];
    char author[META_LEN];
    char format[META_LEN];
    char message[MESSAGE_LEN];
    uint32_t channels;
    uint32_t subsongs;
    uint32_t instruments;
    uintptr_t size;
    sel_tag meta[10];
    uint32_t default_subsong;
    int scanned;                       /* RP_SAMPLE_GET catalogue built */
    uint32_t cat_n;
    sel_sample cat[SAMPLE_CATALOGUE_MAX]; /* distinct samples, order of first use */
    char cat_name[SAMPLE_CATALOGUE_MAX][12];
} nuade_state;

/* A player this adapter can drive: tick players only. Self-rendering
 * players would need a RENDER-kind adapter. */
static int usable(const NuadePlayer* p) {
    return p != NULL && p->probe != NULL && p->tick != NULL;
}

/* Asks every usable player. A player that is sure wins at once; otherwise
 * the first unsure one. Returns the SEL_PROBE_* verdict and the player. */
static int probe_all(const uint8_t* data, size_t size, const char* name, const NuadePlayer** found) {
    uint32_t i, n = nuade_player_count();
    int verdict = SEL_PROBE_NO;
    *found = NULL;
    for (i = 0; i < n; i++) {
        const NuadePlayer* p = nuade_player_get(i);
        if (!usable(p)) {
            continue;
        }
        switch (p->probe(data, size, name)) {
        case NuadeProbe_Supported:
            *found = p;
            return SEL_PROBE_YES;
        case NuadeProbe_Unsure:
            if (verdict == SEL_PROBE_NO) {
                verdict = SEL_PROBE_UNSURE;
                *found = p;
            }
            break;
        default:
            break;
        }
    }
    return verdict;
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    const NuadePlayer* p;
    return probe_all(data, size, name, &p);
}

/* Option this adapter declares in SEL_RP_CONFIG (I134): "nuade.gain", output gain percent 0 to 100, answered by the
 * host from the rp.config pref or the per-song override. Unanswered: 100, the song's own volume. */
static const char* const k_cfg_gain = "nuade.gain";

static uint32_t configured_gain(const sel_host* host) {
    const char* v = host->config_get != NULL ? host->config_get(host, k_cfg_gain, NULL) : NULL;
    long n = 100;
    if (v != NULL) {
        n = strtol(v, NULL, 10);
        if (n < 0) n = 0;
        if (n > 100) n = 100;
    }
    return (uint32_t)n;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    const NuadePlayer* p;
    nuade_state* s;
    NuadePlayerOpenResult r;
    if (probe_all(src->data, src->size, src->path, &p) == SEL_PROBE_NO) {
        return NULL;
    }
    s = (nuade_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->voices.gain = configured_gain(host);
    s->player = p;
    s->src.data = src->data;
    s->src.size = src->size;
    s->src.path = src->path;
    s->src.io = sel_nuade_io_none();
    r = p->open(&s->src);
    if (r.user_data == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    if (r.info.voice_count > SEL_NUADE_MAX_VOICES) {
        p->close(r.user_data);
        host->mem_free(host, s);
        return NULL;
    }
    s->pdata = r.user_data;
    s->channels = r.info.display_voices != 0u ? r.info.display_voices : r.info.voice_count;
    s->subsongs = r.info.subsong_count;
    s->default_subsong = r.info.default_subsong;
    s->size = (uintptr_t)src->size;
    info->voice_count = r.info.voice_count;
    info->subsong_count = r.info.subsong_count;
    info->default_subsong = r.info.default_subsong;
    info->timebase_hz = r.info.timebase_hz;
    return s;
}

static void rp_close(void* state) {
    nuade_state* s = (nuade_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        s->player->close(s->pdata);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    nuade_state* s = (nuade_state*)state;
    s->voices.api = voices;
    return s->player->start(s->pdata, subsong, rate, &s->voices) ? 1 : 0;
}

static void rp_tick(void* state, sel_tick_result* out) {
    nuade_state* s = (nuade_state*)state;
    NuadeTickResult r = s->player->tick(s->pdata, &s->voices);
    out->length = r.length;
    out->flags = ((r.flags & NuadeTick_Looped) ? SEL_TICK_LOOPED : 0u) |
                 ((r.flags & NuadeTick_Ended) ? SEL_TICK_ENDED : 0u);
}

/* RP_TRACKER_INFO / RP_TRACKER_CELLS: the player's tracker_info and
 * tracker_cells hooks, which are stateless and read-only. A player without
 * them (event or grid players) reports no pattern data: the hooks answer 0. */
static NuadePosition order_at(uint32_t order) {
    NuadePosition at;
    memset(&at, 0, sizeof at);
    at.mode = NuadeScroll_Rows;
    at.order = order;
    return at;
}

#define MAX_ORDERS 4096u

static int rp_tracker_info(void* state, uint32_t order, sel_rp_tracker_info* out) {
    nuade_state* s = (nuade_state*)state;
    NuadePosition at = order_at(order);
    NuadeTrackerInfoResult r;
    uint32_t n;
    if (s->player->tracker_info == NULL) {
        return 0;
    }
    r = s->player->tracker_info(s->pdata, &at);
    if (!r.valid) {
        return 0;
    }
    /* The player has no order count: probe upwards until no pattern is there. */
    for (n = order + 1u; n < MAX_ORDERS; n++) {
        at = order_at(n);
        if (!s->player->tracker_info(s->pdata, &at).valid) {
            break;
        }
    }
    out->orders = n;
    out->patterns = 0u; /* nuade does not report it */
    out->rows = r.info.rows;
    out->channels = r.info.channels;
    return 1;
}

static int col_is(const NuadeColumnDesc* c, const char* a, const char* b, const char* d) {
    return strcmp(c->name, a) == 0 || strcmp(c->name, b) == 0 || strcmp(c->name, d) == 0;
}

static uint8_t clamp_u8(int32_t v) {
    return v < 0 ? 0u : v > 255 ? 255u : (uint8_t)v;
}

/* nuade names notes the ProTracker way (period 856 = C-1, so 428 = C-2; value
 * 12 = C-1, plus the column's octave_shift for formats that sit lower or
 * higher). libopenmpt, whose scale sel_rp_cell.note uses, shows the same
 * period three octaves up (428 = C-5 = note 61). Every nuade tracker player
 * derives its value from Paula periods in that ProTracker numbering (formats
 * with a different base declare it in octave_shift), so one constant converts
 * all of them. */
#define NUADE_TO_OPENMPT_OCTAVES 3

static uint8_t note_of(int32_t v, int32_t octave_shift) {
    int32_t n;
    if (v == NUADE_NOTE_CUT) {
        return 254u;
    }
    if (v == NUADE_NOTE_OFF) {
        return 255u;
    }
    n = v < 0 ? 0 : v + 12 * (octave_shift + NUADE_TO_OPENMPT_OCTAVES) + 1;
    return n >= 1 && n <= 120 ? (uint8_t)n : 0u;
}

static uint32_t rp_tracker_cells(void* state, uint32_t order, uint32_t channel, uint32_t row_lo, uint32_t row_hi,
                                 sel_rp_cell* out, uint32_t cap) {
    nuade_state* s = (nuade_state*)state;
    NuadePosition at = order_at(order);
    NuadeTrackerInfoResult r;
    NuadePatternCell cell;
    uint32_t row, n = 0, c;
    if (s->player->tracker_info == NULL || s->player->tracker_cells == NULL) {
        return 0;
    }
    r = s->player->tracker_info(s->pdata, &at);
    if (!r.valid) {
        return 0;
    }
    for (row = row_lo; row < row_hi && n < cap; row++) {
        memset(&out[n], 0, sizeof out[n]);
        if (s->player->tracker_cells(s->pdata, &at, channel, (int32_t)row, (int32_t)row + 1, &cell, 1u) == 1u) {
            for (c = 0; c < r.info.column_count && c < NUADE_MAX_COLUMNS; c++) {
                const NuadeColumnDesc* d = &r.info.columns[c];
                if (cell.value[c] < 0 && d->kind != NuadeColumn_Note) {
                    continue;
                }
                if (d->kind == NuadeColumn_Note) {
                    out[n].note = note_of(cell.value[c], d->octave_shift);
                } else if (col_is(d, "smp", "inst", "ins")) {
                    out[n].instrument = clamp_u8(cell.value[c]);
                } else if (col_is(d, "vol", "volume", "vel")) {
                    out[n].volume = clamp_u8(cell.value[c]);
                } else if (col_is(d, "cmd", "fx", "eff")) {
                    out[n].effect = clamp_u8(cell.value[c]);
                } else if (col_is(d, "arg", "par", "prm")) {
                    out[n].param = clamp_u8(cell.value[c]);
                }
            }
        }
        n++;
    }
    return n;
}

/* RP_SAMPLE_GET. The nuade players have no sample hook, so the catalogue is
 * built from what they hand to the voice API: a second player instance (the
 * scan instance) runs tick by tick against a recording backend until the song
 * ends, loops, or SAMPLE_SCAN_LIMIT_SEC of song time passed, and every distinct
 * sample data pointer is catalogued in order of first use. Built on first call,
 * on the calling thread. Slot numbers are catalogue positions. */
static void scan_record(const sel_sample* smp, nuade_state* s) {
    uint32_t i;
    if (smp == NULL || smp->data == NULL || smp->length == 0u) {
        return;
    }
    for (i = 0; i < s->cat_n; i++) {
        if (s->cat[i].data == smp->data) {
            return;
        }
    }
    if (s->cat_n < SAMPLE_CATALOGUE_MAX) {
        s->cat[s->cat_n++] = *smp;
    }
}

static void scan_play(const sel_voices* v, uint32_t voice, const sel_sample* smp, uint32_t start) {
    (void)voice;
    (void)start;
    scan_record(smp, (nuade_state*)v->ctx);
}

static void scan_queue(const sel_voices* v, uint32_t voice, const sel_sample* smp) {
    (void)voice;
    scan_record(smp, (nuade_state*)v->ctx);
}

static void scan_stop(const sel_voices* v, uint32_t voice) {
    (void)v;
    (void)voice;
}

static void scan_set(const sel_voices* v, uint32_t voice, uint32_t value) {
    (void)v;
    (void)voice;
    (void)value;
}

static void scan_pan(const sel_voices* v, uint32_t voice, int32_t value) {
    (void)v;
    (void)voice;
    (void)value;
}

static int scan_is_playing(const sel_voices* v, uint32_t voice) {
    (void)v;
    (void)voice;
    return 0;
}

static void scan_get_state(const sel_voices* v, uint32_t voice, sel_voice_state* out) {
    (void)v;
    (void)voice;
    memset(out, 0, sizeof *out);
}

static void scan_samples(nuade_state* s) {
    sel_voices api;
    NuadeVoices nv;
    NuadePlayerOpenResult r;
    uint64_t units = 0, limit;
    uint32_t i;
    s->scanned = 1;
    memset(&api, 0, sizeof api);
    api.size = (uint32_t)sizeof api;
    api.play = scan_play;
    api.queue = scan_queue;
    api.stop = scan_stop;
    api.set_pitch_hz = scan_set;
    api.set_period = scan_set;
    api.set_volume = scan_set;
    api.set_pan = scan_pan;
    api.is_playing = scan_is_playing;
    api.get_state = scan_get_state;
    api.ctx = s;
    memset(&nv, 0, sizeof nv);
    nv.api = &api;
    r = s->player->open(&s->src);
    if (r.user_data == NULL) {
        return;
    }
    limit = (uint64_t)SAMPLE_SCAN_LIMIT_SEC * (r.info.timebase_hz != 0u ? r.info.timebase_hz : 1000u);
    if (s->player->start(r.user_data, s->default_subsong, SAMPLE_SCAN_RATE, &nv)) {
        for (i = 0; i < 1000000u && units < limit; i++) {
            NuadeTickResult t = s->player->tick(r.user_data, &nv);
            units += t.length != 0u ? t.length : 1u;
            if (t.flags & (NuadeTick_Looped | NuadeTick_Ended)) {
                break;
            }
        }
    }
    s->player->close(r.user_data);
}

static int rp_sample_get(void* state, uint32_t n, sel_rp_sample* out) {
    nuade_state* s = (nuade_state*)state;
    const sel_sample* c;
    if (!s->scanned) {
        scan_samples(s);
    }
    out->count = s->cat_n;
    if (n < 1u || n > s->cat_n) {
        return 0;
    }
    c = &s->cat[n - 1u];
    out->data = c->data;
    out->length = c->length;
    out->loop_start = c->loop_length != 0u ? c->loop_start : 0u;
    out->loop_length = c->loop_length;
    out->bits = c->format == SEL_SAMPLE_S16 ? 16u : 8u;
    out->channels = 1u;
    out->rate = SAMPLE_BASE_RATE;
    {
        char* nm = s->cat_name[n - 1u];
        memcpy(nm, "sample ", 7);
        nm[7] = (char)('0' + (n / 10u) % 10u);
        nm[8] = (char)('0' + n % 10u);
        nm[9] = '\0';
        out->name = nm;
    }
    return 1;
}

static void copy_str(char* dst, size_t cap, const char* value) {
    strncpy(dst, value, cap - 1u);
    dst[cap - 1u] = '\0';
}

static void set_tag(void* ctx, const char* key, const char* value) {
    nuade_state* s = (nuade_state*)ctx;
    if (strcmp(key, NUADE_META_TITLE) == 0) {
        copy_str(s->title, sizeof s->title, value);
    } else if (strcmp(key, NUADE_META_ARTIST) == 0) {
        copy_str(s->author, sizeof s->author, value);
    } else if (strcmp(key, NUADE_META_FORMAT) == 0) {
        copy_str(s->format, sizeof s->format, value);
    } else if (strcmp(key, NUADE_META_MESSAGE) == 0) {
        copy_str(s->message, sizeof s->message, value);
    }
}

static void add_instrument(void* ctx, uint32_t index, const char* name) {
    nuade_state* s = (nuade_state*)ctx;
    (void)index;
    (void)name;
    s->instruments++;
}

static void add_str(nuade_state* s, uint32_t* n, uint32_t tag, const char* value) {
    if (value[0] != '\0') {
        s->meta[*n].tag = tag;
        s->meta[*n].value = (uintptr_t)value;
        (*n)++;
    }
}

static void add_int(nuade_state* s, uint32_t* n, uint32_t tag, uintptr_t value) {
    if (value != 0u) {
        s->meta[*n].tag = tag;
        s->meta[*n].value = value;
        (*n)++;
    }
}

static const sel_tag* rp_metadata(void* state) {
    nuade_state* s = (nuade_state*)state;
    NuadeMetadataSink sink;
    uint32_t n = 0;
    sink.ctx = s;
    sink.set_tag = set_tag;
    sink.add_instrument = add_instrument;
    s->title[0] = s->author[0] = s->format[0] = s->message[0] = '\0';
    s->instruments = 0;
    if (s->player->metadata != NULL) {
        s->player->metadata(s->pdata, &sink);
    }
    if (s->format[0] == '\0') {
        copy_str(s->format, sizeof s->format, s->player->name);
    }
    /* Only what the player reported. nuade has no pattern or sample count
     * and no song length at player level, so those tags are absent. */
    add_str(s, &n, SEL_META_TITLE, s->title);
    add_str(s, &n, SEL_META_AUTHOR, s->author);
    add_str(s, &n, SEL_META_FORMAT, s->format);
    add_str(s, &n, SEL_META_MESSAGE, s->message);
    add_int(s, &n, SEL_META_CHANNELS, s->channels);
    add_int(s, &n, SEL_META_SUBSONGS, s->subsongs);
    add_int(s, &n, SEL_META_INSTRUMENTS, s->instruments);
    add_int(s, &n, SEL_META_SIZE, s->size);
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

/* Filled at run time: a function pointer cast to an integer is not a
 * constant expression in C99. */
static sel_tag g_hooks[20];
static sel_tag g_config[2]; /* SEL_RP_CONFIG list: one entry per option, value = the host config key */
static sel_tag g_desc[5];
/* Built by the registry walk at entry: every usable player's id, and the
 * union of their extensions. A list that would overflow its buffer fails the
 * entry instead of truncating. */
static char g_formats[LIST_LEN];
static char g_extensions[LIST_LEN];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

static int has_token(const char* list, const char* tok, size_t len) {
    const char* q = list;
    while (*q != '\0') {
        const char* e = strchr(q, ',');
        size_t l = e != NULL ? (size_t)(e - q) : strlen(q);
        if (l == len && strncmp(q, tok, len) == 0) {
            return 1;
        }
        q += l + (e != NULL ? 1u : 0u);
    }
    return 0;
}

static int append_token(char* list, const char* tok, size_t len) {
    size_t used = strlen(list);
    if (len == 0u || has_token(list, tok, len)) {
        return 1;
    }
    if (used + (used != 0u ? 1u : 0u) + len + 1u > LIST_LEN) {
        return 0;
    }
    if (used != 0u) {
        list[used++] = ',';
    }
    memcpy(list + used, tok, len);
    list[used + len] = '\0';
    return 1;
}

static int build_lists(void) {
    uint32_t i, n = nuade_player_count();
    g_formats[0] = g_extensions[0] = '\0';
    for (i = 0; i < n; i++) {
        const NuadePlayer* p = nuade_player_get(i);
        const char* q;
        if (!usable(p)) {
            continue;
        }
        if (!append_token(g_formats, p->id, strlen(p->id))) {
            return 0;
        }
        q = p->extensions != NULL ? p->extensions : "";
        while (*q != '\0') {
            const char* e = strchr(q, ',');
            size_t l = e != NULL ? (size_t)(e - q) : strlen(q);
            if (!append_token(g_extensions, q, l)) {
                return 0;
            }
            q += l + (e != NULL ? 1u : 0u);
        }
    }
    return 1;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u || !build_lists()) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "nuade");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_VOICE);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t)g_extensions);
    set(&g_hooks[4], SEL_RP_FORMATS, (uintptr_t)g_formats);
    set(&g_hooks[5], SEL_RP_MAX_VOICES, SEL_NUADE_MAX_VOICES);
    set(&g_hooks[6], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[7], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[8], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[9], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[10], SEL_RP_TICK, SEL_TAG_FN(rp_tick));
    set(&g_hooks[11], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[12], SEL_RP_TRACKER_INFO, SEL_TAG_FN(rp_tracker_info));
    set(&g_hooks[13], SEL_RP_TRACKER_CELLS, SEL_TAG_FN(rp_tracker_cells));
    set(&g_hooks[14], SEL_RP_SAMPLE_GET, SEL_TAG_FN(rp_sample_get));
    /* Needs: 68020 build, floating point in the voice shim (pending the T60 per-player
     * measurement), and a 1.85 MB runtime image (fast RAM counts). */
    set(&g_hooks[15], SEL_RP_MIN_CPU, 68020);
    set(&g_hooks[16], SEL_RP_NEEDS_FLOAT, 1);
    set(&g_hooks[17], SEL_RP_MEMORY, 2097152);
    set(&g_hooks[18], SEL_RP_CONFIG, (uintptr_t)g_config);
    set(&g_hooks[19], SEL_TAG_END, 0);
    set(&g_config[0], SEL_TAG_USER, (uintptr_t)k_cfg_gain);
    set(&g_config[1], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "nuade");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
