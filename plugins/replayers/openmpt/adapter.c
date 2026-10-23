/* openmpt adapter replayer (design doc section 4): bridges libopenmpt's C API
 * to the Selecta RENDER-kind replayer hooks. libopenmpt mixes itself, so the
 * adapter only forwards: probe, open, start (subsong + rate), render, seek,
 * position and metadata. The engine never feeds it voices. */
#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt_ext.h>

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

#define META_LEN 128u
#define MESSAGE_LEN 512u
#define LIST_LEN 2048u

#define SEEK_DISCARD_FRAMES 1024u

typedef struct openmpt_state {
    const sel_host* host;
    openmpt_module_ext* ext;
    openmpt_module* mod;
    openmpt_module_ext_interface_interactive inter;
    int have_inter;
    uint32_t rate;
    uint32_t size;
    char title[META_LEN];
    char author[META_LEN];
    char format[META_LEN];
    char message[MESSAGE_LEN];
    sel_tag meta[14];
    int16_t discard[SEEK_DISCARD_FRAMES * 2]; /* seek lead-in, allocated with the state */
} openmpt_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    int r;
    (void)name;
    r = openmpt_probe_file_header(OPENMPT_PROBE_FILE_HEADER_FLAGS_DEFAULT, data, size, (uint64_t)size, NULL,
                                  NULL, NULL, NULL, NULL, NULL);
    if (r == OPENMPT_PROBE_FILE_HEADER_RESULT_SUCCESS) {
        return SEL_PROBE_YES;
    }
    if (r == OPENMPT_PROBE_FILE_HEADER_RESULT_WANTMOREDATA) {
        return SEL_PROBE_UNSURE;
    }
    return SEL_PROBE_NO;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    openmpt_state* s;
    openmpt_module_ext* ext = openmpt_module_ext_create_from_memory(src->data, src->size, NULL, NULL, NULL, NULL,
                                                                    NULL, NULL, NULL);
    openmpt_module* mod;
    if (ext == NULL) {
        return NULL;
    }
    mod = openmpt_module_ext_get_module(ext);
    /* libopenmpt dithers integer output from a randomly seeded generator by
       default, so two renders of one song differ by up to 2 LSB. Golden WAVs
       need byte-identical output: turn dither off. */
    openmpt_module_ctl_set_integer(mod, "dither", 0);
    s = (openmpt_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        openmpt_module_ext_destroy(ext);
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->ext = ext;
    s->mod = mod;
    s->have_inter = openmpt_module_ext_get_interface(ext, LIBOPENMPT_EXT_C_INTERFACE_INTERACTIVE, &s->inter,
                                                     sizeof s->inter);
    s->size = (uint32_t)src->size;
    info->voice_count = 0; /* RENDER kind: no voices, no tick timebase */
    info->subsong_count = (uint32_t)openmpt_module_get_num_subsongs(mod);
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    openmpt_state* s = (openmpt_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        openmpt_module_ext_destroy(s->ext);
        host->mem_free(host, s);
    }
}

/* RENDER kind: `voices` is NULL. libopenmpt takes the rate at every read, so
 * it is kept here for rp_render. */
static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    openmpt_state* s = (openmpt_state*)state;
    (void)voices;
    if (!openmpt_module_select_subsong(s->mod, (int32_t)subsong)) {
        return 0;
    }
    openmpt_module_set_repeat_count(s->mod, 0); /* play once, then report the end */
    s->rate = rate;
    return 1;
}

/* RP_MUTE: libopenmpt mixes itself, so its own channel mute does the work. */
static void rp_mute(void* state, uint32_t channel, uint32_t muted) {
    openmpt_state* s = (openmpt_state*)state;
    if (s->have_inter && (int32_t)channel < openmpt_module_get_num_channels(s->mod)) {
        s->inter.set_channel_mute_status(s->ext, (int32_t)channel, muted != 0u);
    }
}

/* RP_SET_SPEED: libopenmpt's interactive pitch and tempo factors (Q16 to double). */
static void rp_set_speed(void* state, uint32_t pitch_q16, uint32_t tempo_q16) {
    openmpt_state* s = (openmpt_state*)state;
    if (s->have_inter) {
        s->inter.set_pitch_factor(s->ext, (double)pitch_q16 / 65536.0);
        s->inter.set_tempo_factor(s->ext, (double)tempo_q16 / 65536.0);
    }
}

/* RP_SAMPLE_GET: the sample table lives in sample_table.cpp (libopenmpt internals). */
extern int selecta_openmpt_sample_get(openmpt_module* mod, uint32_t n, sel_rp_sample* out);

static int rp_sample_get(void* state, uint32_t n, sel_rp_sample* out) {
    return selecta_openmpt_sample_get(((openmpt_state*)state)->mod, n, out);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    openmpt_state* s = (openmpt_state*)state;
    return (uint32_t)openmpt_module_read_interleaved_stereo(s->mod, (int32_t)s->rate, frames, out);
}

