/* Engine shell (M0a): transport state, volume, output name, options,
 * batches. Every verb in commands.def reaches dispatch; the ones outside
 * SEL_CMD_IMPLEMENTED answer ERROR not_implemented and are counted by the
 * ratchet test. */
#include <selecta/engine.h>

#include <selecta/plugin.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>

#include "mixer.h"
#include "scheduler.h"
#include "spsc.h"

#include <stdio.h>
#include <string.h>

/* The verbs with real behaviour in M0a. Adding a row here is how a later
 * plan lowers tests/ratchet/unimplemented.txt. */
#define SEL_CMD_IMPLEMENTED(X)                                                                \
    X(PLAY)                                                                                   \
    X(PAUSE)                                                                                  \
    X(PAUSE_PLAYBACK)                                                                         \
    X(STOP)                                                                                   \
    X(VOLUME)                                                                                 \
    X(OUTPUT)                                                                                 \
    X(OPTION)                                                                                 \
    X(COMMANDS)                                                                               \
    X(EVENTS)                                                                                 \
    X(TAGTYPES)                                                                               \
    X(BEGIN)                                                                                  \
    X(END)                                                                                    \
    X(OPEN)                                                                                   \
    X(SUBSONG)

#define SEL_ERROR_DETAIL_MAX 64
#define SEL_LOG_LINE_MAX 96

/* Events coalesced inside a batch: the latest value wins, emitted at END. */
#define PENDING_STATE (1u << 0)
#define PENDING_VOLUME (1u << 1)
#define PENDING_OUTPUT (1u << 2)
#define PENDING_OPTION_BASE (1u << 3) /* one bit per option above this */

struct sel_engine {
    const sel_host* host;
    sel_state state;
    sel_spsc cmd_q;
    sel_spsc evt_q;
    uint32_t in_batch;
    uint32_t pending;
    uint32_t render_calls;
    uint32_t error_slot;
    char error_detail[8][SEL_ERROR_DETAIL_MAX];
    uint32_t log_slot;
    char log_line[8][SEL_LOG_LINE_MAX];
    uint32_t (*registry_count)(void);
    const sel_tag* (*registry_get)(uint32_t index);
    /* open song (rp_state != NULL) */
    uint32_t rate;
    uint32_t resampler;
    void* rp_state;
    sel_rp_close_fn rp_close;
    sel_rp_start_fn rp_start;
    sel_rp_tick_fn rp_tick;
    sel_rp_metadata_fn rp_metadata;
    sel_rp_info rp_info;
    uint8_t* song_data;
    uint32_t subsong;
    sel_mixer mixer;
    sel_scheduler sched;
    uint32_t frames_left; /* frames of the current tick still to mix */
    uint32_t ticks;
    uint32_t song_frames;  /* frames mixed since the subsong started */
    uint32_t pos_emitted;  /* whole seconds last reported */
    uint32_t last_tick;    /* the current tick carries SEL_TICK_ENDED */
    uint32_t ended;        /* set by render, reported by the next pump */
};

static const char* const k_option_names[] = {
#define SEL_OPTION_NAME(id, name) name,
    SEL_OPTIONS(SEL_OPTION_NAME)
#undef SEL_OPTION_NAME
};

const char* sel_option_name(sel_option_id id) {
    return (uint32_t)id < SEL_OPTION_COUNT ? k_option_names[id] : NULL;
}

const char* sel_play_state_name(sel_play_state s) {
    switch (s) {
    case SEL_PLAYING:
        return "playing";
    case SEL_PAUSED:
        return "paused";
    case SEL_STOPPED:
    default:
        return "stopped";
    }
}

/* ---- event emission -------------------------------------------------- */

static void evt_init(sel_message* m, sel_evt_id id) {
    memset(m, 0, sizeof *m);
    m->kind = SEL_MSG_EVT;
    m->id = id;
}

static void evt_str(sel_message* m, const char* s) {
    sel_arg* a = &m->args[m->argc++];
    a->type = SEL_ARG_STR;
    a->v.s = s;
}

