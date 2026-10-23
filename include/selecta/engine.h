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
#include <selecta/viz.h>
#include <selecta/voices.h>

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
    X(HOOK_COMMAND, "hook_command")                                                           \
    X(SCOPE, "scope")                                                                         \
    X(LOOPS, "loops")                                                                         \
    X(FADE_MS, "fade_ms")                                                                     \
    X(MAX_SONG_MS, "max_song_ms")                                                             \
    X(SILENCE_MS, "silence_ms")

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
    uint32_t flags;         /* SEL_QUEUE_FLAG_* */
} sel_queue_entry;
/* A divider row shows its name (the path text) and is never played. A hidden
 * row plays but the regular style does not show it (Hippo stealth). */
#define SEL_QUEUE_FLAG_DIVIDER 1u
#define SEL_QUEUE_FLAG_HIDDEN 2u

/* The view model (design doc section 9b), regular style: one flat list over
 * the queue or the browse listing, a cursor and the visible window. */
typedef enum sel_view_source { SEL_VIEW_QUEUE = 0, SEL_VIEW_BROWSE = 1, SEL_VIEW_QUERY = 2 } sel_view_source;
typedef enum sel_row_kind { SEL_ROW_DIR = 0, SEL_ROW_FILE, SEL_ROW_QUEUE, SEL_ROW_NODE } sel_row_kind;
/* Styles over the queue: regular (flat), tree (group nodes that expand to
 * their songs) and drilldown (columns: pick a value per level, the next
 * column lists what matches). The browse source is always regular. */
typedef enum sel_view_style { SEL_VIEW_STYLE_REGULAR = 0, SEL_VIEW_STYLE_TREE, SEL_VIEW_STYLE_DRILLDOWN } sel_view_style;

typedef struct sel_view_row {
    uint32_t kind;  /* sel_row_kind */
    uint32_t id;    /* queue entry id (SEL_ROW_QUEUE), node id (SEL_ROW_NODE: 1.. in order of first
                       appearance, stable while the group has songs), else the index in the listing */
    uint32_t depth; /* 0 in the regular style; songs under a tree node are 1 */
    const char* text;
    uint32_t count;    /* SEL_ROW_NODE: songs the node holds */
    uint32_t expanded; /* SEL_ROW_NODE: 1 when its songs are listed below it */
} sel_view_row;

/* rows[0..row_n) are the visible window: listing indices window_start and up.
 * Row and path pointers are valid until the next pump. */
typedef struct sel_view_state {
    uint32_t source;       /* sel_view_source */
    uint32_t style;        /* sel_view_style */
    uint32_t row_count;    /* rows in the whole list */
    uint32_t cursor;       /* index into the whole list */
    uint32_t cursor_id;    /* id of the cursor row (see sel_view_row.id); tree and
                              drilldown keep the song's id while its node row shows */
    uint32_t window_start; /* index of rows[0] */
    uint32_t window_rows;  /* requested window height */
    uint32_t row_n;        /* rows[] entries filled */
    const sel_view_row* rows;
    const char* browse_path; /* directory the browse listing shows */
    const char* group;       /* grouping key: "folder" or "format" */
    uint32_t drill_n;        /* drilldown: values picked so far */
    const char* drill[2];    /* the picked values, column order */
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

/* Pattern window of the playing tracker song (plan M1.3 I101). A front end asks
 * with sel_engine_pattern_request; the audio thread, which owns the replayer,
 * fills it between render calls and the next pump publishes it in
 * sel_state.pattern. */
#define SEL_PATTERN_ROWS 48u     /* rows a window holds */
#define SEL_PATTERN_CHANNELS 16u /* channels a window holds */
typedef struct sel_pattern_view {
    uint32_t seq;      /* the request this answers; 0: none answered yet */
    uint32_t valid;    /* 1: the song has pattern data and the window below is filled */
    uint32_t order, row_lo, row_n; /* the window: rows row_lo .. row_lo + row_n - 1 of the pattern at `order` */
    uint32_t channels; /* columns filled (the song's channels, at most SEL_PATTERN_CHANNELS) */
    sel_rp_tracker_info info; /* the pattern's geometry: orders, patterns, rows, channels */
    sel_rp_cell cell[SEL_PATTERN_ROWS][SEL_PATTERN_CHANNELS]; /* [row - row_lo][channel] */
} sel_pattern_view;

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
    const sel_view_state* view; /* the view model, valid until the next pump */
    const sel_viz_frame* viz;   /* newest visualisation frame (seq 0: none yet), valid until the next pump */
    int32_t pitch_cents;        /* PITCH: -1200..1200 */
    int32_t tempo_percent;      /* TEMPO: -50..100 */
    uint32_t pitch_q16;         /* 2^(pitch_cents / 1200), Q16 */
    uint32_t mute_mask;         /* bit k set: voice k + 1 is muted (MUTE, SOLO) */
    uint32_t mixed_path;        /* 1: the engine mixes (DSP, crossfade); 0: the output plays voices itself */
    const sel_pattern_view* pattern; /* newest answered pattern window, valid until the next pump */
    uint32_t led_on;            /* FILTER_LED: 1 when the LED filter stage is on */
    uint32_t stereo_width;      /* MIX_STEREO_WIDTH: 0 (mono) to 100 (unchanged) */
} sel_state;