/* libopenmpt's set_position_seconds snaps UP to the next row boundary, so the
 * song would be ahead of the reported position. Land on the row containing
 * the target (row start <= target), then render and discard the remaining
 * lead-in so the song clock equals a straight-through render at the target.
 * ms is converted to seconds once, here. */
static int rp_seek(void* state, uint32_t ms) {
    openmpt_state* s = (openmpt_state*)state;
    double target = (double)ms / 1000.0;
    double at = openmpt_module_set_position_seconds(s->mod, target);
    /* libopenmpt snaps the request up to a row boundary. Ask for earlier times until the
     * reported landing time is at or before the target; the rest is rendered and discarded.
     * Only positions libopenmpt reports are used, so pattern breaks, jumps and delays
     * cannot misplace the landing. Each step doubles the back-off, so the loop ends at 0. */
    double back = 0.0;
    while (at > target) {
        double req;
        back = back == 0.0 ? at - target : back * 2.0;
        req = target - back;
        if (req < 0.0) {
            req = 0.0;
        }
        at = openmpt_module_set_position_seconds(s->mod, req);
        if (req == 0.0) {
            break;
        }
    }
    if (at < target && s->rate != 0u) {
        double lead = (target - at) * (double)s->rate;
        uint32_t left = (uint32_t)(lead + 0.5);
        while (left > 0u) {
            uint32_t n = left < SEEK_DISCARD_FRAMES ? left : SEEK_DISCARD_FRAMES;
            size_t got = openmpt_module_read_interleaved_stereo(s->mod, (int32_t)s->rate, n, s->discard);
            if (got == 0u) {
                break;
            }
            left -= (uint32_t)got;
        }
    }
    return 1;
}

static int rp_position(void* state, sel_rp_position* out) {
    openmpt_state* s = (openmpt_state*)state;
    out->order = (uint32_t)openmpt_module_get_current_order(s->mod);
    out->row = (uint32_t)openmpt_module_get_current_row(s->mod);
    out->pattern = (uint32_t)openmpt_module_get_current_pattern(s->mod);
    return 1;
}

/* RP_TRACKER_INFO / RP_TRACKER_CELLS from openmpt_module_get_pattern_row_
 * channel_command. The raw effect command is libopenmpt's internal
 * EffectCommand number (modcommand.h); the formatting call that would give
 * the letter allocates, and the cells may be read on the audio thread, so
 * the common ProTracker effects map by table and the rest read as no effect. */
static const uint8_t g_effect_letter[] = {
    /* NONE   ARPEGGIO PORTUP PORTDOWN TONEPORTA VIBRATO TONEPORTAVOL VIBRATOVOL */
    0xFF, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
    /* TREMOLO PANNING8 OFFSET VOLSLIDE POSJUMP VOLUME PATBREAK RETRIG */
    0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xFF,
    /* SPEED  TEMPO   TREMOR MODCMDEX */
    0xF, 0xF, 0xFF, 0xE,
};

static int rp_tracker_info(void* state, uint32_t order, sel_rp_tracker_info* out) {
    openmpt_state* s = (openmpt_state*)state;
    int32_t pattern;
    if (order >= (uint32_t)openmpt_module_get_num_orders(s->mod)) {
        return 0;
    }
    pattern = openmpt_module_get_order_pattern(s->mod, (int32_t)order);
    out->orders = (uint32_t)openmpt_module_get_num_orders(s->mod);
    out->patterns = (uint32_t)openmpt_module_get_num_patterns(s->mod);
    out->rows = pattern >= 0 ? (uint32_t)openmpt_module_get_pattern_num_rows(s->mod, pattern) : 0u;
    out->channels = (uint32_t)openmpt_module_get_num_channels(s->mod);
    return 1;
}

static uint32_t rp_tracker_cells(void* state, uint32_t order, uint32_t channel, uint32_t row_lo, uint32_t row_hi,
                                 sel_rp_cell* out, uint32_t cap) {
    openmpt_state* s = (openmpt_state*)state;
    int32_t pattern, ch = (int32_t)channel;
    uint32_t row, n = 0;
    if (order >= (uint32_t)openmpt_module_get_num_orders(s->mod)) {
        return 0;
    }
    pattern = openmpt_module_get_order_pattern(s->mod, (int32_t)order);
    for (row = row_lo; row < row_hi && n < cap; row++, n++) {
        int32_t r = (int32_t)row;
        uint8_t note, effect;
        memset(&out[n], 0, sizeof out[n]);
        if (pattern < 0) {
            continue;
        }
        note = openmpt_module_get_pattern_row_channel_command(s->mod, pattern, r, ch, OPENMPT_MODULE_COMMAND_NOTE);
        out[n].note = note <= 120u || note >= 254u ? note : 0u;
        out[n].instrument =
            openmpt_module_get_pattern_row_channel_command(s->mod, pattern, r, ch, OPENMPT_MODULE_COMMAND_INSTRUMENT);
        out[n].volume =
            openmpt_module_get_pattern_row_channel_command(s->mod, pattern, r, ch, OPENMPT_MODULE_COMMAND_VOLUME);
        effect = openmpt_module_get_pattern_row_channel_command(s->mod, pattern, r, ch, OPENMPT_MODULE_COMMAND_EFFECT);
        out[n].effect = effect < sizeof g_effect_letter && g_effect_letter[effect] != 0xFFu ? g_effect_letter[effect] : 0u;
        out[n].param =
            out[n].effect != 0u || effect == 1u
                ? openmpt_module_get_pattern_row_channel_command(s->mod, pattern, r, ch, OPENMPT_MODULE_COMMAND_PARAMETER)
                : 0u;
    }
    return n;
}