static void evt_int(sel_message* m, int32_t v) {
    sel_arg* a = &m->args[m->argc++];
    a->type = SEL_ARG_INT;
    a->v.i = v;
}

static void evt_uint(sel_message* m, uint32_t v) {
    sel_arg* a = &m->args[m->argc++];
    a->type = SEL_ARG_UINT;
    a->v.u = v;
}

static void evt_tags(sel_message* m, const sel_tag* t) {
    sel_arg* a = &m->args[m->argc++];
    a->type = SEL_ARG_TAGS;
    a->v.t = t;
}

static void emit(sel_engine* e, const sel_message* m) {
    if (!sel_spsc_push(&e->evt_q, m)) {
        e->host->log(e->host, SEL_LOG_WARN, "event queue full, event dropped");
    }
}

static void emit_state(sel_engine* e) {
    sel_message m;
    evt_init(&m, SEL_EVT_STATE);
    evt_str(&m, sel_play_state_name(e->state.play));
    emit(e, &m);
}

static void emit_volume(sel_engine* e) {
    sel_message m;
    evt_init(&m, SEL_EVT_VOLUME);
    evt_int(&m, e->state.volume);
    emit(e, &m);
}

static void emit_output(sel_engine* e) {
    sel_message m;
    evt_init(&m, SEL_EVT_OUTPUT_CHANGED);
    evt_str(&m, e->state.output);
    emit(e, &m);
}

static void emit_option(sel_engine* e, uint32_t idx) {
    sel_message m;
    evt_init(&m, SEL_EVT_OPTIONS_CHANGED);
    evt_str(&m, k_option_names[idx]);
    evt_str(&m, e->state.options[idx]);
    emit(e, &m);
}

/* Emits now, or marks for the END flush while a batch is open. */
static void changed(sel_engine* e, uint32_t bit) {
    if (e->in_batch) {
        e->pending |= bit;
        return;
    }
    if (bit == PENDING_STATE) {
        emit_state(e);
    } else if (bit == PENDING_VOLUME) {
        emit_volume(e);
    } else if (bit == PENDING_OUTPUT) {
        emit_output(e);
    } else {
        uint32_t i;
        for (i = 0; i < (uint32_t)SEL_OPTION_COUNT; i++) {
            if (bit == (PENDING_OPTION_BASE << i)) {
                emit_option(e, i);
            }
        }
    }
}

static void flush_pending(sel_engine* e) {
    uint32_t i;
    uint32_t pending = e->pending;
    e->pending = 0;
    if (pending & PENDING_STATE) {
        emit_state(e);
    }
    if (pending & PENDING_VOLUME) {
        emit_volume(e);
    }
    if (pending & PENDING_OUTPUT) {
        emit_output(e);
    }
    for (i = 0; i < (uint32_t)SEL_OPTION_COUNT; i++) {
        if (pending & (PENDING_OPTION_BASE << i)) {
            emit_option(e, i);
        }
    }
}

static void copy_str(char* dst, size_t cap, const char* src) {
    size_t n = src ? strlen(src) : 0;
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(dst, src ? src : "", n);
    dst[n] = '\0';
}

void sel_engine_report_error(sel_engine* e, const char* code, const char* detail) {
    sel_message m;
    char* slot = e->error_detail[e->error_slot++ & 7u];
    evt_init(&m, SEL_EVT_ERROR);
    evt_str(&m, code);
    if (detail) {
        copy_str(slot, SEL_ERROR_DETAIL_MAX, detail);
        evt_str(&m, slot);
    }
    emit(e, &m); /* errors are never deferred */
}

static void emit_log(sel_engine* e, const char* a, const char* b) {
    sel_message m;
    char* slot = e->log_line[e->log_slot++ & 7u];
    snprintf(slot, SEL_LOG_LINE_MAX, "%s %s", a, b ? b : "");
    evt_init(&m, SEL_EVT_LOG);
    evt_str(&m, slot);
    emit(e, &m);
}

