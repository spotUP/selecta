/* The engine: one state machine behind two single-producer single-consumer
 * queues (design doc section 2). Commands go in with sel_engine_send,
 * sel_engine_pump dispatches them and hands the resulting events to a
 * sink. All memory is taken in sel_engine_create; send, pump and render
 * never allocate. */
#ifndef SELECTA_ENGINE_H
#define SELECTA_ENGINE_H

#include <selecta/host.h>
#include <selecta/protocol.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>

SEL_EXTERN_C_BEGIN

typedef enum sel_play_state { SEL_STOPPED = 0, SEL_PLAYING, SEL_PAUSED } sel_play_state;

/* Option names from design section 3, in table order. */
#define SEL_OPTIONS(X)                                                                        \
    X(CONSUME, "consume")                                                                     \
    X(SINGLE, "single")                                                                       \
    X(RANDOM, "random")                                                                       \
    X(REPEAT, "repeat")                                                                       \
    X(CROSSFADE, "crossfade")                                                                 \
    X(STOP_AFTER_CURRENT, "stop-after-current")                                               \
    X(STOP_AFTER_QUEUE, "stop-after-queue")                                                   \
    X(FOLLOW, "follow")                                                                       \
    X(HOOK_COMMAND, "hook_command")

typedef enum sel_option_id {
#define SEL_OPTION_ENUM(id, name) SEL_OPT_##id,
    SEL_OPTIONS(SEL_OPTION_ENUM)
#undef SEL_OPTION_ENUM
        SEL_OPTION_COUNT
} sel_option_id;

#define SEL_OPTION_VALUE_MAX 64
#define SEL_OUTPUT_NAME_MAX 32

/* One play-queue entry. The path is `queue_text + path_offset` of the snapshot. */
typedef struct sel_queue_entry {
    uint32_t id;
    uint32_t subsong_plus1; /* 0: the replayer's default subsong */
    uint32_t path_offset;   /* into the text arena */
} sel_queue_entry;

/* The view model (design doc section 9b), regular style: one flat list over
 * the queue or the browse listing, a cursor and the visible window. */
typedef enum sel_view_source { SEL_VIEW_QUEUE = 0, SEL_VIEW_BROWSE = 1 } sel_view_source;
typedef enum sel_row_kind { SEL_ROW_DIR = 0, SEL_ROW_FILE, SEL_ROW_QUEUE } sel_row_kind;

typedef struct sel_view_row {
    uint32_t kind;  /* sel_row_kind */
    uint32_t id;    /* queue entry id (SEL_ROW_QUEUE), else the index in the listing */
    uint32_t depth; /* 0 in the regular style */
    const char* text;
} sel_view_row;

/* rows[0..row_n) are the visible window: listing indices window_start and up.
 * Row and path pointers are valid until the next pump. */
typedef struct sel_view_state {
    uint32_t source;       /* sel_view_source */
    uint32_t style;        /* 0: regular */
    uint32_t row_count;    /* rows in the whole list */
    uint32_t cursor;       /* index into the whole list */
    uint32_t cursor_id;    /* id of the cursor row (see sel_view_row.id) */
    uint32_t window_start; /* index of rows[0] */
    uint32_t window_rows;  /* requested window height */
    uint32_t row_n;        /* rows[] entries filled */
    const sel_view_row* rows;
    const char* browse_path; /* directory the browse listing shows */
} sel_view_state;

typedef struct sel_song_state {
    uint32_t queue_id; /* 0 = opened outside the queue */
    uint32_t subsong, subsong_count;
    uint32_t duration_ms; /* 0 = unknown */
    uint32_t position_ms;
    sel_rp_position pattern_pos; /* zeros when not reported */
    const sel_tag* meta;         /* SEL_META_* list, valid until next pump */
    const char* path;            /* NULL when no song is open; valid until next pump */
} sel_song_state;

/* The snapshot front ends render. Fields are only appended. Everything below
 * `options` is refreshed at the end of every sel_engine_pump. */
typedef struct sel_state {
    sel_play_state play;
    int32_t volume; /* 0..100 */
    char output[SEL_OUTPUT_NAME_MAX];
    char options[SEL_OPTION_COUNT][SEL_OPTION_VALUE_MAX];
    sel_song_state song;
    const sel_queue_entry* queue;
    const char* queue_text; /* path arena the entries' path_offset points into */
    uint32_t queue_count, queue_playing, queue_version; /* queue_playing: id, 0 = none */
    const sel_view_state* view; /* NULL until the view model exists */
} sel_state;

