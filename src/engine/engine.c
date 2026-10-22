/* Engine shell (M0a): transport state, volume, output name, options,
 * batches. Every verb in commands.def reaches dispatch; the ones outside
 * SEL_CMD_IMPLEMENTED answer ERROR not_implemented and are counted by the
 * ratchet test.
 *
 * Songs live in two replayer slots (design doc section 3). The pump thread
 * prepares the idle slot (file read, RP_OPEN, RP_START, every allocation),
 * publishes its index through the swap ring, and the audio thread switches
 * to it at the start of its next render call and hands the old slot back
 * through the return ring; the pump thread then calls RP_CLOSE and frees.
 * The audio thread therefore never opens, closes or allocates a song. The
 * rings are sel_spsc instances of their own: the command ring has the pump
 * thread as its consumer, these two have the audio thread on one end. */
#include <selecta/engine.h>

#include <selecta/plugin.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>

#include "mixer.h"
#include "scheduler.h"
#include "spsc.h"
#include "thread.h"

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

#define SEL_SLOT_COUNT 2u
#define SEL_SLOT_NONE 0xFFFFFFFFu
#define SEL_SUBSONG_DEFAULT 0xFFFFFFFFu
#define SEL_SWAP_RING_DEPTH 4u
/* ids of the internal ring messages (not commands, not in commands.def) */
#define SEL_RING_SWAP 1u
#define SEL_RING_RETURN 2u
#define SEL_DEFERRED_PATH_MAX 1024u
#define SEL_ERROR_DETAIL_MAX 64
#define SEL_LOG_LINE_MAX 96

/* Events coalesced inside a batch: the latest value wins, emitted at END. */
#define PENDING_STATE (1u << 0)
#define PENDING_VOLUME (1u << 1)
#define PENDING_OUTPUT (1u << 2)
#define PENDING_OPTION_BASE (1u << 3) /* one bit per option above this */

typedef struct sel_slot {
    uint32_t busy; /* pump side: the slot holds a song until RP_CLOSE ran */
    void* rp_state;
    sel_rp_close_fn rp_close;
    sel_rp_start_fn rp_start;
    sel_rp_tick_fn rp_tick;
    sel_rp_metadata_fn rp_metadata;
    sel_rp_info rp_info;
    uint8_t* song_data;
    char* path; /* kept for SUBSONG, which reopens the file in the idle slot */
    uint32_t subsong;
    sel_mixer mixer;
    sel_scheduler sched;
    uint32_t frames_left; /* frames of the current tick still to mix */
    uint32_t ticks;
    uint32_t song_frames; /* frames mixed since the subsong started */
    uint32_t pos_emitted; /* whole seconds last reported */
    uint32_t last_tick;   /* the current tick carries SEL_TICK_ENDED */
    uint32_t ended;       /* set by render, reported by the next pump */
} sel_slot;