/* ---- handlers -------------------------------------------------------- */

static void set_play(sel_engine* e, sel_play_state s) {
    if (e->state.play != s) {
        e->state.play = s;
        changed(e, PENDING_STATE);
    }
}

static void cmd_PLAY(sel_engine* e, const sel_message* m) {
    (void)m;
    set_play(e, SEL_PLAYING);
}

static void cmd_PAUSE(sel_engine* e, const sel_message* m) {
    (void)m;
    if (e->state.play == SEL_PLAYING) {
        set_play(e, SEL_PAUSED);
    } else if (e->state.play == SEL_PAUSED) {
        set_play(e, SEL_PLAYING);
    }
}

static void cmd_PAUSE_PLAYBACK(sel_engine* e, const sel_message* m) {
    const char* v = m->argc ? m->args[0].v.s : "";
    if (strcmp(v, "on") == 0) {
        if (e->state.play == SEL_PLAYING) {
            set_play(e, SEL_PAUSED);
        }
    } else if (strcmp(v, "off") == 0) {
        if (e->state.play == SEL_PAUSED) {
            set_play(e, SEL_PLAYING);
        }
    } else {
        sel_engine_report_error(e, "argument_type", "PAUSE_PLAYBACK");
    }
}

static void cmd_STOP(sel_engine* e, const sel_message* m) {
    (void)m;
    set_play(e, SEL_STOPPED);
}

static void cmd_VOLUME(sel_engine* e, const sel_message* m) {
    const sel_arg* a = &m->args[0];
    int32_t v = (a->flags & SEL_ARG_RELATIVE) ? e->state.volume + a->v.i : a->v.i;
    if (v < 0) {
        v = 0;
    }
    if (v > 100) {
        v = 100;
    }
    e->state.volume = v;
    changed(e, PENDING_VOLUME);
}

static void cmd_OUTPUT(sel_engine* e, const sel_message* m) {
    copy_str(e->state.output, SEL_OUTPUT_NAME_MAX, m->args[0].v.s);
    changed(e, PENDING_OUTPUT);
}

static void cmd_OPTION(sel_engine* e, const sel_message* m) {
    uint32_t i;
    for (i = 0; i < (uint32_t)SEL_OPTION_COUNT; i++) {
        if (strcmp(k_option_names[i], m->args[0].v.s) == 0) {
            copy_str(e->state.options[i], SEL_OPTION_VALUE_MAX, m->args[1].v.s);
            changed(e, PENDING_OPTION_BASE << i);
            return;
        }
    }
    sel_engine_report_error(e, "unknown_option", m->args[0].v.s);
}

static void cmd_COMMANDS(sel_engine* e, const sel_message* m) {
    uint32_t i;
    (void)m;
    for (i = 0; i < SEL_CMD_COUNT; i++) {
        emit_log(e, sel_cmd_name(i), sel_cmd_args(i));
    }
}

static void cmd_EVENTS(sel_engine* e, const sel_message* m) {
    uint32_t i;
    (void)m;
    for (i = 0; i < SEL_EVT_COUNT; i++) {
        emit_log(e, sel_evt_name(i), sel_evt_args(i));
    }
}

static void cmd_TAGTYPES(sel_engine* e, const sel_message* m) {
    uint32_t count, i;
    const sel_tag_info* table = sel_tag_table(&count);
    (void)m;
    for (i = 0; i < count; i++) {
        if ((table[i].tag & 0xF000u) == 0x8000u) {
            emit_log(e, table[i].name, NULL);
        }
    }
}

static void cmd_BEGIN(sel_engine* e, const sel_message* m) {
    (void)m;
    e->in_batch = 1;
}

static void cmd_END(sel_engine* e, const sel_message* m) {
    (void)m;
    e->in_batch = 0;
    flush_pending(e);
}

/* ---- songs ----------------------------------------------------------- */