typedef struct sel_engine_config {
    uint32_t cmd_queue_depth; /* 0: default 64 */
    uint32_t evt_queue_depth; /* 0: default 256 */
    /* Plugin enumeration (sel_registry_count / sel_registry_get). The engine
     * core does not link the registry; the front end hands these in. NULL:
     * OPEN answers ERROR no_replayer. */
    uint32_t (*registry_count)(void);
    const sel_tag* (*registry_get)(uint32_t index);
    /* Queue sizes, allocated once in sel_engine_create. 0: scaled from the
     * SEL_MACHINE_FAST_RAM tag of `machine` (the list sel_machine_probe
     * returns; the front end probes, the engine does not), or the smallest
     * defaults when `machine` is NULL. */
    uint32_t queue_capacity;
    uint32_t queue_text_bytes;
    /* Browse buffer (LIBRARY_BROWSE), allocated once; 0: same as the queue. */
    uint32_t browse_entries;
    uint32_t browse_text_bytes;
    const sel_tag* machine;
} sel_engine_config;

typedef struct sel_engine sel_engine;

/* Called by sel_engine_pump once per delivered event. */
typedef void (*sel_event_sink)(void* ctx, const sel_message* evt);

sel_engine* sel_engine_create(const sel_host* host, const sel_engine_config* cfg);
void sel_engine_destroy(sel_engine* e);

/* Enqueues a command. String and tag arguments are not copied: they must
 * stay valid until the next sel_engine_pump. Returns 0 when the queue is full. */
int sel_engine_send(sel_engine* e, const sel_message* cmd);

/* Enqueues an ERROR event on behalf of the protocol layer (for a line the
 * codec could not decode). `detail` is copied. */
void sel_engine_report_error(sel_engine* e, const char* code, const char* detail);

/* Drains the command queue, dispatches, delivers events. Returns the
 * number of events delivered. */
uint32_t sel_engine_pump(sel_engine* e, sel_event_sink sink, void* ctx);

const sel_state* sel_engine_state(const sel_engine* e);
const char* sel_play_state_name(sel_play_state s);
const char* sel_option_name(sel_option_id id);

/* Sample rate and resampler (a SEL_MIX_RESAMPLE_* value) used by the next
 * OPEN. Rate 0 keeps 44100. Defaults: 44100 Hz, linear. */
void sel_engine_set_mix(sel_engine* e, uint32_t rate, uint32_t resampler);

typedef struct sel_engine_stats {
    uint32_t ticks;        /* replayer ticks run */
    uint32_t voice_ops;    /* calls through the voices API */
    uint32_t mixed_frames; /* frames mixed */
    const char* replayer;            /* name of the open song's replayer, NULL: none */
    uint32_t render_kind;            /* 1: the replayer renders itself (no ticks, no voices) */
    uint32_t replayer_render_calls;  /* RP_RENDER calls (RENDER kind) */
    uint32_t rendered_frames;        /* frames RP_RENDER produced (RENDER kind) */
    uint32_t seek_path;           /* last SEEK: 0 none, 1 native (RP_SEEK), 2 restart-and-skip */
    uint32_t seek_skipped_frames; /* frames the last restart-and-skip ran through */
} sel_engine_stats;
void sel_engine_stats_get(const sel_engine* e, sel_engine_stats* out);

/* Fills `frames` stereo frames: the open song's mix while PLAYING, else
 * silence. Counts calls. Never allocates. */
/* Marks the calling thread as the audio thread (call it from the output's
 * pull callback on its first run). Debug builds then assert that
 * sel_engine_render runs only there and send/pump/destroy never do. */
void sel_engine_mark_audio_thread(sel_engine* e);
uint32_t sel_engine_render(sel_engine* e, int16_t* stereo, uint32_t frames);
uint32_t sel_engine_render_calls(const sel_engine* e);

/* 1 when the engine answers this verb with real behaviour, 0 when it
 * replies ERROR not_implemented. The ratchet (T8) counts the zeros. */
int sel_engine_cmd_implemented(uint32_t cmd_id);

SEL_EXTERN_C_END

#endif /* SELECTA_ENGINE_H */