struct sel_engine {
    const sel_host* host;
    sel_state state;
    sel_spsc cmd_q;
    sel_spsc evt_q;
    uint32_t in_batch;
    uint32_t pending;
    uint32_t render_calls;
    SEL_THREAD_FIELDS
    uint32_t error_slot;
    char error_detail[8][SEL_ERROR_DETAIL_MAX];
    uint32_t log_slot;
    char log_line[8][SEL_LOG_LINE_MAX];
    uint32_t (*registry_count)(void);
    const sel_tag* (*registry_get)(uint32_t index);
    uint32_t rate;
    uint32_t resampler;
    sel_slot slot[SEL_SLOT_COUNT];
    sel_spsc swap_q;                /* pump -> audio: slot index to switch to */
    sel_spsc ret_q;                 /* audio -> pump: slot index to close */
    uint32_t cur;                   /* pump: newest published slot, or NONE */
    volatile uint32_t audio_slot;   /* audio writes, pump reads: slot being mixed */
    uint32_t act;                   /* audio only: same value, private copy */
    /* an OPEN or SUBSONG that found both slots busy, retried by the next pump */
    uint32_t deferred;
    uint32_t deferred_subsong;
    char deferred_path[SEL_DEFERRED_PATH_MAX];
    char subsong_path[SEL_DEFERRED_PATH_MAX];
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

static void emit_position(sel_engine* e, sel_slot* sl) {
    sel_message m;
    uint32_t seconds = sl->song_frames / e->rate;
    evt_init(&m, SEL_EVT_POSITION);
    evt_uint(&m, sl->subsong);
    evt_uint(&m, sl->ticks);
    evt_uint(&m, seconds);
    evt_uint(&m, sl->song_frames);
    sl->pos_emitted = seconds;
    emit(e, &m);
}

/* Pump thread only: RP_CLOSE and free; the audio thread no longer mixes the slot. */
static void close_slot(sel_engine* e, sel_slot* sl) {
    if (sl->rp_state != NULL) {
        sl->rp_close(sl->rp_state);
        sl->rp_state = NULL;
    }
    if (sl->song_data != NULL) {
        e->host->mem_free(e->host, sl->song_data);
        sl->song_data = NULL;
    }
    if (sl->path != NULL) {
        e->host->mem_free(e->host, sl->path);
        sl->path = NULL;
    }
    sl->frames_left = 0;
    sl->ended = 0;
    sl->last_tick = 0;
    sl->busy = 0;
}

/* Fresh mixer and scheduler, then the replayer's start. */
static int start_subsong(sel_engine* e, sel_slot* sl, uint32_t subsong) {
    sel_mixer_init(&sl->mixer, e->rate, sl->rp_info.voice_count, e->resampler);
    sel_scheduler_init(&sl->sched, sl->rp_info.timebase_hz, e->rate);
    sl->frames_left = 0;
    sl->ticks = 0;
    sl->song_frames = 0;
    sl->pos_emitted = 0;
    sl->ended = 0;
    sl->last_tick = 0;
    sl->subsong = subsong;
    return sl->rp_start(sl->rp_state, subsong, e->rate, sel_mixer_voices(&sl->mixer));
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

/* Hands finished slots back: every index the audio thread returned gets its
 * RP_CLOSE here, on the pump thread. */
static void reclaim_slots(sel_engine* e) {
    sel_message r;
    while (sel_spsc_pop(&e->ret_q, &r)) {
        if (r.args[0].v.u < SEL_SLOT_COUNT) {
            close_slot(e, &e->slot[r.args[0].v.u]);
        }
    }
}

static int find_free_slot(const sel_engine* e, uint32_t* idx) {
    uint32_t i;
    for (i = 0; i < SEL_SLOT_COUNT; i++) {
        if (!e->slot[i].busy) {
            *idx = i;
            return 1;
        }
    }
    return 0;
}

/* Prepares the idle slot `idx` with the song at `path` (all allocation, file
 * read, RP_OPEN and RP_START happen here) and publishes it through the swap
 * ring. On failure the slot stays free and the error is reported. */
static int prepare_slot(sel_engine* e, uint32_t idx, const char* path, uint32_t subsong,
                        const char* err_detail) {
    sel_slot* sl = &e->slot[idx];
    sel_song_source src;
    const sel_tag* hooks;
    sel_rp_open_fn open;
    size_t size = 0;
    uint8_t* data;
    sel_message sw;

    data = read_song(e, path, &size);
    if (data == NULL) {
        sel_engine_report_error(e, "open_failed", err_detail);
        return 0;
    }
    hooks = size ? find_replayer(e, data, size, path) : NULL;
    if (hooks == NULL) {
        e->host->mem_free(e->host, data);
        sel_engine_report_error(e, "no_replayer", err_detail);
        return 0;
    }
    if (sel_tag_find(hooks, SEL_RP_KIND, 0) != SEL_RP_KIND_VOICE) {
        e->host->mem_free(e->host, data);
        sel_engine_report_error(e, "unsupported_kind", err_detail);
        return 0;
    }
    open = (sel_rp_open_fn)sel_tag_find(hooks, SEL_RP_OPEN, 0);
    sl->rp_close = (sel_rp_close_fn)sel_tag_find(hooks, SEL_RP_CLOSE, 0);
    sl->rp_start = (sel_rp_start_fn)sel_tag_find(hooks, SEL_RP_START, 0);
    sl->rp_tick = (sel_rp_tick_fn)sel_tag_find(hooks, SEL_RP_TICK, 0);
    sl->rp_metadata = (sel_rp_metadata_fn)sel_tag_find(hooks, SEL_RP_METADATA, 0);
    if (!open || !sl->rp_close || !sl->rp_start || !sl->rp_tick) {
        e->host->mem_free(e->host, data);
        sel_engine_report_error(e, "replayer_incomplete", err_detail);
        return 0;
    }
    src.data = data;
    src.size = size;
    src.path = path;
    memset(&sl->rp_info, 0, sizeof sl->rp_info);
    sl->rp_state = open(&src, e->host, &sl->rp_info);
    if (sl->rp_state == NULL) {
        e->host->mem_free(e->host, data);
        sel_engine_report_error(e, "open_failed", err_detail);
        return 0;
    }
    sl->song_data = data; /* the replayer reads it until close */
    sl->path = e->host->mem_alloc(e->host, strlen(path) + 1u, SEL_MEM_FAST);
    if (sl->path != NULL) {
        memcpy(sl->path, path, strlen(path) + 1u);
    }
    if (sl->path == NULL ||
        !start_subsong(e, sl, subsong == SEL_SUBSONG_DEFAULT ? sl->rp_info.default_subsong : subsong)) {
        close_slot(e, sl);
        sel_engine_report_error(e, "start_failed", err_detail);
        return 0;
    }
    sl->busy = 1;
    memset(&sw, 0, sizeof sw);
    sw.kind = SEL_MSG_CMD;
    sw.id = SEL_RING_SWAP;
    sw.argc = 1;
    sw.args[0].type = SEL_ARG_UINT;
    sw.args[0].v.u = idx;
    if (!sel_spsc_push(&e->swap_q, &sw)) {
        close_slot(e, sl);
        sel_engine_report_error(e, "start_failed", err_detail);
        return 0;
    }
    return 1;
}

/* One OPEN (is_open) or SUBSONG request. Both slots busy means the audio thread
 * has not yet returned the old one: the request waits for the next pump. */
static void begin_song(sel_engine* e, uint32_t is_open, const char* path, uint32_t subsong) {
    uint32_t idx;
    sel_message ev;
    reclaim_slots(e);
    if (!find_free_slot(e, &idx)) {
        size_t n = strlen(path);
        if (n >= SEL_DEFERRED_PATH_MAX) {
            sel_engine_report_error(e, "open_failed", is_open ? path : "SUBSONG");
            return;
        }
        memcpy(e->deferred_path, path, n + 1u);
        e->deferred = is_open ? 1u : 2u;
        e->deferred_subsong = subsong;
        return;
    }
    e->deferred = 0;
    if (!prepare_slot(e, idx, path, subsong, is_open ? path : "SUBSONG")) {
        return;
    }
    e->cur = idx;
    if (is_open) {
        evt_init(&ev, SEL_EVT_SONG_OPENED);
        evt_tags(&ev, e->slot[idx].rp_metadata ? e->slot[idx].rp_metadata(e->slot[idx].rp_state) : NULL);
        emit(e, &ev);
    }
    emit_position(e, &e->slot[idx]);
}

static void cmd_OPEN(sel_engine* e, const sel_message* m) {
    begin_song(e, 1u, m->args[0].v.s, SEL_SUBSONG_DEFAULT);
}

static void cmd_SUBSONG(sel_engine* e, const sel_message* m) {
    uint32_t n = m->args[0].v.u;
    sel_slot* cur = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    if (cur == NULL || cur->rp_state == NULL) {
        sel_engine_report_error(e, "no_song", "SUBSONG");
        return;
    }
    if (n >= cur->rp_info.subsong_count) {
        sel_engine_report_error(e, "range", "SUBSONG");
        return;
    }
    /* the path lives in the slot that may be closed by reclaim: copy it first */
    if (strlen(cur->path) >= SEL_DEFERRED_PATH_MAX) {
        sel_engine_report_error(e, "start_failed", "SUBSONG");
        return;
    }
    memcpy(e->subsong_path, cur->path, strlen(cur->path) + 1u);
    begin_song(e, 0u, e->subsong_path, n);
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
    e->cur = SEL_SLOT_NONE;
    e->audio_slot = SEL_SLOT_NONE;
    e->act = SEL_SLOT_NONE;
    if (!sel_spsc_init(&e->cmd_q, host, cmd_depth) || !sel_spsc_init(&e->evt_q, host, evt_depth) ||
        !sel_spsc_init(&e->swap_q, host, SEL_SWAP_RING_DEPTH) ||
        !sel_spsc_init(&e->ret_q, host, SEL_SWAP_RING_DEPTH)) {
        sel_engine_destroy(e);
        return NULL;
    }
    return e;
}

void sel_engine_mark_audio_thread(sel_engine* e) {
    SEL_THREAD_MARK_AUDIO(e);
}

void sel_engine_destroy(sel_engine* e) {
    if (e) {
        SEL_THREAD_NOT_AUDIO(e);
        uint32_t i;
        for (i = 0; i < SEL_SLOT_COUNT; i++) {
            close_slot(e, &e->slot[i]);
        }
        sel_spsc_free(&e->cmd_q, e->host);
        sel_spsc_free(&e->evt_q, e->host);
        sel_spsc_free(&e->swap_q, e->host);
        sel_spsc_free(&e->ret_q, e->host);
        e->host->mem_free(e->host, e);
    }
}

int sel_engine_send(sel_engine* e, const sel_message* cmd) {
    SEL_THREAD_NOT_AUDIO(e);
    return sel_spsc_push(&e->cmd_q, cmd);
}

uint32_t sel_engine_pump(sel_engine* e, sel_event_sink sink, void* ctx) {
    sel_message m;
    uint32_t delivered = 0;
    SEL_THREAD_NOT_AUDIO(e);
    while (sel_spsc_pop(&e->cmd_q, &m)) {
        if (m.kind == SEL_MSG_CMD && m.id < SEL_CMD_COUNT) {
            dispatch(e, &m);
        }
    }
    reclaim_slots(e);
    if (e->deferred) {
        begin_song(e, e->deferred == 1u, e->deferred_path, e->deferred_subsong);
    }
    if (e->cur != SEL_SLOT_NONE) {
        sel_slot* sl = &e->slot[e->cur];
        if (sl->ended) {
            sel_message ev;
            sl->ended = 0;
            evt_init(&ev, SEL_EVT_SONG_ENDED);
            emit(e, &ev);
            set_play(e, SEL_STOPPED);
        }
        if (sl->rp_state != NULL && sl->song_frames / e->rate != sl->pos_emitted) {
            emit_position(e, sl);
        }
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
    const sel_slot* sl = e->audio_slot < SEL_SLOT_COUNT ? &e->slot[e->audio_slot] : NULL;
    out->ticks = sl ? sl->ticks : 0;
    out->voice_ops = sl ? sl->mixer.voice_ops : 0;
    out->mixed_frames = sl ? sl->mixer.mixed_frames : 0;
}

/* Mixes the open song: the scheduler turns each tick's length into frames,
 * the mixer fills them. The tick that reports ENDED plays out, then the
 * song is flagged ended for the next pump (events are produced on the
 * command side only, keeping the event queue single-producer). */
uint32_t sel_engine_render(sel_engine* e, int16_t* stereo, uint32_t frames) {
    uint32_t done = 0;
    SEL_THREAD_AUDIO(e);
    e->render_calls++;
    if (stereo == NULL || frames == 0) {
        return frames;
    }
    sel_message sw;
    sel_slot* sl;
    while (sel_spsc_pop(&e->swap_q, &sw)) {
        if (e->act < SEL_SLOT_COUNT) {
            sel_message back;
            memset(&back, 0, sizeof back);
            back.kind = SEL_MSG_CMD;
            back.id = SEL_RING_RETURN;
            back.argc = 1;
            back.args[0].type = SEL_ARG_UINT;
            back.args[0].v.u = e->act;
            sel_spsc_push(&e->ret_q, &back);
        }
        e->act = sw.args[0].v.u;
        e->audio_slot = e->act;
    }
    sl = e->act < SEL_SLOT_COUNT ? &e->slot[e->act] : NULL;
    if (sl != NULL && e->state.play == SEL_PLAYING) {
        while (done < frames && !sl->ended) {
            uint32_t n;
            if (sl->frames_left == 0) {
                sel_tick_result r;
                sl->rp_tick(sl->rp_state, &r);
                sl->ticks++;
                sl->frames_left = sel_scheduler_frames(&sl->sched, r.length);
                if (r.flags & SEL_TICK_ENDED) {
                    sl->last_tick = 1;
                }
            }
            n = frames - done < sl->frames_left ? frames - done : sl->frames_left;
            sel_mixer_mix(&sl->mixer, stereo + (size_t)done * 2u, n);
            sl->frames_left -= n;
            sl->song_frames += n;
            done += n;
            if (sl->frames_left == 0 && sl->last_tick) {
                sl->last_tick = 0;
                sl->ended = 1;
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