static void emit_position(sel_engine* e) {
    sel_message m;
    uint32_t seconds = e->song_frames / e->rate;
    evt_init(&m, SEL_EVT_POSITION);
    evt_uint(&m, e->subsong);
    evt_uint(&m, e->ticks);
    evt_uint(&m, seconds);
    evt_uint(&m, e->song_frames);
    e->pos_emitted = seconds;
    emit(e, &m);
}

static void close_song(sel_engine* e) {
    if (e->rp_state != NULL) {
        e->rp_close(e->rp_state);
        e->rp_state = NULL;
    }
    if (e->song_data != NULL) {
        e->host->mem_free(e->host, e->song_data);
        e->song_data = NULL;
    }
    e->frames_left = 0;
    e->ended = 0;
    e->last_tick = 0;
}

/* Fresh mixer and scheduler, then the replayer's start. */
static int start_subsong(sel_engine* e, uint32_t subsong) {
    sel_mixer_init(&e->mixer, e->rate, e->rp_info.voice_count, e->resampler);
    sel_scheduler_init(&e->sched, e->rp_info.timebase_hz, e->rate);
    e->frames_left = 0;
    e->ticks = 0;
    e->song_frames = 0;
    e->ended = 0;
    e->last_tick = 0;
    e->subsong = subsong;
    return e->rp_start(e->rp_state, subsong, e->rate, sel_mixer_voices(&e->mixer));
}