struct sel_library;
/* Counters of the last LIBRARY_SCAN (test sentinels; probed > 0 proves the scan code ran). */
typedef struct sel_scan_info {
    uint32_t scanned_files, skipped_files, scan_batches, probed, dirs;
    uint32_t archives;        /* archive files opened */
    uint32_t archive_members; /* members written as songs */
} sel_scan_info;

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
    /* Directory of prefs.conf (see sel_config_dir). NULL: prefs are not read
     * or written, they stay at their defaults for this run. */
    const char* config_dir;
    /* Library database (src/library/db); the front end opens and closes it. NULL:
     * LIBRARY_SEARCH, LIBRARY_SCAN, LIBRARY_FETCH, PLAY_QUERY, QUEUE_ADD_QUERY and VIEW_SAVE answer ERROR no_library. */
    struct sel_library* library;
} sel_engine_config;

typedef struct sel_engine sel_engine;

void sel_engine_scan_info(const sel_engine* e, sel_scan_info* out);

/* Counters of the last LIBRARY_SYNC (I161; test sentinels: listing_rows > 0 proves the sync code ran). */
typedef struct sel_sync_info {
    uint32_t listing_rows, skipped_rows, changelogs;
    uint32_t cl_added, cl_removed, cl_moved, cl_updated;
    uint32_t new_this_week;
    uint32_t songlengths, stil_entries, subtunes; /* hvsc sync (I162) */
    uint32_t hippo_lists, prg_files, prg_entries, prg_dividers, prg_stealth, prg_subsongs; /* hippo sync (I163) */
} sel_sync_info;
void sel_engine_sync_info(const sel_engine* e, sel_sync_info* out);

/* The config directory: $XDG_CONFIG_HOME/selecta, else ~/.config/selecta;
 * ENVARC:Selecta on the Amiga. Writes the path to `out`; 0 when it cannot be
 * determined or does not fit. The directory is not created. */
int sel_config_dir(char* out, size_t cap);

/* Creates `dir` and missing parents (src/core/<os>/config_mkdir.c). 1 when it exists afterwards. */
int sel_config_mkdir(const char* dir);

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

/* The shared protocol line parser (src/protocol/parse.c): decodes one command
 * line into `pool` (the strings stay in it, so it must live until the next
 * sel_engine_pump) and sends it, or raises the ERROR event. Returns
 * SEL_CODEC_OK or the codec error. */
int sel_engine_send_line(sel_engine* e, const char* line, char* pool, size_t poolcap);

/* Drains the command queue, dispatches, delivers events. Returns the
 * number of events delivered. */
uint32_t sel_engine_pump(sel_engine* e, sel_event_sink sink, void* ctx);

const sel_state* sel_engine_state(const sel_engine* e);
const char* sel_play_state_name(sel_play_state s);
const char* sel_option_name(sel_option_id id);

/* Sample rate and resampler (a SEL_MIX_RESAMPLE_* value) used by the next
 * OPEN. Rate 0 keeps 44100. Defaults: 44100 Hz, linear. */
void sel_engine_set_mix(sel_engine* e, uint32_t rate, uint32_t resampler);

/* Tells the engine what the chosen output is (its hook list): an output with
 * SEL_OUT_VOICES plays voices itself, so no DSP chain runs; SEL_OUT_HEADPHONES
 * turns the crossfeed on unless the pref dsp.crossfeed is "off". Call after
 * sel_engine_set_mix and the registry hook-up, before the song starts. */
void sel_engine_set_output(sel_engine* e, const sel_tag* out_hooks);
/* An output that plays voices itself (SEL_OUT_VOICE_BACKEND) hands its backend here before the song starts;
 * the replayer's voice calls then reach it instead of the mixer. NULL restores the mixer. */
void sel_engine_set_voice_sink(sel_engine* e, const sel_voices* sink);

/* Routing table (I121). The sink is used only when the replayer is VOICE kind (voice_count > 0) and
 * voice_count <= the output's SEL_OUT_VOICE_MAX (0: no limit; the Paula outputs say 4); else the mixer. */
enum { SEL_ROUTE_MIXER = 0, SEL_ROUTE_DIRECT = 1 };
uint32_t sel_engine_voice_route(uint32_t voice_count, int has_sink, uint32_t voice_max);
/* Where the song started last went: 1 = the output's voice sink, 0 = the mixer. */
uint32_t sel_engine_routed_direct(const sel_engine* e);

/* LED filter on/off and stereo width percent (0 mono, 100 unchanged), applied
 * to the mixed output before the DSP chain (I127, I128). Call before the
 * audio thread renders. Both stages are bypassed while the output is real
 * Paula: the Paula output calls sel_engine_set_paula_output(e, 1). */
