/* Engine shell (M0a): transport state, volume, output name, options,
 * batches. Every verb in commands.def reaches dispatch; the ones outside
 * SEL_CMD_IMPLEMENTED answer ERROR not_implemented and are counted by the
 * ratchet test. */
#include <selecta/engine.h>

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
    X(END)

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
    copy_str(e->state.output, SEL_OUTPUT_NAME_MAX, "null");
    if (!sel_spsc_init(&e->cmd_q, host, cmd_depth) || !sel_spsc_init(&e->evt_q, host, evt_depth)) {
        sel_engine_destroy(e);
        return NULL;
    }
    return e;
}

void sel_engine_destroy(sel_engine* e) {
    if (e) {
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

uint32_t sel_engine_render(sel_engine* e, int16_t* stereo, uint32_t frames) {
    e->render_calls++;
    if (stereo && frames) {
        memset(stereo, 0, (size_t)frames * 2u * sizeof(int16_t));
    }
    return frames;
}

uint32_t sel_engine_render_calls(const sel_engine* e) {
    return e->render_calls;
}