/* Reads the whole file into host memory; NULL when it cannot be read. */
static uint8_t* read_song(sel_engine* e, const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    long len;
    uint8_t* buf;
    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = e->host->mem_alloc(e->host, (size_t)len + 1u, SEL_MEM_FAST);
    if (buf == NULL || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        if (buf != NULL) {
            e->host->mem_free(e->host, buf);
        }
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size = (size_t)len;
    return buf;
}

/* The replayer hook list that claims the data: first YES, else first UNSURE. */
static const sel_tag* find_replayer(const sel_engine* e, const uint8_t* data, size_t size,
                                    const char* path) {
    const sel_tag* best = NULL;
    uint32_t i, n;
    if (e->registry_count == NULL || e->registry_get == NULL) {
        return NULL;
    }
    n = e->registry_count();
    for (i = 0; i < n; i++) {
        const sel_tag* desc = e->registry_get(i);
        const sel_tag* hooks;
        sel_rp_probe_fn probe;
        int r;
        if (sel_tag_find(desc, SEL_PLUGIN_KIND, 0) != SEL_PLUGIN_KIND_REPLAYER) {
            continue;
        }
        hooks = (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0);
        probe = hooks ? (sel_rp_probe_fn)sel_tag_find(hooks, SEL_RP_PROBE, 0) : NULL;
        if (probe == NULL) {
            continue;
        }
        r = probe(data, size, path);
        if (r == SEL_PROBE_YES) {
            return hooks;
        }
        if (r == SEL_PROBE_UNSURE && best == NULL) {
            best = hooks;
        }
    }
    return best;
}

static void cmd_OPEN(sel_engine* e, const sel_message* m) {
    const char* path = m->args[0].v.s;
    sel_song_source src;
    const sel_tag* hooks;
    sel_rp_open_fn open;
    size_t size = 0;
    uint8_t* data;
    sel_message ev;

    close_song(e);
    data = read_song(e, path, &size);
    if (data == NULL) {
        sel_engine_report_error(e, "open_failed", path);
        return;
    }
    hooks = size ? find_replayer(e, data, size, path) : NULL;
    if (hooks == NULL) {
        e->host->mem_free(e->host, data);
        sel_engine_report_error(e, "no_replayer", path);
        return;
    }
    if (sel_tag_find(hooks, SEL_RP_KIND, 0) != SEL_RP_KIND_VOICE) {
        e->host->mem_free(e->host, data);
        sel_engine_report_error(e, "unsupported_kind", path);
        return;
    }
    open = (sel_rp_open_fn)sel_tag_find(hooks, SEL_RP_OPEN, 0);
    e->rp_close = (sel_rp_close_fn)sel_tag_find(hooks, SEL_RP_CLOSE, 0);
    e->rp_start = (sel_rp_start_fn)sel_tag_find(hooks, SEL_RP_START, 0);
    e->rp_tick = (sel_rp_tick_fn)sel_tag_find(hooks, SEL_RP_TICK, 0);
    e->rp_metadata = (sel_rp_metadata_fn)sel_tag_find(hooks, SEL_RP_METADATA, 0);
    if (!open || !e->rp_close || !e->rp_start || !e->rp_tick) {
        e->host->mem_free(e->host, data);
        sel_engine_report_error(e, "replayer_incomplete", path);
        return;
    }
    src.data = data;
    src.size = size;
    src.path = path;
    memset(&e->rp_info, 0, sizeof e->rp_info);
    e->rp_state = open(&src, e->host, &e->rp_info);
    if (e->rp_state == NULL) {
        e->host->mem_free(e->host, data);
        sel_engine_report_error(e, "open_failed", path);
        return;
    }
    e->song_data = data; /* the replayer reads it until close */
    if (!start_subsong(e, e->rp_info.default_subsong)) {
        close_song(e);
        sel_engine_report_error(e, "start_failed", path);
        return;
    }
    evt_init(&ev, SEL_EVT_SONG_OPENED);
    evt_tags(&ev, e->rp_metadata ? e->rp_metadata(e->rp_state) : NULL);
    emit(e, &ev);
    emit_position(e);
}

static void cmd_SUBSONG(sel_engine* e, const sel_message* m) {
    uint32_t n = m->args[0].v.u;
    if (e->rp_state == NULL) {
        sel_engine_report_error(e, "no_song", "SUBSONG");
        return;
    }
    if (n >= e->rp_info.subsong_count) {
        sel_engine_report_error(e, "range", "SUBSONG");
        return;
    }
    if (!start_subsong(e, n)) {
        sel_engine_report_error(e, "start_failed", "SUBSONG");
        return;
    }
    emit_position(e);
}

static void cmd_not_implemented(sel_engine* e, const sel_message* m) {
    sel_engine_report_error(e, "not_implemented", sel_cmd_name(m->id));
}

static void dispatch(sel_engine* e, const sel_message* m) {
    switch ((sel_cmd_id)m->id) {
#define SEL_CASE(id)                                                                          \
    case SEL_CMD_##id:                                                                        \
        cmd_##id(e, m);                                                                       \
        break;
        SEL_CMD_IMPLEMENTED(SEL_CASE)
#undef SEL_CASE
    default:
        cmd_not_implemented(e, m);
        break;
    }
}

int sel_engine_cmd_implemented(uint32_t cmd_id) {
    switch ((sel_cmd_id)cmd_id) {
#define SEL_CASE(id) case SEL_CMD_##id:
        SEL_CMD_IMPLEMENTED(SEL_CASE)
#undef SEL_CASE
        return 1;
    default:
        return 0;
    }
}

/* ---- lifecycle ------------------------------------------------------- */

sel_engine* sel_engine_create(const sel_host* host, const sel_engine_config* cfg) {
    sel_engine* e = host->mem_alloc(host, sizeof *e, SEL_MEM_FAST);
    uint32_t cmd_depth = cfg && cfg->cmd_queue_depth ? cfg->cmd_queue_depth : 64;
    uint32_t evt_depth = cfg && cfg->evt_queue_depth ? cfg->evt_queue_depth : 256;
    if (e == NULL) {
        return NULL;
    }
    memset(e, 0, sizeof *e);
    e->host = host;
    e->state.volume = 100;
    e->rate = 44100;
    e->resampler = SEL_MIX_RESAMPLE_LINEAR;
    if (cfg) {
        e->registry_count = cfg->registry_count;
        e->registry_get = cfg->registry_get;
    }
    copy_str(e->state.output, SEL_OUTPUT_NAME_MAX, "null");
    if (!sel_spsc_init(&e->cmd_q, host, cmd_depth) || !sel_spsc_init(&e->evt_q, host, evt_depth)) {
        sel_engine_destroy(e);
        return NULL;
    }
    return e;
}

void sel_engine_destroy(sel_engine* e) {
    if (e) {
        close_song(e);
        sel_spsc_free(&e->cmd_q, e->host);
        sel_spsc_free(&e->evt_q, e->host);
        e->host->mem_free(e->host, e);
    }
}

int sel_engine_send(sel_engine* e, const sel_message* cmd) {
    return sel_spsc_push(&e->cmd_q, cmd);
}

uint32_t sel_engine_pump(sel_engine* e, sel_event_sink sink, void* ctx) {
    sel_message m;
    uint32_t delivered = 0;
    while (sel_spsc_pop(&e->cmd_q, &m)) {
        if (m.kind == SEL_MSG_CMD && m.id < SEL_CMD_COUNT) {
            dispatch(e, &m);
        }
    }
    if (e->ended) {
        sel_message ev;
        e->ended = 0;
        evt_init(&ev, SEL_EVT_SONG_ENDED);
        emit(e, &ev);
        set_play(e, SEL_STOPPED);
    }
    if (e->rp_state != NULL && e->song_frames / e->rate != e->pos_emitted) {
        emit_position(e);
    }
    while (sel_spsc_pop(&e->evt_q, &m)) {
        if (sink) {
            sink(ctx, &m);
        }
        delivered++;
    }
    return delivered;
}

const sel_state* sel_engine_state(const sel_engine* e) {
    return &e->state;
}

void sel_engine_set_mix(sel_engine* e, uint32_t rate, uint32_t resampler) {
    if (rate) {
        e->rate = rate;
    }
    e->resampler = resampler;
}

void sel_engine_stats_get(const sel_engine* e, sel_engine_stats* out) {
    out->ticks = e->ticks;
    out->voice_ops = e->mixer.voice_ops;
    out->mixed_frames = e->mixer.mixed_frames;
}

/* Mixes the open song: the scheduler turns each tick's length into frames,
 * the mixer fills them. The tick that reports ENDED plays out, then the
 * song is flagged ended for the next pump (events are produced on the
 * command side only, keeping the event queue single-producer). */
uint32_t sel_engine_render(sel_engine* e, int16_t* stereo, uint32_t frames) {
    uint32_t done = 0;
    e->render_calls++;
    if (stereo == NULL || frames == 0) {
        return frames;
    }
    if (e->rp_state != NULL && e->state.play == SEL_PLAYING) {
        while (done < frames && !e->ended) {
            uint32_t n;
            if (e->frames_left == 0) {
                sel_tick_result r;
                e->rp_tick(e->rp_state, &r);
                e->ticks++;
                e->frames_left = sel_scheduler_frames(&e->sched, r.length);
                if (r.flags & SEL_TICK_ENDED) {
                    e->last_tick = 1;
                }
            }
            n = frames - done < e->frames_left ? frames - done : e->frames_left;
            sel_mixer_mix(&e->mixer, stereo + (size_t)done * 2u, n);
            e->frames_left -= n;
            e->song_frames += n;
            done += n;
            if (e->frames_left == 0 && e->last_tick) {
                e->last_tick = 0;
                e->ended = 1;
            }
        }
        if (e->state.volume != 100) {
            uint32_t i;
            for (i = 0; i < done * 2u; i++) {
                stereo[i] = (int16_t)(((int32_t)stereo[i] * e->state.volume) / 100);
            }
        }
    }
    if (done < frames) {
        memset(stereo + (size_t)done * 2u, 0, (size_t)(frames - done) * 2u * sizeof(int16_t));
    }
    return frames;
}

uint32_t sel_engine_render_calls(const sel_engine* e) {
    return e->render_calls;
}
