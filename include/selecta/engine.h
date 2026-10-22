/* The engine: one state machine behind two single-producer single-consumer
 * queues (design doc section 2). Commands go in with sel_engine_send,
 * sel_engine_pump dispatches them and hands the resulting events to a
 * sink. All memory is taken in sel_engine_create; send, pump and render
 * never allocate. */
#ifndef SELECTA_ENGINE_H
#define SELECTA_ENGINE_H

#include <selecta/host.h>
#include <selecta/protocol.h>

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

typedef struct sel_state {
    sel_play_state play;
    int32_t volume; /* 0..100 */
    char output[SEL_OUTPUT_NAME_MAX];
    char options[SEL_OPTION_COUNT][SEL_OPTION_VALUE_MAX];
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

/* Read-only queue view for front ends and tests (the snapshot fields of
 * SNAPSHOT_QUEUE will fold these in). playing_id is 0 when no queue entry
 * is playing. sel_engine_queue_id_at returns 0 past the end. */
uint32_t sel_engine_queue_version(const sel_engine* e);
uint32_t sel_engine_queue_count(const sel_engine* e);
uint32_t sel_engine_queue_playing_id(const sel_engine* e);
uint32_t sel_engine_queue_id_at(const sel_engine* e, uint32_t index);

/* 1 when the engine answers this verb with real behaviour, 0 when it
 * replies ERROR not_implemented. The ratchet (T8) counts the zeros. */
int sel_engine_cmd_implemented(uint32_t cmd_id);

SEL_EXTERN_C_END

#endif /* SELECTA_ENGINE_H */