void sel_engine_set_stage(sel_engine* e, uint32_t led_on, uint32_t width_percent);
void sel_engine_set_paula_output(sel_engine* e, uint32_t on);

/* Per-song replayer options, "plugin.key=value,..." (D-M2-8): beats the global pref rp.config. They live in
 * the library DB (sticker rp.config of the song key, sel_library_song_config_set) and are read at OPEN (I170). */

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
    uint32_t songs_opened;        /* songs started by OPEN or the queue (T19) */
    uint32_t history_writes;      /* history rows the engine wrote on song end or skip (T78 sentinel) */
    uint32_t library_searches;    /* queries sent to the library worker (T64) */
    uint32_t library_fts_queries; /* of those, queries the FTS5 index served (T64) */
    uint32_t result_count;         /* keys held by the last query result (T64, T65) */
    uint32_t rp_config_applied;   /* replayer option values answered by the rp.config pref or the song override (T61) */
    uint32_t undo_depth, redo_depth; /* queue edits that UNDO / REDO can step over (T28) */
    uint32_t dsp_blocks;             /* render blocks that ran at least one DSP plugin (T47) */
    uint32_t xfade_frames;           /* frames mixed with two songs at once (T44) */
    uint32_t slots_mixed;            /* most songs mixed at once so far */
    uint32_t mixed_path;             /* 1: the engine mixes, 0: the output owns voices */
    uint32_t mutes;                  /* mute mask changes (MUTE, SOLO) (T39) */
    uint32_t fades;                  /* end fades started (loop count, max length) (T39) */
    uint32_t crossfeed_on;           /* 1: the crossfeed DSP is in the chain (T39) */
    uint32_t sleep_armed;            /* 1: a sleep timer is counting down (T39) */
    uint32_t led_on, led_frames, led_cutoff_hz;       /* LED filter state, frames filtered, designed -3 dB point (T53) */
    uint32_t width_percent, width_frames, width_mono; /* stereo width, frames processed, 1: width 0 (T53) */
} sel_engine_stats;
void sel_engine_stats_get(const sel_engine* e, sel_engine_stats* out);
/* Key `i` of the last LIBRARY_SEARCH, PLAY_QUERY or QUEUE_ADD_QUERY result (byte order of the key), NULL past the end. */
const char* sel_engine_result_key(const sel_engine* e, uint32_t i);

/* "regular", "tree" or "drilldown" for a sel_view_style value; "unknown" otherwise. */
const char* sel_view_style_label(uint32_t style);
/* "queue", "browse" or "query" for a sel_view_source value; "unknown" otherwise. */
const char* sel_view_source_label(uint32_t source);
/* Key of the song under the cursor in the queue or query list; NULL on a directory row or an empty list. */
const char* sel_engine_view_cursor_key(const sel_engine* e);

/* Fills `frames` stereo frames: the open song's mix while PLAYING, else
 * silence. Counts calls. Never allocates. */
/* Marks the calling thread as the audio thread (call it from the output's
 * pull callback on its first run). Debug builds then assert that
 * sel_engine_render runs only there and send/pump/destroy never do. */
void sel_engine_mark_audio_thread(sel_engine* e);
uint32_t sel_engine_render(sel_engine* e, int16_t* stereo, uint32_t frames);
uint32_t sel_engine_render_calls(const sel_engine* e);
/* The running end fade of the playing song: 1 and its position and length in frames, else 0. */
int sel_engine_fade(const sel_engine* e, uint32_t* pos, uint32_t* total);

/* Tracker pattern data of the open song (RP_TRACKER_INFO / RP_TRACKER_CELLS).
 * Return 0 when no song is open or the replayer has no pattern data. They
 * call the replayer directly: use them only from the thread that owns it
 * (the script driver); there is no audio-thread request path yet. */
int sel_engine_tracker_info(const sel_engine* e, uint32_t order, sel_rp_tracker_info* out);
uint32_t sel_engine_tracker_cells(const sel_engine* e, uint32_t order, uint32_t channel, uint32_t row_lo,
                                  uint32_t row_hi, sel_rp_cell* out, uint32_t cap);

/* Asks for a pattern window: rows row_lo .. row_lo + rows - 1 (at most
 * SEL_PATTERN_ROWS) of the pattern at `order`. Pump thread only. The audio
 * thread answers after its next render call and the following pump publishes
 * the answer in sel_state.pattern (same `seq` as the return value). Returns the
 * request number, or 0 when the previous request is still unanswered (ask
 * again after the next pump). */
uint32_t sel_engine_pattern_request(sel_engine* e, uint32_t order, uint32_t row_lo, uint32_t rows);

/* 1 when the engine answers this verb with real behaviour, 0 when it
 * replies ERROR not_implemented. The ratchet (T8) counts the zeros. */
int sel_engine_cmd_implemented(uint32_t cmd_id);

SEL_EXTERN_C_END

#endif /* SELECTA_ENGINE_H */