/* One metadata key into a fixed buffer; libopenmpt's string is freed here. */
static void fetch(openmpt_module* mod, const char* key, char* dst, size_t cap) {
    const char* v = openmpt_module_get_metadata(mod, key);
    dst[0] = '\0';
    if (v != NULL) {
        strncpy(dst, v, cap - 1u);
        dst[cap - 1u] = '\0';
        openmpt_free_string(v);
    }
}

static void add_str(openmpt_state* s, uint32_t* n, uint32_t tag, const char* value) {
    if (value[0] != '\0') {
        s->meta[*n].tag = tag;
        s->meta[*n].value = (uintptr_t)value;
        (*n)++;
    }
}

static void add_int(openmpt_state* s, uint32_t* n, uint32_t tag, uintptr_t value) {
    if (value != 0u) {
        s->meta[*n].tag = tag;
        s->meta[*n].value = value;
        (*n)++;
    }
}

static const sel_tag* rp_metadata(void* state) {
    openmpt_state* s = (openmpt_state*)state;
    uint32_t n = 0;
    double seconds;
    fetch(s->mod, "title", s->title, sizeof s->title);
    fetch(s->mod, "artist", s->author, sizeof s->author);
    fetch(s->mod, "type_long", s->format, sizeof s->format);
    fetch(s->mod, "message", s->message, sizeof s->message);
    seconds = openmpt_module_get_duration_seconds(s->mod);
    add_str(s, &n, SEL_META_TITLE, s->title);
    add_str(s, &n, SEL_META_AUTHOR, s->author);
    add_str(s, &n, SEL_META_FORMAT, s->format);
    add_str(s, &n, SEL_META_MESSAGE, s->message);
    add_int(s, &n, SEL_META_CHANNELS, (uintptr_t)openmpt_module_get_num_channels(s->mod));
    add_int(s, &n, SEL_META_PATTERNS, (uintptr_t)openmpt_module_get_num_patterns(s->mod));
    add_int(s, &n, SEL_META_INSTRUMENTS, (uintptr_t)openmpt_module_get_num_instruments(s->mod));
    add_int(s, &n, SEL_META_SAMPLES, (uintptr_t)openmpt_module_get_num_samples(s->mod));
    add_int(s, &n, SEL_META_SUBSONGS, (uintptr_t)openmpt_module_get_num_subsongs(s->mod));
    add_int(s, &n, SEL_META_DURATION_MS, (uintptr_t)(seconds * 1000.0));
    add_int(s, &n, SEL_META_SIZE, s->size);
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

/* Filled at run time: a function pointer cast to an integer is not a
 * constant expression in C99. */
static sel_tag g_hooks[20];
static sel_tag g_desc[5];
static char g_extensions[LIST_LEN];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

/* libopenmpt lists extensions separated by ';'; Selecta's lists use ','.
 * A list that would overflow the buffer fails the entry. */
static int build_extensions(void) {
    const char* list = openmpt_get_supported_extensions();
    size_t i, n;
    if (list == NULL) {
        return 0;
    }
    n = strlen(list);
    if (n + 1u > LIST_LEN) {
        openmpt_free_string(list);
        return 0;
    }
    for (i = 0; i < n; i++) {
        g_extensions[i] = list[i] == ';' ? ',' : list[i];
    }
    g_extensions[n] = '\0';
    openmpt_free_string(list);
    return 1;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u || !build_extensions()) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "openmpt");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t)g_extensions);
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_POSITION, SEL_TAG_FN(rp_position));
    set(&g_hooks[11], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[12], SEL_RP_TRACKER_INFO, SEL_TAG_FN(rp_tracker_info));
    set(&g_hooks[13], SEL_RP_TRACKER_CELLS, SEL_TAG_FN(rp_tracker_cells));
    set(&g_hooks[14], SEL_RP_MUTE, SEL_TAG_FN(rp_mute));
    set(&g_hooks[15], SEL_RP_SET_SPEED, SEL_TAG_FN(rp_set_speed));
    set(&g_hooks[16], SEL_RP_SAMPLE_GET, SEL_TAG_FN(rp_sample_get));
    set(&g_hooks[17], SEL_RP_MIN_CPU, 68020);
    set(&g_hooks[18], SEL_RP_NEEDS_FLOAT, 1);
    set(&g_hooks[19], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "openmpt");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
