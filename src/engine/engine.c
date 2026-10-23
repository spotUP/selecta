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
#include "history.h"
#include "queue.h"
#include "playlist/playlist.h"
#include "shuffle.h"
#include "local.h"
#include "prefs.h"
#include "view.h"
#include "scheduler.h"
#include "spsc.h"
#include "thread.h"

#include <stdio.h>
#include <string.h>

/* The verbs with real behaviour in M0a. Adding a row here is how a later
 * plan lowers tests/ratchet/unimplemented.txt. */
#define SEL_CMD_IMPLEMENTED(X)                                                                \
    X(PREF_GET)                                                                               \
    X(PREF_SET)                                                                               \
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
    X(SUBSONG)                                                                                \
    X(NEXT)                                                                                   \
    X(PREV)                                                                                   \
    X(SEEK)                                                                                   \
    X(QUEUE_ADD)                                                                              \
    X(QUEUE_REMOVE)                                                                           \
    X(QUEUE_CLEAR)                                                                            \
    X(QUEUE_MOVE)                                                                             \
    X(QUEUE_SWAP)                                                                             \
    X(UNDO)                                                                                   \
    X(REDO)                                                                                   \
    X(QUEUE_SHUFFLE)                                                                          \
    X(RESHUFFLE)                                                                              \
    X(SHUFFLE)                                                                                \
    X(SCOPE)                                                                                  \
    X(NEXT_ALBUM)                                                                             \
    X(PREV_ALBUM)                                                                             \
    X(PLAYLIST_LOAD)                                                                          \
    X(PLAYLIST_SAVE)                                                                          \
    X(QUEUE_PLAY_NEXT)                                                                        \
    X(LIBRARY_BROWSE)                                                                         \
    X(VIEW_SHOW)                                                                              \
    X(VIEW_CURSOR)                                                                            \
    X(VIEW_ACTIVATE)                                                                          \
    X(VIEW_WINDOW)                                                                            \
    X(VIEW_STYLE)                                                                             \
    X(VIEW_GROUP)                                                                             \
    X(VIEW_EXPAND)                                                                            \
    X(VIEW_COLLAPSE)                                                                          \
    X(VIEW_DRILL)

#define SEL_SLOT_COUNT 2u
#define SEL_SLOT_NONE 0xFFFFFFFFu
#define SEL_SUBSONG_DEFAULT 0xFFFFFFFFu
/* kinds of song change, shared by begin_song and the deferred request */
#define SEL_BEGIN_OPEN 1u    /* new song: SONG_OPENED */
#define SEL_BEGIN_SUBSONG 2u /* other subsong of the open file: no event */
#define SEL_BEGIN_RESTART 3u /* same song from the start: SEEKED 0 */
#define SEL_BEGIN_SEEK 4u    /* same song, restart-and-skip to a frame: SEEKED frame */
/* PREV inside the first three seconds goes to the previous entry, later restarts */
#define SEL_PREV_RESTART_MS 3000u
#define SEL_SWAP_RING_DEPTH 4u
#define SEL_SEEK_PATH_NATIVE 1u
#define SEL_SEEK_PATH_SKIP 2u
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
    sel_rp_seek_fn rp_seek;         /* optional: native seek, else restart-and-skip */
    sel_rp_position_fn rp_position; /* optional: order/row/pattern */
    uint32_t pos_order, pos_row, pos_pattern, pos_valid; /* audio side writes after each render */
    sel_rp_render_fn rp_render; /* RENDER kind: the replayer mixes, rp_tick is NULL */
    const char* rp_name;
    uint32_t rp_render_calls;
    uint32_t rendered_frames; /* frames the RENDER replayer produced this subsong */
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
    uint32_t reported;    /* pump side: SONG_ENDED was emitted for this song */
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
    uint32_t deferred_seek; /* target ms of a deferred SEL_BEGIN_SEEK */
    uint32_t seek_path;     /* SEL_SEEK_PATH_*: how the last SEEK got there */
    uint32_t seek_skipped;  /* frames the last restart-and-skip ran through */
    uint32_t songs_opened;  /* OPEN-kind song starts that succeeded (not seeks, restarts, subsongs) */
    char deferred_path[SEL_DEFERRED_PATH_MAX];
    char subsong_path[SEL_DEFERRED_PATH_MAX];
    sel_queue queue;
    sel_history history;
    uint32_t rng; /* shuffle generator state, 0: not seeded yet */
    /* random play order: the first order_done ids are played (the last one is
     * playing), the rest is the shuffled remainder. Valid for order_qver. */
    uint32_t* order;
    uint8_t* order_seen; /* scratch, one byte per queue position */
    uint32_t order_n;
    uint32_t order_done;
    uint32_t order_qver;
    /* PLAYLIST_LOAD / PLAYLIST_SAVE scratch, allocated once in create: the
     * codecs fill pl_entries and pl_text, a file is read whole into pl_file.
     * pl_text and pl_file hold queue text bytes + 16. */
    sel_pl_entry* pl_entries;
    char* pl_text;
    char* pl_file;
    uint32_t q_playing; /* id of the playing queue entry, 0: none */
    uint32_t q_hint;    /* position NEXT and PLAY start from while q_playing is 0 */
    uint32_t song_over; /* the open song ended or the queue ran out: PLAY restarts the queue */
    sel_browse browse;  /* the listing LIBRARY_BROWSE fills */
    uint32_t lib_slot;
    sel_tag lib_tags[4][3];                  /* LIBRARY_RESULT tags, a ring like error_detail */
    char lib_path[4][SEL_BROWSE_PATH_MAX];
    sel_view view;
    sel_prefs prefs;
    char config_dir[256]; /* "": prefs.conf is not read or written */
    sel_tag view_tags[4][5]; /* VIEW_CHANGED / VIEW_ROWS tags, a ring */
    uint32_t view_slot;
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

static void order_reset(sel_engine* e);

static void set_option(sel_engine* e, sel_option_id id, const char* value) {
    if (id == SEL_OPT_RANDOM && strcmp(value, "on") == 0) {
        order_reset(e);
    }
    copy_str(e->state.options[id], SEL_OPTION_VALUE_MAX, value);
    changed(e, PENDING_OPTION_BASE << (uint32_t)id);
}

static void cmd_OPTION(sel_engine* e, const sel_message* m) {
    uint32_t i;
    for (i = 0; i < (uint32_t)SEL_OPTION_COUNT; i++) {
        if (strcmp(k_option_names[i], m->args[0].v.s) == 0) {
            set_option(e, (sel_option_id)i, m->args[1].v.s);
            return;
        }
    }
    sel_engine_report_error(e, "unknown_option", m->args[0].v.s);
}

/* ---- prefs ----------------------------------------------------------- */

static void emit_pref(sel_engine* e, int id) {
    sel_message m;
    evt_init(&m, SEL_EVT_PREFS_CHANGED);
    evt_str(&m, sel_prefs_key(id));
    evt_str(&m, e->prefs.value[id]);
    emit(e, &m);
}

/* The reply to PREF_GET is the PREFS_CHANGED event carrying the current value. */
static void cmd_PREF_GET(sel_engine* e, const sel_message* m) {
    int id = sel_prefs_find(m->args[0].v.s);
    if (id < 0) {
        sel_engine_report_error(e, "unknown_pref", m->args[0].v.s);
        return;
    }
    emit_pref(e, id);
}

static void cmd_PREF_SET(sel_engine* e, const sel_message* m) {
    int rc = sel_prefs_set(&e->prefs, m->args[0].v.s, m->args[1].v.s);
    if (rc == SEL_PREFS_SET_UNKNOWN) {
        sel_engine_report_error(e, "unknown_pref", m->args[0].v.s);
        return;
    }
    if (rc == SEL_PREFS_SET_BAD) {
        sel_engine_report_error(e, "bad_format", m->args[0].v.s);
        return;
    }
    if (e->config_dir[0] != '\0' && !sel_prefs_save(&e->prefs, e->config_dir)) {
        sel_engine_report_error(e, "prefs_save_failed", e->config_dir);
    }
    emit_pref(e, sel_prefs_find(m->args[0].v.s));
}

static void cmd_COMMANDS(sel_engine* e, const sel_message* m) {
    uint32_t i;
    (void)m;
    for (i = 0; i < SEL_CMD_COUNT; i++) {
        char fields[96];
        snprintf(fields, sizeof fields, "%s area=%s", sel_cmd_args(i), sel_cmd_area(i));
        emit_log(e, sel_cmd_name(i), fields);
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

/* RP_POSITION into the slot's display fields. Runs on the thread that owns
 * the replayer instance: the audio thread after each render call, or the
 * pump thread while the slot is still unpublished. The pump only reads the
 * copies, so it never touches the replayer while it renders. */
static void sample_position(sel_slot* sl) {
    sel_rp_position p;
    if (sl->rp_position != NULL && sl->rp_position(sl->rp_state, &p)) {
        sl->pos_order = p.order;
        sl->pos_row = p.row;
        sl->pos_pattern = p.pattern;
        sl->pos_valid = 1;
    }
}

static void emit_position(sel_engine* e, sel_slot* sl) {
    sel_message m;
    uint32_t seconds = sl->song_frames / e->rate;
    evt_init(&m, SEL_EVT_POSITION);
    evt_uint(&m, sl->subsong);
    evt_uint(&m, sl->ticks);
    evt_uint(&m, seconds);
    evt_uint(&m, sl->song_frames);
    if (sl->pos_valid) {
        evt_uint(&m, sl->pos_order);
        evt_uint(&m, sl->pos_row);
        evt_uint(&m, sl->pos_pattern);
    }
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
    sl->reported = 0;
    sl->last_tick = 0;
    sl->busy = 0;
}

/* Fresh mixer and scheduler, then the replayer's start. */
static int start_subsong(sel_engine* e, sel_slot* sl, uint32_t subsong) {
    sl->rp_render_calls = 0;
    sl->rendered_frames = 0;
    if (sl->rp_render != NULL) {
        sl->frames_left = 0;
        sl->ticks = 0;
        sl->song_frames = 0;
        sl->pos_emitted = 0;
        sl->ended = 0;
        sl->reported = 0;
        sl->last_tick = 0;
        sl->subsong = subsong;
        return sl->rp_start(sl->rp_state, subsong, e->rate, NULL);
    }
    sel_mixer_init(&sl->mixer, e->rate, sl->rp_info.voice_count, e->resampler);
    sel_scheduler_init(&sl->sched, sl->rp_info.timebase_hz, e->rate);
    sl->frames_left = 0;
    sl->ticks = 0;
    sl->song_frames = 0;
    sl->pos_emitted = 0;
    sl->ended = 0;
    sl->reported = 0;
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

/* Restart-and-skip (design: Seek): the fresh instance runs its ticks through the
 * scheduler without mixing until `target` frames have passed. The voice path is
 * deterministic, so the voices end up where a full render would have left them.
 * Pump thread; the slot is not yet published. Returns the frames skipped. */
static uint32_t skip_frames(sel_slot* sl, uint32_t target) {
    if (sl->rp_render != NULL) {
        /* A RENDER replayer without RP_SEEK has no voices to advance:
         * render and discard. */
        int16_t scratch[512u * 2u];
        while (sl->song_frames < target && !sl->ended) {
            uint32_t want = target - sl->song_frames < 512u ? target - sl->song_frames : 512u;
            uint32_t got = sl->rp_render(sl->rp_state, scratch, want);
            sl->song_frames += got;
            if (got < want) {
                sl->ended = 1;
            }
        }
        return sl->song_frames;
    }
    while (sl->song_frames < target && !sl->ended) {
        uint32_t n;
        if (sl->frames_left == 0) {
            sel_tick_result r;
            sl->rp_tick(sl->rp_state, &r);
            sl->ticks++;
            sl->frames_left = sel_scheduler_frames(&sl->sched, r.length);
            if (r.flags & (SEL_TICK_ENDED | SEL_TICK_LOOPED)) {
                sl->last_tick = 1;
            }
        }
        n = target - sl->song_frames < sl->frames_left ? target - sl->song_frames : sl->frames_left;
        sel_mixer_advance(&sl->mixer, n);
        sl->frames_left -= n;
        sl->song_frames += n;
        if (sl->frames_left == 0 && sl->last_tick) {
            sl->last_tick = 0;
            sl->ended = 1;
        }
    }
    return sl->song_frames;
}

/* Prepares the idle slot `idx` with the song at `path` (all allocation, file
 * read, RP_OPEN and RP_START happen here) and publishes it through the swap
 * ring. On failure the slot stays free and the error is reported. */
static int prepare_slot(sel_engine* e, uint32_t idx, const char* path, uint32_t subsong,
                        uint32_t skip_ms, const char* err_detail) {
    sel_slot* sl = &e->slot[idx];
    sel_song_source src;
    const sel_tag* hooks;
    sel_rp_open_fn open;
    size_t size = 0;
    uint8_t* data;
    sel_message sw;
    uintptr_t kind;

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
    kind = sel_tag_find(hooks, SEL_RP_KIND, 0);
    if (kind != SEL_RP_KIND_VOICE && kind != SEL_RP_KIND_RENDER) {
        e->host->mem_free(e->host, data);
        sel_engine_report_error(e, "unsupported_kind", err_detail);
        return 0;
    }
    open = (sel_rp_open_fn)sel_tag_find(hooks, SEL_RP_OPEN, 0);
    sl->rp_close = (sel_rp_close_fn)sel_tag_find(hooks, SEL_RP_CLOSE, 0);
    sl->rp_start = (sel_rp_start_fn)sel_tag_find(hooks, SEL_RP_START, 0);
    sl->rp_tick = (sel_rp_tick_fn)sel_tag_find(hooks, SEL_RP_TICK, 0);
    sl->rp_metadata = (sel_rp_metadata_fn)sel_tag_find(hooks, SEL_RP_METADATA, 0);
    sl->rp_render = kind == SEL_RP_KIND_RENDER ? (sel_rp_render_fn)sel_tag_find(hooks, SEL_RP_RENDER, 0) : NULL;
    sl->rp_name = (const char*)sel_tag_find(hooks, SEL_RP_NAME, 0);
    sl->rp_seek = (sel_rp_seek_fn)sel_tag_find(hooks, SEL_RP_SEEK, 0);
    sl->rp_position = (sel_rp_position_fn)sel_tag_find(hooks, SEL_RP_POSITION, 0);
    sl->pos_valid = 0;
    if (kind == SEL_RP_KIND_RENDER) {
        sl->rp_tick = NULL;
    }
    if (!open || !sl->rp_close || !sl->rp_start || (kind == SEL_RP_KIND_VOICE ? !sl->rp_tick : !sl->rp_render)) {
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
    if (skip_ms > 0) {
        /* The target is converted from ms to a frame once, here. */
        uint32_t target = (uint32_t)((uint64_t)skip_ms * e->rate / 1000u);
        if (sl->rp_seek != NULL && sl->rp_seek(sl->rp_state, skip_ms)) {
            sl->song_frames = target;
            e->seek_skipped = 0;
            e->seek_path = SEL_SEEK_PATH_NATIVE;
        } else {
            e->seek_skipped = skip_frames(sl, target);
            e->seek_path = SEL_SEEK_PATH_SKIP;
        }
    }
    sample_position(sl);
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

/* One song change (SEL_BEGIN_*). Both slots busy means the audio thread has
 * not yet returned the old one: the request waits for the next pump. Returns
 * 0 when the song could not be started (an ERROR was reported). */
static int begin_song(sel_engine* e, uint32_t kind, const char* path, uint32_t subsong, uint32_t seek_to) {
    uint32_t idx;
    sel_message ev;
    const char* detail = kind == SEL_BEGIN_OPEN     ? path
                         : kind == SEL_BEGIN_SUBSONG ? "SUBSONG"
                         : kind == SEL_BEGIN_SEEK    ? "SEEK"
                                                     : "PREV";
    reclaim_slots(e);
    if (!find_free_slot(e, &idx)) {
        size_t n = strlen(path);
        if (n >= SEL_DEFERRED_PATH_MAX) {
            sel_engine_report_error(e, "open_failed", detail);
            return 0;
        }
        memcpy(e->deferred_path, path, n + 1u);
        e->deferred = kind;
        e->deferred_subsong = subsong;
        e->deferred_seek = seek_to;
        e->song_over = 0;
        return 1;
    }
    e->deferred = 0;
    if (!prepare_slot(e, idx, path, subsong, kind == SEL_BEGIN_SEEK ? seek_to : 0, detail)) {
        return 0;
    }
    e->cur = idx;
    e->song_over = 0;
    if (kind == SEL_BEGIN_OPEN) {
        e->songs_opened++;
        evt_init(&ev, SEL_EVT_SONG_OPENED);
        evt_tags(&ev, e->slot[idx].rp_metadata ? e->slot[idx].rp_metadata(e->slot[idx].rp_state) : NULL);
        emit(e, &ev);
    } else if (kind == SEL_BEGIN_RESTART) {
        evt_init(&ev, SEL_EVT_SEEKED);
        evt_uint(&ev, 0);
        emit(e, &ev);
    } else if (kind == SEL_BEGIN_SEEK) {
        evt_init(&ev, SEL_EVT_SEEKED);
        evt_uint(&ev, e->slot[idx].song_frames);
        emit(e, &ev);
    }
    emit_position(e, &e->slot[idx]);
    return 1;
}

static void cmd_OPEN(sel_engine* e, const sel_message* m) {
    e->q_playing = 0;
    begin_song(e, SEL_BEGIN_OPEN, m->args[0].v.s, SEL_SUBSONG_DEFAULT, 0);
}

/* Starts the open song's file again in the idle slot: SUBSONG (n) or
 * PREV's restart. The path lives in the slot that reclaim may close, so it
 * is copied first. 0: no song open. */
static int reopen_current(sel_engine* e, uint32_t kind, uint32_t subsong, uint32_t seek_to, const char* who) {
    sel_slot* cur = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    if (cur == NULL || cur->rp_state == NULL) {
        sel_engine_report_error(e, "no_song", who);
        return 0;
    }
    if (strlen(cur->path) >= SEL_DEFERRED_PATH_MAX) {
        sel_engine_report_error(e, "start_failed", who);
        return 0;
    }
    memcpy(e->subsong_path, cur->path, strlen(cur->path) + 1u);
    return begin_song(e, kind, e->subsong_path, subsong == SEL_SUBSONG_DEFAULT ? cur->subsong : subsong,
                      seek_to);
}

static void cmd_SUBSONG(sel_engine* e, const sel_message* m) {
    uint32_t n = m->args[0].v.u;
    sel_slot* cur = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    if (cur != NULL && cur->rp_state != NULL && n >= cur->rp_info.subsong_count) {
        sel_engine_report_error(e, "range", "SUBSONG");
        return;
    }
    reopen_current(e, SEL_BEGIN_SUBSONG, n, 0, "SUBSONG");
}

/* SEEK "ms" | "+ms" | "-ms": absolute or relative to the playing position,
 * clamped at 0. The target stays in ms; prepare_slot converts it to a frame once. */
static void cmd_SEEK(sel_engine* e, const sel_message* m) {
    const char* a = m->args[0].v.s;
    const sel_slot* cur = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    int64_t ms = 0;
    int rel = 0, neg = 0;
    if (cur == NULL || cur->rp_state == NULL) {
        sel_engine_report_error(e, "no_song", "SEEK");
        return;
    }
    if (a != NULL && (*a == '+' || *a == '-')) {
        rel = 1;
        neg = *a == '-';
        a++;
    }
    if (a == NULL || *a == '\0') {
        sel_engine_report_error(e, "range", "SEEK");
        return;
    }
    for (; *a != '\0'; a++) {
        if (*a < '0' || *a > '9' || ms > 4000000000000LL) {
            sel_engine_report_error(e, "range", "SEEK");
            return;
        }
        ms = ms * 10 + (*a - '0');
    }
    if (rel) {
        int64_t now = (int64_t)((uint64_t)cur->song_frames * 1000u / e->rate);
        ms = neg ? now - ms : now + ms;
        if (ms < 0) {
            ms = 0;
        }
    }
    if (ms > 4000000) { /* far beyond any song; keeps ms * rate inside 32 bits */
        ms = 4000000;
    }
    reopen_current(e, SEL_BEGIN_SEEK, SEL_SUBSONG_DEFAULT, (uint32_t)ms, "SEEK");
}

/* ---- queue ----------------------------------------------------------- */

static void emit_queue_changed(sel_engine* e) {
    sel_message m;
    evt_init(&m, SEL_EVT_QUEUE_CHANGED);
    evt_uint(&m, e->queue.version);
    emit(e, &m);
    evt_init(&m, SEL_EVT_QUEUE_HISTORY);
    evt_uint(&m, e->history.undo_count);
    evt_uint(&m, e->history.redo_count);
    emit(e, &m);
}

/* The album of a queue entry is the directory of its path. */
static int same_album(const sel_queue* q, uint32_t a, uint32_t b) {
    const char* pa = sel_queue_path(q, a);
    const char* pb = sel_queue_path(q, b);
    const char* sa = strrchr(pa, '/');
    const char* sb = strrchr(pb, '/');
    size_t la = sa ? (size_t)(sa - pa) : 0;
    size_t lb = sb ? (size_t)(sb - pb) : 0;
    return la == lb && memcmp(pa, pb, la) == 0;
}

/* First position of the album that holds position `idx`. */
static uint32_t album_first(const sel_queue* q, uint32_t idx) {
    while (idx > 0 && same_album(q, idx - 1u, idx)) {
        idx--;
    }
    return idx;
}

static int random_on(const sel_engine* e) {
    return strcmp(e->state.options[SEL_OPT_RANDOM], "on") == 0;
}

static int row_is_divider(const sel_queue* q, uint32_t idx) {
    return (q->entries[idx].flags & SEL_QUEUE_FLAG_DIVIDER) != 0;
}

/* First position at or after idx that is not a divider row. */
static uint32_t skip_dividers(const sel_queue* q, uint32_t idx) {
    while (idx < q->count && row_is_divider(q, idx)) {
        idx++;
    }
    return idx;
}

/* ---- random play order ------------------------------------------------ */

/* Starts a new round: nothing played, except the song that plays now. */
static void order_reset(sel_engine* e) {
    e->order_n = 0;
    e->order_done = 0;
    if (e->q_playing && e->order != NULL) {
        e->order[0] = e->q_playing;
        e->order_n = 1;
        e->order_done = 1;
    }
    e->order_qver = 0xFFFFFFFFu; /* never equal to a queue version: rebuild on first use */
}

/* Brings the order in line with the queue: played ids that are still queued
 * stay played; every other playable row is shuffled behind them. */
static void order_ensure(sel_engine* e) {
    const sel_queue* q = &e->queue;
    uint32_t i, kept = 0;
    if (e->order == NULL || e->order_qver == q->version) {
        return;
    }
    memset(e->order_seen, 0, q->count);
    for (i = 0; i < e->order_done; i++) {
        uint32_t at = sel_queue_index_of(q, e->order[i]);
        if (at != SEL_QUEUE_NONE) {
            e->order[kept++] = e->order[i];
            e->order_seen[at] = 1;
        }
    }
    e->order_done = kept;
    for (i = 0; i < q->count; i++) {
        if (!e->order_seen[i] && !row_is_divider(q, i)) {
            e->order[kept++] = q->entries[i].id;
        }
    }
    e->order_n = kept;
    e->rng = sel_shuffle_ids(e->order + e->order_done, kept - e->order_done, e->rng);
    e->order_qver = q->version;
}

/* Marks the entry at queue position idx as the one that plays now. */
static void order_mark(sel_engine* e, uint32_t idx) {
    uint32_t id = e->queue.entries[idx].id;
    uint32_t k;
    order_ensure(e);
    for (k = 0; k < e->order_n && e->order[k] != id; k++) {
    }
    if (k == e->order_n) {
        return;
    }
    if (k >= e->order_done) {
        uint32_t t = e->order[e->order_done];
        e->order[e->order_done] = id;
        e->order[k] = t;
        e->order_done++;
    } else {
        e->order_done = k + 1u;
    }
}

/* Position of the next unplayed entry in the order that the scope allows;
 * `from` is the playing position, SEL_QUEUE_NONE when nothing plays. */
static uint32_t order_next(sel_engine* e, uint32_t from) {
    uint32_t k;
    int album = from != SEL_QUEUE_NONE && strcmp(e->state.options[SEL_OPT_SCOPE], "album") == 0;
    order_ensure(e);
    for (k = e->order_done; k < e->order_n; k++) {
        uint32_t at = sel_queue_index_of(&e->queue, e->order[k]);
        if (at != SEL_QUEUE_NONE && (!album || same_album(&e->queue, from, at))) {
            return at;
        }
    }
    return SEL_QUEUE_NONE;
}

/* Position of the entry that played before the playing one in the order. */
static uint32_t order_prev(sel_engine* e) {
    uint32_t k;
    order_ensure(e);
    for (k = 0; k < e->order_n && e->order[k] != e->q_playing; k++) {
    }
    return (k > 0 && k < e->order_n) ? sel_queue_index_of(&e->queue, e->order[k - 1u]) : SEL_QUEUE_NONE;
}

/* Position NEXT and PLAY start from while nothing plays. */
static uint32_t start_index(sel_engine* e) {
    if (random_on(e)) {
        if (e->song_over) {
            order_reset(e);
        }
        return order_next(e, SEL_QUEUE_NONE);
    }
    return skip_dividers(&e->queue, e->q_hint);
}

/* Plays the entry at `idx` through the one song-change path. A stopped
 * transport starts; paused stays paused. 0: the entry could not start. */
static int queue_start(sel_engine* e, uint32_t idx) {
    if (random_on(e)) {
        order_mark(e, idx);
    }
    e->q_playing = e->queue.entries[idx].id;
    e->q_hint = idx;
    if (!begin_song(e, SEL_BEGIN_OPEN, sel_queue_path(&e->queue, idx),
                    e->queue.entries[idx].subsong_plus1 ? e->queue.entries[idx].subsong_plus1 - 1u : SEL_SUBSONG_DEFAULT, 0)) {
        e->q_playing = 0;
        return 0;
    }
    if (e->state.play == SEL_STOPPED) {
        set_play(e, SEL_PLAYING);
    }
    return 1;
}

/* The queue ran out: the transport stops and the next NEXT or PLAY starts over. */
static void queue_finish(sel_engine* e) {
    e->q_playing = 0;
    e->q_hint = 0;
    e->song_over = 1;
    set_play(e, SEL_STOPPED);
}

static void cmd_PLAY(sel_engine* e, const sel_message* m) {
    (void)m;
    if (e->q_playing == 0 && e->queue.count > 0 && (e->cur == SEL_SLOT_NONE || e->song_over)) {
        uint32_t idx;
        if (!random_on(e) && e->q_hint >= e->queue.count) {
            e->q_hint = 0;
        }
        idx = start_index(e);
        if (idx < e->queue.count) {
            queue_start(e, idx);
        }
        return;
    }
    set_play(e, SEL_PLAYING);
}

/* Position that follows the playing one, honouring the scope option;
 * SEL_QUEUE_NONE when the queue (or the album under scope album) ends. */
static uint32_t queue_next_index(sel_engine* e) {
    uint32_t cur, next;
    if (e->q_playing == 0) {
        return SEL_QUEUE_NONE;
    }
    cur = sel_queue_index_of(&e->queue, e->q_playing);
    if (random_on(e)) {
        return order_next(e, cur);
    }
    next = skip_dividers(&e->queue, cur + 1u);
    if (next >= e->queue.count) {
        return SEL_QUEUE_NONE;
    }
    if (strcmp(e->state.options[SEL_OPT_SCOPE], "album") == 0 && !same_album(&e->queue, cur, next)) {
        return SEL_QUEUE_NONE;
    }
    return next;
}

static void cmd_NEXT(sel_engine* e, const sel_message* m) {
    uint32_t idx = e->q_playing ? queue_next_index(e) : start_index(e);
    (void)m;
    if (idx >= e->queue.count) {
        queue_finish(e);
        return;
    }
    queue_start(e, idx);
}

static void cmd_NEXT_ALBUM(sel_engine* e, const sel_message* m) {
    uint32_t cur = e->q_playing ? sel_queue_index_of(&e->queue, e->q_playing) : e->q_hint;
    uint32_t idx = cur;
    (void)m;
    if (cur >= e->queue.count) {
        queue_finish(e);
        return;
    }
    while (idx < e->queue.count && same_album(&e->queue, cur, idx)) {
        idx++;
    }
    if (idx >= e->queue.count) {
        queue_finish(e);
        return;
    }
    queue_start(e, idx);
}

/* Plays the first song of the album before the current one; in the first
 * album it plays that album's first song. */
static void cmd_PREV_ALBUM(sel_engine* e, const sel_message* m) {
    uint32_t cur = e->q_playing ? sel_queue_index_of(&e->queue, e->q_playing) : e->q_hint;
    uint32_t first;
    (void)m;
    if (cur >= e->queue.count) {
        sel_engine_report_error(e, "no_song", "PREV_ALBUM");
        return;
    }
    first = album_first(&e->queue, cur);
    if (first > 0) {
        first = album_first(&e->queue, first - 1u);
    }
    queue_start(e, first);
}

static void cmd_SCOPE(sel_engine* e, const sel_message* m) {
    const char* v = m->args[0].v.s;
    if (strcmp(v, "library") != 0 && strcmp(v, "album") != 0 && strcmp(v, "playlist") != 0) {
        sel_engine_report_error(e, "range", "SCOPE");
        return;
    }
    set_option(e, SEL_OPT_SCOPE, v);
}

static void cmd_PREV(sel_engine* e, const sel_message* m) {
    const sel_slot* cur = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    uint32_t idx = e->q_playing ? sel_queue_index_of(&e->queue, e->q_playing) : 0;
    (void)m;
    if (cur == NULL || cur->rp_state == NULL) {
        sel_engine_report_error(e, "no_song", "PREV");
        return;
    }
    uint32_t prev = SEL_QUEUE_NONE;
    if (random_on(e)) {
        prev = order_prev(e);
    } else {
        while (idx > 0 && prev == SEL_QUEUE_NONE) {
            idx--;
            if (!row_is_divider(&e->queue, idx)) {
                prev = idx;
            }
        }
    }
    if (prev == SEL_QUEUE_NONE || (uint64_t)cur->song_frames * 1000u / e->rate >= SEL_PREV_RESTART_MS) {
        reopen_current(e, SEL_BEGIN_RESTART, SEL_SUBSONG_DEFAULT, 0, "PREV");
        return;
    }
    queue_start(e, prev);
}

static void queue_insert(sel_engine* e, uint32_t at, const char* path, const char* who) {
    uint32_t id;
    sel_history_record(&e->history, &e->queue);
    if (!sel_queue_insert(&e->queue, at, path, &id)) {
        sel_history_forget(&e->history);
        sel_engine_report_error(e, "queue_full", who);
        return;
    }
    emit_queue_changed(e);
}

static void cmd_QUEUE_ADD(sel_engine* e, const sel_message* m) {
    queue_insert(e, e->queue.count, m->args[0].v.s, "QUEUE_ADD");
}

/* ---- playlist files --------------------------------------------------- */

/* A key is resolved against the playlist's directory unless it is already
 * absolute: a URL (scheme://), a Unix path, or an Amiga volume path (Work:x).
 * The result is false when it does not fit. */
static int playlist_resolve(char* out, size_t cap, const char* plpath, const char* key) {
    size_t dirlen = 0, i, klen = strlen(key);
    int absolute = key[0] == '/' || strstr(key, "://") != NULL;
    for (i = 0; key[i] != '\0' && key[i] != '/' && !absolute; i++) {
        if (key[i] == ':') {
            absolute = 1;
        }
    }
    if (!absolute) {
        for (i = 0; plpath[i] != '\0'; i++) {
            if (plpath[i] == '/' || plpath[i] == ':') {
                dirlen = i + 1u;
            }
        }
    }
    if (dirlen + klen + 1u > cap) {
        return 0;
    }
    memcpy(out, plpath, dirlen);
    memcpy(out + dirlen, key, klen + 1u);
    return 1;
}

static void playlist_warn(void* ctx, const char* msg) {
    emit_log((sel_engine*)ctx, "playlist", msg);
}

static void cmd_PLAYLIST_LOAD(sel_engine* e, const sel_message* m) {
    const char* path = m->args[0].v.s;
    int replace = 0;
    sel_playlist pl;
    sel_pl_format fmt;
    FILE* f;
    size_t n;
    uint32_t i, text_cap = e->queue.text_bytes + 16u;
    char full[SEL_DEFERRED_PATH_MAX];
    int rc;

    if (m->argc > 1) {
        if (strcmp(m->args[1].v.s, "replace") == 0) {
            replace = 1;
        } else if (strcmp(m->args[1].v.s, "append") != 0) {
            sel_engine_report_error(e, "range", "PLAYLIST_LOAD");
            return;
        }
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        sel_engine_report_error(e, "open_failed", "PLAYLIST_LOAD");
        return;
    }
    n = fread(e->pl_file, 1, text_cap, f);
    rc = fgetc(f); /* anything left means the file does not fit the buffer */
    fclose(f);
    if (rc != EOF) {
        sel_engine_report_error(e, "queue_full", "PLAYLIST_LOAD");
        return;
    }
    fmt = sel_playlist_format_from_name(path);
    sel_playlist_init(&pl, e->pl_entries, e->queue.capacity, e->pl_text, text_cap);
    rc = sel_playlist_read(fmt, e->pl_file, n, &pl, playlist_warn, e);
    if (rc != SEL_PL_OK) {
        sel_engine_report_error(e, rc == SEL_PL_RANGE ? "queue_full" : "bad_format", "PLAYLIST_LOAD");
        return;
    }
    sel_history_record(&e->history, &e->queue);
    if (replace) {
        sel_history_bury(&e->history, &e->queue, SEL_QUEUE_NONE);
        sel_queue_clear(&e->queue);
        e->q_playing = 0;
        e->q_hint = 0;
    }
    for (i = 0; i < pl.count; i++) {
        const sel_pl_entry* pe = &pl.entries[i];
        const char* key = sel_playlist_str(&pl, pe->key);
        const char* shown = pe->kind == SEL_PL_DIVIDER ? sel_playlist_str(&pl, pe->name) : full;
        uint32_t id, at;
        if (pe->kind != SEL_PL_DIVIDER && !playlist_resolve(full, sizeof full, path, key)) {
            sel_engine_report_error(e, "range", "PLAYLIST_LOAD");
            break;
        }
        if (!sel_queue_insert(&e->queue, e->queue.count, shown, &id)) {
            sel_engine_report_error(e, "queue_full", "PLAYLIST_LOAD");
            break;
        }
        at = sel_queue_index_of(&e->queue, id);
        e->queue.entries[at].subsong_plus1 = pe->subsong;
        e->queue.entries[at].flags = pe->kind == SEL_PL_DIVIDER ? SEL_QUEUE_FLAG_DIVIDER
                                     : pe->kind == SEL_PL_STEALTH ? SEL_QUEUE_FLAG_HIDDEN
                                                                  : 0u;
    }
    emit_queue_changed(e);
}

static int playlist_sink(void* ctx, const char* bytes, uint32_t len) {
    return fwrite(bytes, 1, len, (FILE*)ctx) == len;
}

static void cmd_PLAYLIST_SAVE(sel_engine* e, const sel_message* m) {
    const char* path = m->args[0].v.s;
    sel_playlist pl;
    sel_pl_format fmt = sel_playlist_format_from_name(path);
    uint32_t i;
    FILE* f;
    int rc;

    if (fmt == SEL_PL_AUTO) {
        sel_engine_report_error(e, "bad_format", "PLAYLIST_SAVE");
        return;
    }
    sel_playlist_init(&pl, e->pl_entries, e->queue.capacity, e->pl_text, e->queue.text_bytes + 16u);
    for (i = 0; i < e->queue.count; i++) {
        const sel_queue_entry* en = &e->queue.entries[i];
        const char* text = sel_queue_path(&e->queue, i);
        size_t len = strlen(text);
        int divider = (en->flags & SEL_QUEUE_FLAG_DIVIDER) != 0;
        sel_pl_kind kind = divider ? SEL_PL_DIVIDER : (en->flags & SEL_QUEUE_FLAG_HIDDEN) ? SEL_PL_STEALTH : SEL_PL_SONG;
        uint8_t sub = en->subsong_plus1 > 255u ? 255u : (uint8_t)en->subsong_plus1;
        if (sel_playlist_add(&pl, kind, divider ? NULL : text, divider ? 0 : len, divider ? text : NULL, divider ? len : 0, sub,
                             0) != 0) {
            sel_engine_report_error(e, "queue_full", "PLAYLIST_SAVE");
            return;
        }
    }
    f = fopen(path, "wb");
    if (f == NULL) {
        sel_engine_report_error(e, "open_failed", "PLAYLIST_SAVE");
        return;
    }
    rc = sel_playlist_write(fmt, &pl, playlist_sink, f, playlist_warn, e);
    if (fclose(f) != 0 && rc == SEL_PL_OK) {
        rc = SEL_PL_SINK;
    }
    if (rc != SEL_PL_OK) {
        sel_engine_report_error(e, "open_failed", "PLAYLIST_SAVE");
    }
}

static void cmd_QUEUE_PLAY_NEXT(sel_engine* e, const sel_message* m) {
    uint32_t at = e->q_playing ? sel_queue_index_of(&e->queue, e->q_playing) + 1u : e->q_hint;
    queue_insert(e, at, m->args[0].v.s, "QUEUE_PLAY_NEXT");
}

static void cmd_QUEUE_REMOVE(sel_engine* e, const sel_message* m) {
    uint32_t id = m->args[0].v.u;
    uint32_t at = sel_queue_index_of(&e->queue, id);
    if (at != SEL_QUEUE_NONE) {
        sel_history_record(&e->history, &e->queue);
        sel_history_bury(&e->history, &e->queue, at);
        sel_queue_remove(&e->queue, id);
    }
    if (at == SEL_QUEUE_NONE) {
        sel_engine_report_error(e, "range", "QUEUE_REMOVE");
        return;
    }
    if (id == e->q_playing) {
        /* the song keeps playing; the entry now at this position is next */
        e->q_playing = 0;
        e->q_hint = at;
    } else if (e->q_playing == 0 && at < e->q_hint) {
        e->q_hint--;
    }
    emit_queue_changed(e);
}

static void cmd_QUEUE_CLEAR(sel_engine* e, const sel_message* m) {
    (void)m;
    sel_history_record(&e->history, &e->queue);
    sel_history_bury(&e->history, &e->queue, SEL_QUEUE_NONE);
    sel_queue_clear(&e->queue);
    e->q_playing = 0;
    e->q_hint = 0;
    emit_queue_changed(e);
}

/* The playing entry may be gone after a rewrite of the order: the song keeps
 * playing and the hint points at the position it had. */
static void queue_after_rewrite(sel_engine* e, uint32_t old_index) {
    if (e->q_playing && sel_queue_index_of(&e->queue, e->q_playing) == SEL_QUEUE_NONE) {
        e->q_playing = 0;
        e->q_hint = old_index;
    }
    if (e->q_hint > e->queue.count) {
        e->q_hint = e->queue.count;
    }
}

static void cmd_QUEUE_MOVE(sel_engine* e, const sel_message* m) {
    sel_history_record(&e->history, &e->queue);
    if (!sel_queue_move(&e->queue, m->args[0].v.u, m->args[1].v.u)) {
        sel_history_forget(&e->history);
        sel_engine_report_error(e, "range", "QUEUE_MOVE");
        return;
    }
    emit_queue_changed(e);
}

static void cmd_QUEUE_SWAP(sel_engine* e, const sel_message* m) {
    sel_history_record(&e->history, &e->queue);
    if (!sel_queue_swap(&e->queue, m->args[0].v.u, m->args[1].v.u)) {
        sel_history_forget(&e->history);
        sel_engine_report_error(e, "range", "QUEUE_SWAP");
        return;
    }
    emit_queue_changed(e);
}

/* Shuffles the rows after the playing one (all rows when nothing plays). */
static void queue_shuffle_rows(sel_engine* e, uint32_t seed) {
    uint32_t first = e->q_playing ? sel_queue_index_of(&e->queue, e->q_playing) + 1u : 0;
    sel_history_record(&e->history, &e->queue);
    e->rng = sel_queue_shuffle(&e->queue, first, seed);
    emit_queue_changed(e);
}

static void cmd_QUEUE_SHUFFLE(sel_engine* e, const sel_message* m) {
    uint32_t seed = 0;
    const char* p = m->argc ? m->args[0].v.s : NULL;
    if (p != NULL) {
        for (; *p >= '0' && *p <= '9'; p++) {
            seed = seed * 10u + (uint32_t)(*p - '0');
        }
        if (p == m->args[0].v.s || *p != '\0') {
            sel_engine_report_error(e, "range", "QUEUE_SHUFFLE");
            return;
        }
    } else {
        seed = e->host->clock_ms(e->host) ^ e->rng;
    }
    queue_shuffle_rows(e, seed);
}

static void cmd_RESHUFFLE(sel_engine* e, const sel_message* m) {
    (void)m;
    if (random_on(e)) {
        /* rows stay; the unplayed part of the play order is permuted again */
        order_ensure(e);
        e->rng = sel_shuffle_ids(e->order + e->order_done, e->order_n - e->order_done,
                                 sel_xorshift32(sel_shuffle_seed(e->rng)));
        return;
    }
    queue_shuffle_rows(e, sel_xorshift32(sel_shuffle_seed(e->rng)));
}

static void cmd_SHUFFLE(sel_engine* e, const sel_message* m) {
    const char* v = m->argc ? m->args[0].v.s : "on";
    if (strcmp(v, "on") != 0 && strcmp(v, "off") != 0) {
        sel_engine_report_error(e, "range", "SHUFFLE");
        return;
    }
    if (m->argc > 1) {
        uint32_t seed = 0;
        const char* p = m->args[1].v.s;
        for (; *p >= '0' && *p <= '9'; p++) {
            seed = seed * 10u + (uint32_t)(*p - '0');
        }
        if (p == m->args[1].v.s || *p != '\0') {
            sel_engine_report_error(e, "range", "SHUFFLE");
            return;
        }
        e->rng = seed;
    } else if (strcmp(v, "on") == 0) {
        e->rng = e->host->clock_ms(e->host) ^ e->rng;
    }
    set_option(e, SEL_OPT_RANDOM, v);
}

static void queue_step(sel_engine* e, int undo, const char* who) {
    uint32_t old_index = e->q_playing ? sel_queue_index_of(&e->queue, e->q_playing) : e->q_hint;
    if (!(undo ? sel_history_undo(&e->history, &e->queue) : sel_history_redo(&e->history, &e->queue))) {
        sel_engine_report_error(e, "nothing_to_do", who);
        return;
    }
    queue_after_rewrite(e, old_index);
    emit_queue_changed(e);
}

static void cmd_UNDO(sel_engine* e, const sel_message* m) {
    (void)m;
    queue_step(e, 1, "UNDO");
}

static void cmd_REDO(sel_engine* e, const sel_message* m) {
    (void)m;
    queue_step(e, 0, "REDO");
}

/* ---- library --------------------------------------------------------- */

static void emit_library_result(sel_engine* e) {
    sel_message m;
    uint32_t slot = e->lib_slot++ & 3u;
    sel_tag* t = e->lib_tags[slot];
    memcpy(e->lib_path[slot], e->browse.path, strlen(e->browse.path) + 1u);
    t[0].tag = SEL_SRC_PATH;
    t[0].value = (uintptr_t)e->lib_path[slot];
    t[1].tag = SEL_SRC_COUNT;
    t[1].value = e->browse.count;
    t[2].tag = SEL_TAG_END;
    t[2].value = 0;
    evt_init(&m, SEL_EVT_LIBRARY_RESULT);
    evt_tags(&m, t);
    emit(e, &m);
}

/* Lists `path` into the browse buffer and reports it. 0: the directory
 * could not be read (ERROR browse_failed). */
static int browse_to(sel_engine* e, const char* path, const char* who) {
    if (!sel_browse_list(&e->browse, path)) {
        sel_engine_report_error(e, "browse_failed", who);
        return 0;
    }
    if (e->browse.truncated) {
        char n[16];
        snprintf(n, sizeof n, "%u", (unsigned)e->browse.truncated);
        emit_log(e, "browse_truncated", n);
    }
    emit_library_result(e);
    return 1;
}

static void cmd_LIBRARY_BROWSE(sel_engine* e, const sel_message* m) {
    browse_to(e, m->args[0].v.s, "LIBRARY_BROWSE");
}

/* ---- view ------------------------------------------------------------ */

static void view_src(const sel_engine* e, sel_view_src* s) {
    s->queue = &e->queue;
    s->browse = &e->browse;
    s->playing = e->q_playing;
    s->title_format = e->prefs.value[SEL_PREF_VIEW_TITLE_FORMAT];
    s->prefs_generation = e->prefs.generation;
}

/* VIEW_ROWS carries the window position only; the rows are read from the
 * snapshot view. VIEW_CHANGED adds the style name. */
static void emit_view(sel_engine* e, sel_evt_id id) {
    sel_message m;
    sel_tag* t = e->view_tags[e->view_slot++ & 3u];
    const sel_view_state* v = &e->view.pub;
    t[0].tag = SEL_UI_ROW_COUNT;
    t[0].value = v->row_count;
    t[1].tag = SEL_UI_CURSOR;
    t[1].value = v->cursor;
    t[2].tag = SEL_UI_WINDOW_START;
    t[2].value = v->window_start;
    t[3].tag = SEL_UI_WINDOW_ROWS;
    t[3].value = v->window_rows;
    t[4].tag = SEL_TAG_END;
    t[4].value = 0;
    evt_init(&m, id);
    if (id == SEL_EVT_VIEW_CHANGED) {
        evt_str(&m, sel_view_style_name(&e->view));
    }
    evt_tags(&m, t);
    emit(e, &m);
}

/* Pump thread, after the snapshot: tells the front end what moved. */
static void view_sync(sel_engine* e) {
    sel_view_src s;
    int structure;
    view_src(e, &s);
    if (sel_view_changed(&e->view, &s, &structure)) {
        emit_view(e, structure ? SEL_EVT_VIEW_CHANGED : SEL_EVT_VIEW_ROWS);
        if (structure) {
            emit_view(e, SEL_EVT_VIEW_ROWS);
        }
    }
}

static void cmd_VIEW_SHOW(sel_engine* e, const sel_message* m) {
    sel_view_src s;
    view_src(e, &s);
    e->view.active = 1;
    if (!sel_view_show(&e->view, &s, m->args[0].v.s)) {
        sel_engine_report_error(e, "unknown_view", m->args[0].v.s);
    }
}

static void cmd_VIEW_CURSOR(sel_engine* e, const sel_message* m) {
    sel_view_src s;
    view_src(e, &s);
    e->view.active = 1;
    if (!sel_view_cursor(&e->view, &s, m->args[0].v.s)) {
        sel_engine_report_error(e, "range", "VIEW_CURSOR");
    }
}

static void cmd_VIEW_WINDOW(sel_engine* e, const sel_message* m) {
    sel_view_src s;
    view_src(e, &s);
    e->view.active = 1;
    if (!sel_view_window(&e->view, &s, m->args[0].v.u)) {
        sel_engine_report_error(e, "range", "VIEW_WINDOW");
    }
}

static void cmd_VIEW_STYLE(sel_engine* e, const sel_message* m) {
    sel_view_src s;
    view_src(e, &s);
    if (!sel_view_set_style(&e->view, &s, m->args[0].v.s)) {
        sel_engine_report_error(e, "unsupported_style", m->args[0].v.s);
        return;
    }
    e->view.active = 1;
    e->view.force_changed = 1;
}

static void cmd_VIEW_GROUP(sel_engine* e, const sel_message* m) {
    sel_view_src s;
    view_src(e, &s);
    if (!sel_view_set_group(&e->view, &s, m->args[0].v.s)) {
        sel_engine_report_error(e, "unsupported", "VIEW_GROUP");
        return;
    }
    e->view.active = 1;
    e->view.force_changed = 1;
}

static void view_expand(sel_engine* e, const sel_message* m, int on, const char* name) {
    sel_view_src s;
    view_src(e, &s);
    e->view.active = 1;
    if (!sel_view_expand(&e->view, &s, m->args[0].v.u, on)) {
        sel_engine_report_error(e, "range", name);
        return;
    }
    e->view.force_changed = 1;
}

static void cmd_VIEW_EXPAND(sel_engine* e, const sel_message* m) {
    view_expand(e, m, 1, "VIEW_EXPAND");
}

static void cmd_VIEW_COLLAPSE(sel_engine* e, const sel_message* m) {
    view_expand(e, m, 0, "VIEW_COLLAPSE");
}

static void cmd_VIEW_DRILL(sel_engine* e, const sel_message* m) {
    sel_view_src s;
    view_src(e, &s);
    e->view.active = 1;
    if (!sel_view_drill(&e->view, &s, m->args[0].v.u, m->args[1].v.s)) {
        sel_engine_report_error(e, "range", "VIEW_DRILL");
        return;
    }
    e->view.force_changed = 1;
}

/* Enter a directory, play a file, or start a queue entry. */
static void cmd_VIEW_ACTIVATE(sel_engine* e, const sel_message* m) {
    sel_view_src s;
    sel_view_state* v = &e->view.pub;
    uint32_t at;
    (void)m;
    e->view.active = 1;
    view_src(e, &s);
    sel_view_refresh(&e->view, &s);
    if (v->row_count == 0) {
        return;
    }
    at = v->cursor;
    if (v->source == SEL_VIEW_QUEUE) {
        uint32_t qi;
        if (sel_view_enter(&e->view, &s, &qi)) {
            queue_start(e, qi);
        } else {
            e->view.force_changed = 1;
        }
        return;
    }
    {
        char path[SEL_BROWSE_PATH_MAX];
        char leaf[256];
        const char* name = sel_browse_name(&e->browse, at);
        int up = strcmp(name, "..") == 0;
        copy_str(leaf, sizeof leaf, sel_browse_basename(e->browse.path));
        if (!sel_browse_join(path, sizeof path, e->browse.path, name)) {
            sel_engine_report_error(e, "browse_failed", "VIEW_ACTIVATE");
            return;
        }
        if (e->browse.entries[at].is_dir) {
            if (browse_to(e, path, "VIEW_ACTIVATE")) {
                uint32_t found = up ? sel_browse_find(&e->browse, leaf) : 0;
                e->view.land = found < e->browse.count ? found : 0;
            }
        } else {
            uint32_t before = e->queue.count;
            queue_insert(e, before, path, "VIEW_ACTIVATE");
            if (e->queue.count > before) {
                queue_start(e, before);
            }
        }
    }
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
    uint32_t queue_cap, queue_text;
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
    sel_queue_default_sizes(cfg ? (uint32_t)sel_tag_find(cfg->machine, SEL_MACHINE_FAST_RAM, 0) : 0, &queue_cap,
                            &queue_text);
    if (cfg && cfg->queue_capacity) {
        queue_cap = cfg->queue_capacity;
    }
    if (cfg && cfg->queue_text_bytes) {
        queue_text = cfg->queue_text_bytes;
    }
    sel_prefs_init(&e->prefs);
    if (cfg && cfg->config_dir && strlen(cfg->config_dir) < sizeof e->config_dir) {
        strcpy(e->config_dir, cfg->config_dir);
        (void)sel_prefs_load(&e->prefs, e->config_dir);
    }
    copy_str(e->state.output, SEL_OUTPUT_NAME_MAX, "null");
    e->cur = SEL_SLOT_NONE;
    e->audio_slot = SEL_SLOT_NONE;
    e->act = SEL_SLOT_NONE;
    if (!sel_spsc_init(&e->cmd_q, host, cmd_depth) || !sel_spsc_init(&e->evt_q, host, evt_depth) ||
        !sel_spsc_init(&e->swap_q, host, SEL_SWAP_RING_DEPTH) ||
        !sel_spsc_init(&e->ret_q, host, SEL_SWAP_RING_DEPTH) ||
        !sel_queue_init(&e->queue, host, queue_cap, queue_text) ||
        !sel_history_init(&e->history, host, queue_cap, queue_text) ||
        (e->order = host->mem_alloc(host, (size_t)queue_cap * sizeof *e->order, SEL_MEM_FAST)) == NULL ||
        (e->order_seen = host->mem_alloc(host, queue_cap, SEL_MEM_FAST)) == NULL ||
        (e->pl_entries = host->mem_alloc(host, (size_t)queue_cap * sizeof *e->pl_entries, SEL_MEM_FAST)) == NULL ||
        (e->pl_text = host->mem_alloc(host, (size_t)queue_text + 16u, SEL_MEM_FAST)) == NULL ||
        (e->pl_file = host->mem_alloc(host, (size_t)queue_text + 16u, SEL_MEM_FAST)) == NULL ||
        !sel_browse_init(&e->browse, host, cfg && cfg->browse_entries ? cfg->browse_entries : queue_cap,
                         cfg && cfg->browse_text_bytes ? cfg->browse_text_bytes : queue_text) ||
        !sel_view_init(&e->view, host, SEL_VIEW_WINDOW_MAX)) {
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
        sel_queue_free(&e->queue, e->host);
        sel_history_free(&e->history, e->host);
        if (e->order) {
            e->host->mem_free(e->host, e->order);
        }
        if (e->order_seen) {
            e->host->mem_free(e->host, e->order_seen);
        }
        if (e->pl_entries) {
            e->host->mem_free(e->host, e->pl_entries);
        }
        if (e->pl_text) {
            e->host->mem_free(e->host, e->pl_text);
        }
        if (e->pl_file) {
            e->host->mem_free(e->host, e->pl_file);
        }
        sel_browse_free(&e->browse, e->host);
        sel_view_free(&e->view, e->host);
        e->host->mem_free(e->host, e);
    }
}

int sel_engine_send(sel_engine* e, const sel_message* cmd) {
    SEL_THREAD_NOT_AUDIO(e);
    return sel_spsc_push(&e->cmd_q, cmd);
}

/* Pump thread: copies the song and queue facts into the snapshot. */
static void refresh_snapshot(sel_engine* e) {
    const sel_slot* sl = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    sel_song_state* song = &e->state.song;
    e->state.view = &e->view.pub;
    memset(song, 0, sizeof *song);
    if (sl != NULL && sl->rp_state != NULL) {
        song->queue_id = e->q_playing;
        song->subsong = sl->subsong;
        song->subsong_count = sl->rp_info.subsong_count;
        song->position_ms = (uint32_t)((uint64_t)sl->song_frames * 1000u / e->rate);
        song->meta = sl->rp_metadata ? sl->rp_metadata(sl->rp_state) : NULL;
        song->duration_ms = (uint32_t)sel_tag_find(song->meta, SEL_META_DURATION_MS, 0);
        song->path = sl->path;
        if (sl->pos_valid) {
            song->pattern_pos.order = sl->pos_order;
            song->pattern_pos.row = sl->pos_row;
            song->pattern_pos.pattern = sl->pos_pattern;
        }
    }
    e->state.queue = e->queue.entries;
    e->state.queue_text = e->queue.text;
    e->state.queue_count = e->queue.count;
    e->state.queue_playing = e->q_playing;
    e->state.queue_version = e->queue.version;
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
        begin_song(e, e->deferred, e->deferred_path, e->deferred_subsong, e->deferred_seek);
    }
    if (e->cur != SEL_SLOT_NONE) {
        sel_slot* sl = &e->slot[e->cur];
        int advanced = 0;
        if (sl->ended && !sl->reported) {
            sel_message ev;
            uint32_t next = queue_next_index(e);
            sl->reported = 1;
            evt_init(&ev, SEL_EVT_SONG_ENDED);
            emit(e, &ev);
            if (next < e->queue.count && queue_start(e, next)) {
                advanced = 1;
            } else {
                queue_finish(e);
            }
        }
        if (!advanced && sl->rp_state != NULL && sl->song_frames / e->rate != sl->pos_emitted) {
            emit_position(e, sl);
        }
    }
    refresh_snapshot(e);
    view_sync(e);
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
    out->replayer = sl ? sl->rp_name : NULL;
    out->render_kind = sl ? (sl->rp_render != NULL) : 0;
    out->replayer_render_calls = sl ? sl->rp_render_calls : 0;
    out->rendered_frames = sl ? sl->rendered_frames : 0;
    out->seek_path = e->seek_path;
    out->seek_skipped_frames = e->seek_skipped;
    out->songs_opened = e->songs_opened;
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
        while (sl->rp_render != NULL && done < frames && !sl->ended) {
            uint32_t want = frames - done;
            uint32_t got = sl->rp_render(sl->rp_state, stereo + (size_t)done * 2u, want);
            sl->rp_render_calls++;
            sl->rendered_frames += got;
            sl->song_frames += got;
            done += got;
            sample_position(sl);
            if (got < want) { /* short read: the song ended */
                sl->ended = 1;
            }
        }
        while (sl->rp_render == NULL && done < frames && !sl->ended) {
            uint32_t n;
            if (sl->frames_left == 0) {
                sel_tick_result r;
                sl->rp_tick(sl->rp_state, &r);
                sl->ticks++;
                sl->frames_left = sel_scheduler_frames(&sl->sched, r.length);
                /* A looping song ends after its first pass (repeat count 1, as the
                 * nuade core does); REPEAT will make this configurable. */
                if (r.flags & (SEL_TICK_ENDED | SEL_TICK_LOOPED)) {
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
