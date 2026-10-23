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
#include <selecta/machine.h>

#include <selecta/plugin.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>

#include "mixer.h"
#include "stage.h"
#include "loudness_gain.h"
#include "dsp_chain.h"
#include "timer.h"
#include "pitch_table.h"
#include "xfade_table.h"
#include "history.h"
#include "queue.h"
#include "playlist/playlist.h"
#include "shuffle.h"
#include "local.h"
#include "export/export.h"
#include "prefs.h"
#include "view.h"
#include "db.h"
#include "xxh32.h"
#include "cache.h"
#include "share.h"
#if !defined(SEL_NO_ANALYSE)
#include "loudness.h"
#endif
#include "sources/local_source.h"
#include "sources/hippo_publish.h"
#include "sources/hippo_sync.h"
#include "sources/hvsc_sync.h"
#include "sources/modland_net.h"
#include "net/devilbox.h"
#include "sources/modland_sync.h"
#include "scheduler.h"
#include "spsc.h"
#include "thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <time.h>

/* The verbs with real behaviour in M0a. Adding a row here is how a later
 * plan lowers tests/ratchet/unimplemented.txt. */
#define SEL_CMD_IMPLEMENTED(X)                                                                \
    X(PREF_GET)                                                                               \
    X(PREF_SET)                                                                               \
    X(FILTER_LED)                                                                             \
    X(MIX_STEREO_WIDTH)                                                                       \
    X(DSP)                                                                                    \
    X(PLAY)                                                                                   \
    X(PAUSE)                                                                                  \
    X(PAUSE_PLAYBACK)                                                                         \
    X(STOP)                                                                                   \
    X(VOLUME)                                                                                 \
    X(MUTE)                                                                                   \
    X(PITCH)                                                                                  \
    X(TEMPO)                                                                                  \
    X(SOLO)                                                                                   \
    X(OUTPUT)                                                                                 \
    X(SLEEP_TIMER)                                                                            \
    X(ALARM)                                                                                  \
    X(REPEAT)                                                                                 \
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
    X(PLAYLIST_PUBLISH)                                                                       \
    X(SAMPLE_LIST)                                                                            \
    X(SAMPLE_EXPORT)                                                                          \
    X(QUEUE_PLAY_NEXT)                                                                        \
    X(LIBRARY_BROWSE)                                                                         \
    X(LIBRARY_SEARCH)                                                                         \
    X(PLAY_QUERY)                                                                             \
    X(QUEUE_ADD_QUERY)                                                                        \
    X(VIEW_SAVE)                                                                              \
    X(VIEW_SOURCE)                                                                            \
    X(LIBRARY_FETCH)                                                                          \
    X(LIBRARY_SCAN)                                                                           \
    X(LIBRARY_SYNC)                                                                           \
    X(LIKE)                                                                                   \
    X(FLAG)                                                                                   \
    X(RATE) X(VOTE) X(ANALYSE) X(CUE_SET) X(CUE_GOTO) X(SHARE) X(RADIO) X(STICKER_SET) X(STICKER_GET) X(STICKER_DEL) X(STICKER_LIST) \
    X(FOLDER_ADD) X(FOLDER_MOVE) X(FOLDER_DEL) X(PLAYLIST_MOVE) \
    X(KEEP)                                                                                   \
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
#define SEL_SILENCE_LEVEL 16 /* |sample| below this counts as silent */
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

/* Why a song ended (the SONG_ENDED reason argument). */
typedef enum sel_end_reason { SEL_END_NONE = 0, SEL_END_END, SEL_END_LOOP, SEL_END_MAX_LENGTH, SEL_END_SILENCE } sel_end_reason;
static const char* const k_end_reason_names[] = {"", "end", "loop", "max_length", "silence"};

/* Playback policy state of one song (design section 3, loop detect and fade); audio side. */
typedef struct sel_playpol {
    uint32_t loops_seen;  /* LOOPED ticks seen */
    uint32_t fade_on;     /* a fade is running */
    uint32_t fade_pos;    /* frames faded so far */
    uint32_t fade_total;  /* fade length in frames */
    uint32_t fade_reason; /* sel_end_reason the fade ends with */
    uint32_t reason;      /* sel_end_reason, set with ended */
    uint32_t silent;      /* consecutive silent frames */
} sel_playpol;

typedef struct sel_slot {
    uint32_t busy; /* pump side: the slot holds a song until RP_CLOSE ran */
    void* rp_state;
    sel_rp_close_fn rp_close;
    sel_rp_start_fn rp_start;
    sel_rp_tick_fn rp_tick;
    sel_rp_metadata_fn rp_metadata;
    sel_rp_seek_fn rp_seek;         /* optional: native seek, else restart-and-skip */
    sel_rp_position_fn rp_position; /* optional: order/row/pattern */
    sel_rp_tracker_info_fn rp_tracker_info;   /* optional: pattern geometry */
    sel_rp_tracker_cells_fn rp_tracker_cells; /* optional: pattern cells */
    sel_rp_sample_get_fn rp_sample_get;       /* optional: sample table (I97) */
    sel_rp_mute_fn rp_mute;                   /* optional, RENDER kind: channel mute */
    sel_rp_set_speed_fn rp_set_speed;         /* optional, RENDER kind: pitch and tempo factors */
    uint32_t applied_pitch, applied_tempo;    /* audio side: factors last handed to rp_set_speed */
    uint32_t applied_mute;                    /* audio side: mask last handed to rp_mute */
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
    uint32_t scrobbled;   /* pump side: SCROBBLE_DUE was emitted for this song (I177) */
    sel_playpol pol;      /* loop count, fade and silence state of this song */
    uint32_t loud_active;      /* loudness gain applies to this song (I189a); set by prepare_slot before the swap */
    uint32_t loud_gain_q16;    /* this song's gain, 16.16 */
    const char* loud_source;   /* "off", "none", "stored", "provisional" or "leveler" */
    uint32_t lev_on;           /* auto mode, no gain known: the adaptive leveler runs on this slot (I190) */
    uint32_t lev_done;         /* the leveler's estimate was written back (command thread) */
    sel_leveler lev;           /* audio thread writes; the command thread reads it once the slot is returned */
} sel_slot;

struct sel_engine {
    const sel_host* host;
    sel_host rp_host;               /* copy of host whose config_get answers per-replayer options (I134) */
    char rp_song_config[256];       /* per-song override list "plugin.key=value,...", read from the library DB sticker rp.config at OPEN (D-M2-8, I170) */
    char hist_key[SEL_DEFERRED_PATH_MAX]; /* key of the song being played, "" none: its end or skip goes to the history table (I170) */
    char vote_song[SEL_DEFERRED_PATH_MAX]; /* song key of the last VOTE request: the count the service answers is stored under it (I174a) */
    char vote_mkey[13];                    /* UADE key of that request, matched against the answer */
    unsigned vote_seen;                    /* vote result sequence already stored */
    uint32_t votes_generation;             /* moves when a count is stored; the view re-reads its vote texts */
    uint32_t scrobble_due;   /* SCROBBLE_DUE events emitted (sentinel of the I177 rule) */
    uint32_t history_writes; /* history rows written (sentinel of the I170 writer) */
    uint32_t cue_sets, cue_gotos, sticker_ops; /* sentinels of I175 / I176 */
    uint32_t analyse_jobs, analyse_speed, analyse_gated; /* I187 sentinels: jobs stored, x real time, R128 (not fallback) */
    int32_t analyse_lufs_cdb, analyse_gain_cdb, analyse_peak_cdb;
    uint32_t folder_view_rows;                 /* rows the folders view source built (I179 sentinel) */
    uint32_t folder_ops;                       /* FOLDER_* / PLAYLIST_MOVE commands run against the library (I178) */
    uint32_t links_parsed, links_made;         /* share links OPEN parsed / SHARE wrote (I180 sentinels) */
    char panel[1024];                          /* catalogue rows of the open song, "label\0value\0" pairs (I185) */
    uint32_t panel_rows, panel_loads;          /* rows in `panel` / library lookups made for it (I185 sentinel) */
    const void* panel_state;                   /* replayer state the panel was loaded for (reload on a new song) */
    char radio_seed[SEL_DEFERRED_PATH_MAX]; /* RADIO seed song (queue path or library key), "" radio off (I186) */
    char radio_last[SEL_DEFERRED_PATH_MAX]; /* last song the radio appended, left out of the next draw */
    uint32_t radio_rng;                     /* radio draw state, stepped once per appended song */
    uint32_t radio_appends;                 /* songs the radio appended to the queue (I186 sentinel) */
    uint32_t radio_min_ahead;               /* fewest songs queued after the playing one, measured after each top-up */
    uint32_t skip_excluded;  /* skip-flagged queue rows left out of a random order (sentinel of I171) */
    char rp_value[SEL_PREF_VALUE_MAX]; /* answer of the last rp_config_get */
    uint32_t rp_config_applied;     /* option values that came from an override layer (T61) */
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
    /* Playback policy, parsed from the options by set_option; the audio side reads them. */
    uint32_t repeat;  /* 0 off, 1 one, 2 all */
    uint32_t loops;   /* LOOPED ticks before the fade starts */
    uint32_t fade_ms; /* 0..30000 */
    uint32_t max_song_ms;
    uint32_t silence_ms;
    uint32_t crossfade_ms; /* 0..10000, 0: gapless */
    sel_timer timer; /* sleep timer and alarm (timer.h) */
    char timer_text[8][16]; /* event strings live until the pump delivers them (ring) */
    uint32_t timer_text_next;
    /* Next song, prepared in the idle slot (plan M1.3 I95). nx and the xf_* fields belong to
     * the audio thread; nx_slot, nx_id and nx_qver to the pump. */
    uint32_t nx;                 /* audio: prepared incoming slot, or NONE */
    uint32_t xf_on, xf_pos, xf_total; /* audio: crossfade running, frames done, frames in all */
    volatile uint32_t xf_reason; /* audio writes before the switch: end reason of the old song */
    uint32_t nx_slot, nx_id, nx_qver; /* pump: slot, queue entry id and queue version of the prepared song */
    uint32_t xfade_frames, slots_mixed; /* audio: counters for @stats */
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
    uint32_t dup_redirects; /* reads that took the best source of the same md5 (D-M3-14, I184) */
    char dup_played[SEL_DEFERRED_PATH_MAX]; /* key of the last best source read */
    char deferred_path[SEL_DEFERRED_PATH_MAX];
    char subsong_path[SEL_DEFERRED_PATH_MAX];
    sel_queue queue;
    sel_queue qview; /* the library query rows the query view source lists */
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
    sel_scan_info scan_info; /* counters of the last LIBRARY_SCAN */
    sel_sync_info sync_info; /* counters of the last LIBRARY_SYNC */
    uint32_t lib_slot;
    sel_tag lib_tags[4][4];                  /* LIBRARY_RESULT tags, a ring like error_detail */
    char lib_path[4][SEL_BROWSE_PATH_MAX];
    uint32_t st_slot;                         /* STICKER answers: ring of 8, like lib_tags */
    sel_tag st_tags[8][5];
    char st_key[8][SEL_DEFERRED_PATH_MAX];
    char st_name[8][128];
    char st_value[8][SEL_STICKER_VALUE_MAX];
    sel_library* lib;     /* the database of the front end; NULL: no library */
    uint32_t pf_id;       /* queue entry id whose song is being prefetched (I168); 0 = none */
    uint32_t pf_qver;     /* queue version that request was made under */
    uint32_t sm_key;      /* smart download plan made for this queue version, playing entry and prefs generation (I169); 0 = none */
    char* res_text;       /* last query result: keys, each NUL terminated */
    uint32_t res_count;
    char* res_meta;       /* last query result: title, artist, format, duration per key, each NUL terminated */
    uint32_t res_meta_used;
    char* qmeta;          /* snapshot of res_meta taken with qview (VIEW_SOURCE query) */
    uint32_t* qmeta_off;  /* qmeta offset of each qview row; UINT32_MAX: no metadata */
    uint32_t qmeta_rows;
    uint32_t searches, fts_queries;
    char lib_err[4][24]; /* query error codes of the library worker, a ring (the event keeps the pointer) */
    uint32_t lib_err_slot;
    sel_view view;
    sel_viz_tap viz;       /* visualisation taps; the audio thread writes, the pump reads */
    uint32_t viz_spectrum; /* capability: the spectrum is computed (CPU 68030 and up) */
    sel_dsp_chain dsp;
    sel_mix_stage stage; /* LED filter and stereo width, before the DSP chain (I127, I128) */
    sel_loudness_gain loud; /* the -1 dBFS limiter on the mixed output, after the DSP chain (I189); the gain is per slot */
    uint32_t loud_ref;       /* audio thread: this render call's reference gain, 16.16; the limiter stage applies it (I189b) */
    uint32_t lev_frames;     /* frames the adaptive leveler measured (sentinel of I190) */
    uint32_t lev_writes;     /* provisional gains written back to the library (I190) */
    uint32_t loud_mixed;     /* audio thread: a slot with a loudness gain was mixed in this render call */
    uint32_t mutes; /* mute mask changes, for @stats (T39) */
    uint32_t fades; /* end fades started; audio side (T39) */
    uint32_t mixed_path; /* 1: the engine mixes (a DSP chain can run); 0: the output plays voices itself */
    const sel_voices* voice_sink; /* the output's own voice backend (Paula-direct), or NULL: voices go to the mixer */
    uint32_t voice_max;           /* most voices voice_sink plays (SEL_OUT_VOICE_MAX); 0: no limit */
    uint32_t route_direct;        /* routing of the song started last: 1 = voice_sink, 0 = mixer (sel_engine_voice_route) */
    /* Pattern window request (I101). The pump writes pat_order/pat_row_lo/pat_rows and then
     * raises pat_req; the audio thread fills pat_res between render calls and sets pat_ack
     * equal to pat_req; the pump copies pat_res to pat_pub and points the snapshot at it.
     * No new request is written while pat_ack differs from pat_req, so the audio thread
     * never reads torn parameters. */
    volatile uint32_t pat_req, pat_ack;
    uint32_t pat_order, pat_row_lo, pat_rows;
    sel_pattern_view pat_res; /* audio writes while pat_ack != pat_req, pump copies after */
    sel_pattern_view pat_pub; /* what the snapshot points at */
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

/* The mask is a state field (the snapshot shows it) and a VOICE event. The
 * audio thread reads it every render call; a 32-bit store is atomic enough. */
static void set_mute_mask(sel_engine* e, uint32_t mask) {
    static const sel_tag no_tags[1] = {{SEL_TAG_END, 0}};
    sel_message m;
    e->state.mute_mask = mask;
    e->mutes++;
    evt_init(&m, SEL_EVT_VOICE);
    evt_uint(&m, mask);
    evt_tags(&m, no_tags);
    emit(e, &m);
}

/* MUTE <voice> on|off|toggle: voices count from 1. A voice beyond 32 is a
 * range error; a voice the loaded song lacks is accepted (the mask outlives
 * the song) so the mask is the same on every replayer. */
static void cmd_MUTE(sel_engine* e, const sel_message* m) {
    uint32_t v = m->args[0].v.u;
    const char* how = m->args[1].v.s;
    uint32_t bit, mask = e->state.mute_mask;
    if (v < 1u || v > 32u) {
        sel_engine_report_error(e, "range", "MUTE");
        return;
    }
    bit = 1u << (v - 1u);
    if (strcmp(how, "on") == 0) {
        mask |= bit;
    } else if (strcmp(how, "off") == 0) {
        mask &= ~bit;
    } else if (strcmp(how, "toggle") == 0) {
        mask ^= bit;
    } else {
        sel_engine_report_error(e, "range", "MUTE");
        return;
    }
    set_mute_mask(e, mask);
}

/* SOLO <voice>: mute every other voice of the loaded song; SOLO 0 clears the
 * mask. The song's channel count comes from the tracker geometry (RENDER
 * replayers report no voices), else the voice count, else all 32 bits. */
static void cmd_SOLO(sel_engine* e, const sel_message* m) {
    uint32_t v = m->args[0].v.u;
    uint32_t n = 32u;
    sel_rp_tracker_info ti;
    const sel_slot* sl = e->act < SEL_SLOT_COUNT ? &e->slot[e->act] : NULL;
    if (v > 32u) {
        sel_engine_report_error(e, "range", "SOLO");
        return;
    }
    if (sel_engine_tracker_info(e, 0, &ti) && ti.channels > 0u && ti.channels < 32u) {
        n = ti.channels;
    } else if (sl != NULL && sl->busy && sl->rp_info.voice_count > 0u && sl->rp_info.voice_count < 32u) {
        n = sl->rp_info.voice_count;
    }
    set_mute_mask(e, v == 0u ? 0u : (n >= 32u ? 0xFFFFFFFFu : ((1u << n) - 1u)) & ~(1u << (v - 1u)));
}

/* PITCH <cents>: scales every voice's resample step (VOICE kind) or the
 * replayer's pitch factor (RENDER kind, RP_SET_SPEED) by 2^(cents / 1200). */
static void cmd_PITCH(sel_engine* e, const sel_message* m) {
    const sel_arg* a = &m->args[0];
    int32_t c = (a->flags & SEL_ARG_RELATIVE) ? e->state.pitch_cents + a->v.i : a->v.i;
    if (c < -SEL_PITCH_MAX_CENTS || c > SEL_PITCH_MAX_CENTS) {
        sel_engine_report_error(e, "range", "PITCH");
        return;
    }
    e->state.pitch_cents = c;
    e->state.pitch_q16 = sel_pitch_q16[c + SEL_PITCH_MAX_CENTS];
}

/* TEMPO <percent>: -50..100, 100 percent doubles the tick rate. VOICE kind
 * shortens or lengthens the scheduler's tick interval; RENDER kind passes the
 * factor to RP_SET_SPEED. */
static void cmd_TEMPO(sel_engine* e, const sel_message* m) {
    const sel_arg* a = &m->args[0];
    int32_t t = (a->flags & SEL_ARG_RELATIVE) ? e->state.tempo_percent + a->v.i : a->v.i;
    if (t < -50 || t > 100) {
        sel_engine_report_error(e, "range", "TEMPO");
        return;
    }
    e->state.tempo_percent = t;
}

static void cmd_OUTPUT(sel_engine* e, const sel_message* m) {
    uint32_t i, n = e->registry_count != NULL && e->registry_get != NULL ? e->registry_count() : 0;
    copy_str(e->state.output, SEL_OUTPUT_NAME_MAX, m->args[0].v.s);
    for (i = 0; i < n; i++) { /* the output's capability decides the path (SEL_OUT_VOICES: no mixing) */
        const sel_tag* desc = e->registry_get(i);
        const char* pid = (const char*)sel_tag_find(desc, SEL_PLUGIN_ID, 0);
        if (sel_tag_find(desc, SEL_PLUGIN_KIND, 0) == SEL_PLUGIN_KIND_OUTPUT && pid != NULL &&
            strcmp(pid, m->args[0].v.s) == 0) {
            e->mixed_path = sel_tag_find((const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0), SEL_OUT_VOICES, 0) == 0;
        }
    }
    changed(e, PENDING_OUTPUT);
}

static void order_reset(sel_engine* e);

/* Decimal text to a number; 0 when value is not a plain unsigned number. */
static int parse_uint(const char* v, uint32_t max, uint32_t* out) {
    uint32_t n = 0;
    if (*v == '\0') {
        return 0;
    }
    for (; *v; v++) {
        if (*v < '0' || *v > '9' || n > max) {
            return 0;
        }
        n = n * 10u + (uint32_t)(*v - '0');
    }
    if (n > max) {
        return 0;
    }
    *out = n;
    return 1;
}

/* Validates the options that drive the playback policy and caches their numbers
 * for the audio side. Returns 0 (and reports) for a value out of range. */
static int policy_option(sel_engine* e, sel_option_id id, const char* value) {
    uint32_t n = 0;
    switch (id) {
    case SEL_OPT_REPEAT:
        if (strcmp(value, "off") == 0) {
            e->repeat = 0;
        } else if (strcmp(value, "one") == 0) {
            e->repeat = 1;
        } else if (strcmp(value, "all") == 0) {
            e->repeat = 2;
        } else {
            return 0;
        }
        return 1;
    case SEL_OPT_LOOPS:
        if (!parse_uint(value, 1000u, &n) || n == 0) {
            return 0;
        }
        e->loops = n;
        return 1;
    case SEL_OPT_FADE_MS:
        if (!parse_uint(value, 30000u, &n)) {
            return 0;
        }
        e->fade_ms = n;
        return 1;
    case SEL_OPT_MAX_SONG_MS:
        if (!parse_uint(value, 86400000u, &n)) {
            return 0;
        }
        e->max_song_ms = n;
        return 1;
    case SEL_OPT_SILENCE_MS:
        if (!parse_uint(value, 3600000u, &n)) {
            return 0;
        }
        e->silence_ms = n;
        return 1;
    case SEL_OPT_CROSSFADE:
        if (!parse_uint(value, 10000u, &n)) {
            return 0;
        }
        e->crossfade_ms = n;
        return 1;
    default:
        return 1;
    }
}

static void set_option(sel_engine* e, sel_option_id id, const char* value) {
    if (id == SEL_OPT_CROSSFADE && !e->mixed_path && strcmp(value, "0") != 0) {
        /* A Paula-direct output owns its voices: two songs cannot be mixed (plan M1.3 I95). */
        sel_engine_report_error(e, "unsupported_path", "OPTION");
        return;
    }
    if (!policy_option(e, id, value)) {
        sel_engine_report_error(e, "range", k_option_names[id]);
        return;
    }
    if (id == SEL_OPT_RANDOM && strcmp(value, "on") == 0) {
        order_reset(e);
    }
    copy_str(e->state.options[id], SEL_OPTION_VALUE_MAX, value);
    changed(e, PENDING_OPTION_BASE << (uint32_t)id);
}

static void cmd_PLAY(sel_engine* e, const sel_message* m);

/* Local time of day in ms from HOSTQ_LOCAL_TIME; 0 when the host has no clock. */
static uint32_t local_ms(const sel_engine* e) {
    sel_tag ask, ans;
    ask.tag = SEL_HOSTQ_LOCAL_TIME;
    ask.value = 0;
    ans.tag = SEL_TAG_END;
    ans.value = 0;
    if (e->host->query != NULL && e->host->query(e->host, &ask, &ans) && ans.tag == SEL_HOSTQ_LOCAL_TIME) {
        return (uint32_t)ans.value % SEL_DAY_MS;
    }
    return 0;
}

static void emit_timer(sel_engine* e, uint32_t which, const char* value) {
    sel_message m;
    char* text = e->timer_text[e->timer_text_next++ % 8u];
    copy_str(text, sizeof e->timer_text[0], value);
    evt_init(&m, SEL_EVT_OPTIONS_CHANGED);
    evt_str(&m, which == 0u ? "sleep_timer" : "alarm");
    evt_str(&m, text);
    emit(e, &m);
}

/* SLEEP_TIMER minutes: 0 cancels, at most 1440. */
static void cmd_SLEEP_TIMER(sel_engine* e, const sel_message* m) {
    uint32_t min = m->args[0].v.u;
    char buf[16];
    if (min > 1440u) {
        sel_engine_report_error(e, "range", "SLEEP_TIMER");
        return;
    }
    sel_timer_set_sleep(&e->timer, min * 60000u, local_ms(e));
    if (min == 0u && e->timer.ramp_dir == SEL_RAMP_DOWN) {
        sel_timer_ramp_clear(&e->timer); /* cancelled mid-ramp: volume back at once */
    }
    snprintf(buf, sizeof buf, "%lu", (unsigned long)min * 60000ul);
    emit_timer(e, 0, buf);
}

/* ALARM HH:MM|off: one daily alarm on the local clock. */
static void cmd_ALARM(sel_engine* e, const sel_message* m) {
    const char* v = m->args[0].v.s;
    unsigned hh, mm;
    char buf[8];
    if (strcmp(v, "off") == 0) {
        sel_timer_set_alarm(&e->timer, SEL_ALARM_OFF, local_ms(e));
        emit_timer(e, 1, "off");
        return;
    }
    if (strlen(v) != 5u || v[2] != ':' || v[0] < '0' || v[0] > '9' || v[1] < '0' || v[1] > '9' || v[3] < '0' ||
        v[3] > '9' || v[4] < '0' || v[4] > '9') {
        sel_engine_report_error(e, "range", "ALARM");
        return;
    }
    hh = (unsigned)((v[0] - '0') * 10 + (v[1] - '0'));
    mm = (unsigned)((v[3] - '0') * 10 + (v[4] - '0'));
    if (hh > 23u || mm > 59u) {
        sel_engine_report_error(e, "range", "ALARM");
        return;
    }
    sel_timer_set_alarm(&e->timer, (hh * 60u + mm) * 60000u, local_ms(e));
    snprintf(buf, sizeof buf, "%02u:%02u", hh, mm);
    emit_timer(e, 1, buf);
}

/* Pump side of the timers: a finished down ramp pauses and restores the volume
 * gain; a fired alarm resumes (or starts the queue) with a rising ramp. */
static void timer_pump(sel_engine* e) {
    sel_timer* t = &e->timer;
    if (t->sleep_expired) {
        t->sleep_expired = 0;
        emit_timer(e, 0, "0");
    }
    if (t->sleep_done) {
        t->sleep_done = 0;
        sel_timer_ramp_clear(t);
        if (e->state.play == SEL_PLAYING) {
            set_play(e, SEL_PAUSED);
        }
    }
    if (t->alarm_fired) {
        t->alarm_fired = 0;
        if (e->state.play != SEL_PLAYING) {
            sel_timer_ramp_up(t, (uint32_t)((uint64_t)SEL_ALARM_RAMP_MS * e->rate / 1000u));
            cmd_PLAY(e, NULL);
        }
    }
    if (t->ramp_dir == SEL_RAMP_DOWN && e->state.play != SEL_PLAYING) {
        sel_timer_ramp_clear(t); /* paused or stopped by hand mid-ramp */
    }
}

/* REPEAT off|one|all: the repeat option. */
static void cmd_REPEAT(sel_engine* e, const sel_message* m) {
    set_option(e, SEL_OPT_REPEAT, m->args[0].v.s);
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

/* The registry descriptor of the DSP plugin `id`, or NULL. */
static const sel_tag* find_dsp(const sel_engine* e, const char* id) {
    uint32_t i, n;
    if (e->registry_count == NULL || e->registry_get == NULL) {
        return NULL;
    }
    n = e->registry_count();
    for (i = 0; i < n; i++) {
        const sel_tag* desc = e->registry_get(i);
        const char* pid = (const char*)sel_tag_find(desc, SEL_PLUGIN_ID, 0);
        if (sel_tag_find(desc, SEL_PLUGIN_KIND, 0) == SEL_PLUGIN_KIND_DSP && pid != NULL && strcmp(pid, id) == 0) {
            return desc;
        }
    }
    return NULL;
}

/* Applies the LED and width state to the stage, the snapshot and the prefs. The pref is what
 * prefs.conf keeps, so the next start comes up the same (apply_stage_prefs). */
static void stage_changed(sel_engine* e, int pref_id) {
    emit_pref(e, pref_id);
    if (e->config_dir[0] != '\0' && !sel_prefs_save(&e->prefs, e->config_dir)) {
        sel_engine_report_error(e, "prefs_save_failed", e->config_dir);
    }
}

static void cmd_FILTER_LED(sel_engine* e, const sel_message* m) {
    const char* mode = m->args[0].v.s;
    uint32_t on;
    if (strcmp(mode, "on") == 0) {
        on = 1;
    } else if (strcmp(mode, "off") == 0) {
        on = 0;
    } else if (strcmp(mode, "toggle") == 0) {
        on = !e->state.led_on;
    } else {
        sel_engine_report_error(e, "argument_type", "FILTER_LED");
        return;
    }
    e->state.led_on = on;
    sel_engine_set_stage(e, on, e->state.stereo_width);
    (void)sel_prefs_set(&e->prefs, "output.led", on ? "on" : "off");
    stage_changed(e, SEL_PREF_OUTPUT_LED);
}

static void cmd_MIX_STEREO_WIDTH(sel_engine* e, const sel_message* m) {
    const sel_arg* a = &m->args[0];
    int32_t v = (a->flags & SEL_ARG_RELATIVE) ? (int32_t)e->state.stereo_width + a->v.i : a->v.i;
    char text[16];
    if (v < 0) {
        v = 0;
    }
    if (v > 100) {
        v = 100;
    }
    e->state.stereo_width = (uint32_t)v;
    sel_engine_set_stage(e, e->state.led_on, (uint32_t)v);
    snprintf(text, sizeof text, "%d", (int)v);
    (void)sel_prefs_set(&e->prefs, "mix.stereo_width", text);
    stage_changed(e, SEL_PREF_MIX_STEREO_WIDTH);
}

/* DSP <name> on|off. The chain only exists on the mixed path: a Paula-direct
 * output plays voices without any PCM to filter. */
static void cmd_DSP(sel_engine* e, const sel_message* m) {
    const char* name = m->args[0].v.s;
    const char* mode = m->args[1].v.s;
    const sel_tag* desc = find_dsp(e, name);
    int on;
    if (desc == NULL) {
        sel_engine_report_error(e, "unknown_dsp", name);
        return;
    }
    if (strcmp(mode, "on") == 0) {
        on = 1;
    } else if (strcmp(mode, "off") == 0) {
        on = 0;
    } else {
        sel_engine_report_error(e, "argument_type", "DSP");
        return;
    }
    if (!on) {
        sel_dsp_chain_disable(&e->dsp, (const char*)sel_tag_find(desc, SEL_PLUGIN_ID, 0));
        return;
    }
    if (!e->mixed_path) {
        sel_engine_report_error(e, "unsupported_path", "DSP");
        return;
    }
    if (!sel_dsp_chain_enable(&e->dsp, e->host, (const char*)sel_tag_find(desc, SEL_PLUGIN_ID, 0),
                              (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0), e->rate)) {
        sel_engine_report_error(e, "dsp_failed", name);
    }
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

/* Scrobble rule (I177, last.fm): a song longer than 30 s is due once it has played half its length or 4 minutes,
 * whichever comes first. A song of unknown length (duration 0) is due at the 4 minute mark. */
#define SEL_SCROBBLE_MIN_MS 30000u
#define SEL_SCROBBLE_CAP_MS 240000u

static void scrobble_check(sel_engine* e, sel_slot* sl) {
    uint32_t dur = (uint32_t)sel_tag_find(sl->rp_metadata != NULL ? sl->rp_metadata(sl->rp_state) : NULL,
                                          SEL_META_DURATION_MS, 0);
    uint32_t due = SEL_SCROBBLE_CAP_MS;
    uint32_t played = (uint32_t)((uint64_t)sl->song_frames * 1000u / e->rate);
    sel_message ev;

    if (sl->scrobbled) return;
    if (dur != 0) {
        if (dur <= SEL_SCROBBLE_MIN_MS) return;
        if (dur / 2u < due) due = dur / 2u;
    }
    if (played < due) return;
    sl->scrobbled = 1;
    e->scrobble_due++;
    evt_init(&ev, SEL_EVT_SCROBBLE_DUE);
    emit(e, &ev);
}

/* Pump thread only: RP_CLOSE and free; the audio thread no longer mixes the slot. */
static void leveler_writeback(sel_engine* e, sel_slot* sl);

static void close_slot(sel_engine* e, sel_slot* sl) {
    leveler_writeback(e, sl);
    sl->lev_on = 0;
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
    sl->scrobbled = 0;
    sl->last_tick = 0;
    memset(&sl->pol, 0, sizeof sl->pol);
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
        sl->scrobbled = 0;
        sl->last_tick = 0;
        memset(&sl->pol, 0, sizeof sl->pol);
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
    sl->scrobbled = 0;
    sl->last_tick = 0;
    memset(&sl->pol, 0, sizeof sl->pol);
    sl->subsong = subsong;
    e->route_direct = sel_engine_voice_route(sl->rp_info.voice_count, e->voice_sink != NULL, e->voice_max) == SEL_ROUTE_DIRECT;
    return sl->rp_start(sl->rp_state, subsong, e->rate, e->route_direct ? e->voice_sink : sel_mixer_voices(&sl->mixer));
}

/* Reads the whole file into host memory; NULL when it cannot be read. */
static uint8_t* read_song(sel_engine* e, const char* path, size_t* size) {
    FILE* f;
    long len;
    uint8_t* buf;
    char cached[SEL_BROWSE_PATH_MAX];
    char best[SEL_BROWSE_PATH_MAX];
    /* D-M3-14 (I184): a library song with twins of the same md5 plays its best source. */
    if (e->lib != NULL && sel_library_best_source(e->lib, path, best, sizeof best) && strcmp(best, path) != 0) {
        snprintf(e->dup_played, sizeof e->dup_played, "%s", best);
        e->dup_redirects++;
        path = best;
    }
    if (e->lib != NULL && sel_cache_is_remote(path)) {
        /* Remote key: the file cache serves it (a hit when the prefetch got there first), I168. */
        char rules[512];
        uint32_t files = 0;
        sel_companion_rules(e->registry_count, e->registry_get, rules, sizeof rules);
        if (sel_library_fetch(e->lib, path, rules, cached, sizeof cached, &files) <= 0) {
            return NULL;
        }
        path = cached;
    }
    f = fopen(path, "rb");
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

/* Value of `key` in a "k=v,k=v" list into e->rp_value; 0 when absent. */
static int rp_list_find(sel_engine* e, const char* list, const char* key) {
    size_t kl = strlen(key);
    while (*list != '\0') {
        const char* end = strchr(list, ',');
        size_t n = end != NULL ? (size_t)(end - list) : strlen(list);
        if (n > kl && list[kl] == '=' && strncmp(list, key, kl) == 0) {
            size_t vl = n - kl - 1u;
            if (vl >= sizeof e->rp_value) vl = sizeof e->rp_value - 1u;
            memcpy(e->rp_value, list + kl + 1u, vl);
            e->rp_value[vl] = '\0';
            return 1;
        }
        if (end == NULL) break;
        list = end + 1;
    }
    return 0;
}

/* host->config_get handed to replayers (I134): the per-song override beats the pref "rp.config",
 * which beats whatever the real host answers. Each layer that answered counts in rp_config_applied. */
static const char* rp_config_get(const sel_host* h, const char* key, const char* def) {
    sel_engine* e = (sel_engine*)h->ctx;
    int have = 0;
    if (rp_list_find(e, e->prefs.value[SEL_PREF_RP_CONFIG], key)) {
        e->rp_config_applied++;
        have = 1;
    }
    if (rp_list_find(e, e->rp_song_config, key)) {
        e->rp_config_applied++;
        have = 1;
    }
    if (have) return e->rp_value;
    return e->host->config_get != NULL ? e->host->config_get(e->host, key, def) : def;
}

/* The per-song override of `path` comes from the library DB (sticker rp.config, I170); no library: none. */
static void song_config_load(sel_engine* e, const char* path) {
    e->rp_song_config[0] = '\0';
    if (e->lib != NULL) {
        sel_library_song_config_get(e->lib, path, e->rp_song_config, sizeof e->rp_song_config);
    }
}

/* History (I170): the song that was playing ended. skipped = 0: it reached its natural end (a play);
 * skipped = 1: the user replaced it first. One row per song, written through the library worker. */
static void hist_end(sel_engine* e, int skipped) {
    if (e->hist_key[0] != '\0' && e->lib != NULL) {
        if (sel_library_history_add(e->lib, e->hist_key, (int64_t)time(NULL), skipped)) {
            e->history_writes++;
        }
    }
    e->hist_key[0] = '\0';
}

/* The library key of a song: a "local:" key stays, a plain path gets the prefix. Used by the loudness lookup and
 * by ANALYSE. */
static void song_key(const char* path, char* key, size_t cap) {
    snprintf(key, cap, strncmp(path, "local:", 6) == 0 ? "%s" : "local:%s", path);
}

/* Loudness (I189, I189a, D-M3-9): the gain is a property of the song, so it is fixed per slot here on the command
 * thread, before the slot is published through the swap ring. mix.loudness off leaves the samples alone; track and
 * auto apply the library's stored gain of the song. A song without a stored gain stays untouched (auto's adaptive
 * leveler is I190). */
static void loudness_slot(sel_engine* e, sel_slot* sl, const char* path) {
    const char* mode = e->prefs.value[SEL_PREF_MIX_LOUDNESS];
    char key[SEL_DEFERRED_PATH_MAX + 8];
    int32_t cdb = 0;
    int provisional = 0;
    sl->loud_active = 0;
    sl->loud_gain_q16 = SEL_GAIN_UNITY;
    sl->lev_on = 0;
    sl->lev_done = 0;
    sel_leveler_init(&sl->lev);
    if (strcmp(mode, "off") == 0) {
        sl->loud_source = "off";
        return;
    }
    song_key(path, key, sizeof key);
    if (e->lib != NULL && sel_library_loudness_get_ex(e->lib, key, &cdb, &provisional)) {
        sl->loud_source = provisional ? "provisional" : "stored";
        sl->loud_active = 1;
        sl->loud_gain_q16 = sel_gain_q16_from_cdb(cdb);
        return;
    }
    if (strcmp(mode, "auto") == 0) { /* no gain known: the adaptive leveler (I190) */
        sl->loud_source = "leveler";
        sl->loud_active = 1;
        sl->lev_on = 1;
        return;
    }
    sl->loud_source = "none";
}

/* Command thread (I190): a song the leveler measured long enough leaves its estimate in the library as a
 * provisional gain; a real analysis or DEViLBOX value overrides it (the library never lets a provisional write
 * replace a real gain). The slot is no longer rendered when this runs. The audio thread only wrote the estimate. */
static void leveler_writeback(sel_engine* e, sel_slot* sl) {
    int32_t cdb = 0;
    char key[SEL_DEFERRED_PATH_MAX + 8];
    if (!sl->lev_on || sl->lev_done || sl->path == NULL || e->lib == NULL ||
        !sel_leveler_estimate_cdb(&sl->lev, e->rate, &cdb)) {
        return;
    }
    sl->lev_done = 1;
    song_key(sl->path, key, sizeof key);
    if (sel_library_loudness_set_ex(e->lib, key, key + 6, cdb, 1)) {
        e->lev_writes++;
    }
}

/* Audio thread: the loudness reference gain of this render call, 16.16 (I189b). The DSP chain and the mix stage
 * between the mix and the limiter work in place on 16-bit samples, so a boost cannot travel through them. Instead the
 * slots are mixed at their gain relative to the largest gain of the songs that can sound in this call (at most
 * unity, so no slot or crossfade sum can pass 16 bits) and the limiter stage multiplies that reference back in
 * wide arithmetic, saturating only after its ceiling. A call without a boost has reference unity: the slot gains
 * are applied as before, bit for bit. Every song that can sound must carry a gain, else the limiter would raise
 * an unscaled song. */
static uint32_t loud_reference(const sel_engine* e, uint32_t frames) {
    uint32_t ref = SEL_GAIN_UNITY;
    uint32_t idx[2];
    uint32_t i;
    idx[0] = e->act;
    idx[1] = e->nx;
    for (i = 0; i < 2u; i++) {
        const sel_slot* sl;
        if (idx[i] >= SEL_SLOT_COUNT) {
            continue;
        }
        sl = &e->slot[idx[i]];
        if (!sl->loud_active) {
            return SEL_GAIN_UNITY;
        }
        {
            uint32_t g = sl->lev_on ? sel_leveler_ceiling_q16(&sl->lev, frames, e->rate) : sl->loud_gain_q16;
            if (g > ref) {
                ref = g;
            }
        }
    }
    return ref;
}

/* Audio thread: this slot's gain on its own samples, before the crossfade mix, relative to the call's reference
 * gain (at most unity, so it never saturates). The limiter runs once on the mixed output. */
static void slot_gain(sel_engine* e, sel_slot* sl, int16_t* stereo, uint32_t frames) {
    uint32_t i;
    uint32_t gain;
    if (!sl->loud_active || frames == 0u) {
        return;
    }
    e->loud_mixed = 1;
    if (sl->lev_on) { /* the leveler: gain per short chunk, measured on the unscaled samples (I190) */
        uint32_t at = 0;
        while (at < frames) {
            uint32_t n = frames - at < SEL_LEVELER_CHUNK ? frames - at : SEL_LEVELER_CHUNK;
            int16_t* p = stereo + (size_t)at * 2u;
            uint32_t g = sel_leveler_chunk(&sl->lev, p, n, e->rate);
            if (e->loud_ref > SEL_GAIN_UNITY) {
                g = (uint32_t)((((uint64_t)g << 16) + (e->loud_ref >> 1)) / e->loud_ref);
            }
            if (g != SEL_GAIN_UNITY) {
                for (i = 0; i < n * 2u; i++) {
                    int64_t v = ((int64_t)p[i] * g + 32768) >> 16;
                    p[i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
                }
            }
            at += n;
        }
        e->lev_frames += frames;
        sl->loud_gain_q16 = sel_gain_q16_from_cdb((int32_t)((sl->lev.cur_q16 + 32768) >> 16));
        return;
    }
    gain = sl->loud_gain_q16;
    if (e->loud_ref > SEL_GAIN_UNITY) {
        gain = (uint32_t)((((uint64_t)gain << 16) + (e->loud_ref >> 1)) / e->loud_ref);
    }
    if (gain == SEL_GAIN_UNITY) {
        return;
    }
    for (i = 0; i < frames * 2u; i++) {
        int64_t v = ((int64_t)stereo[i] * gain + 32768) >> 16; /* saturates only when a song without a gain shares the call */
        stereo[i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
    }
}

static void hist_begin(sel_engine* e, const char* path) {
    size_t n = strlen(path);
    if (n >= sizeof e->hist_key) n = sizeof e->hist_key - 1u;
    memcpy(e->hist_key, path, n);
    e->hist_key[n] = '\0';
}

/* Prepares the idle slot `idx` with the song at `path` (all allocation, file
 * read, RP_OPEN and RP_START happen here) and publishes it through the swap
 * ring. On failure the slot stays free and the error is reported. */
static int prepare_slot(sel_engine* e, uint32_t idx, const char* path, uint32_t subsong,
                        uint32_t skip_ms, const char* err_detail, uint32_t queued) {
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
    sl->rp_tracker_info = (sel_rp_tracker_info_fn)sel_tag_find(hooks, SEL_RP_TRACKER_INFO, 0);
    sl->rp_tracker_cells = (sel_rp_tracker_cells_fn)sel_tag_find(hooks, SEL_RP_TRACKER_CELLS, 0);
    sl->rp_sample_get = (sel_rp_sample_get_fn)sel_tag_find(hooks, SEL_RP_SAMPLE_GET, 0);
    sl->rp_mute = kind == SEL_RP_KIND_RENDER ? (sel_rp_mute_fn)sel_tag_find(hooks, SEL_RP_MUTE, 0) : NULL;
    sl->applied_mute = 0;
    sl->rp_set_speed =
        kind == SEL_RP_KIND_RENDER ? (sel_rp_set_speed_fn)sel_tag_find(hooks, SEL_RP_SET_SPEED, 0) : NULL;
    sl->applied_pitch = 0;
    sl->applied_tempo = 0;
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
    sl->rp_state = open(&src, &e->rp_host, &sl->rp_info);
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
    loudness_slot(e, sl, path);
    sl->busy = 1;
    memset(&sw, 0, sizeof sw);
    sw.kind = SEL_MSG_CMD;
    sw.id = SEL_RING_SWAP;
    sw.argc = 2;
    sw.args[0].type = SEL_ARG_UINT;
    sw.args[0].v.u = idx;
    sw.args[1].type = SEL_ARG_UINT;
    sw.args[1].v.u = queued; /* 0: switch now, 1: keep as the next song, 2: drop the next song */
    if (!sel_spsc_push(&e->swap_q, &sw)) {
        close_slot(e, sl);
        sel_engine_report_error(e, "start_failed", err_detail);
        return 0;
    }
    return 1;
}

static void cancel_next(sel_engine* e);

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
    cancel_next(e);
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
    if (kind == SEL_BEGIN_OPEN) {
        song_config_load(e, path);
    }
    if (!prepare_slot(e, idx, path, subsong, kind == SEL_BEGIN_SEEK || kind == SEL_BEGIN_OPEN ? seek_to : 0, detail, 0)) {
        return 0;
    }
    e->cur = idx;
    e->song_over = 0;
    if (kind == SEL_BEGIN_OPEN) {
        hist_end(e, 1); /* a song still open here was replaced by the user */
        hist_begin(e, path);
        e->songs_opened++;
        evt_init(&ev, SEL_EVT_SONG_OPENED);
        evt_tags(&ev, e->slot[idx].rp_metadata ? e->slot[idx].rp_metadata(e->slot[idx].rp_state) : NULL);
        emit(e, &ev);
        if (seek_to > 0) { /* a share link with t=<ms> opens at that position (I180) */
            evt_init(&ev, SEL_EVT_SEEKED);
            evt_uint(&ev, e->slot[idx].song_frames);
            emit(e, &ev);
        }
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

static int open_share_link(sel_engine* e, const char* text);

static void cmd_OPEN(sel_engine* e, const sel_message* m) {
    if (sel_share_is_link(m->args[0].v.s)) {
        open_share_link(e, m->args[0].v.s);
        return;
    }
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
            /* The one place the random candidate set is built: a skip-flagged song is never a candidate (I171). */
            if (e->lib != NULL && sel_library_flag_skipped(e->lib, sel_queue_path(q, i))) {
                e->skip_excluded++;
                continue;
            }
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
static void radio_fill(sel_engine* e);

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
    radio_fill(e);
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
        if (e->repeat != 2u || strcmp(e->state.options[SEL_OPT_SCOPE], "album") == 0) {
            return SEL_QUEUE_NONE;
        }
        next = skip_dividers(&e->queue, 0);
        if (next >= e->queue.count || strcmp(e->state.options[SEL_OPT_STOP_AFTER_QUEUE], "on") == 0) {
            return SEL_QUEUE_NONE;
        }
        return next;
    }
    if (strcmp(e->state.options[SEL_OPT_SCOPE], "album") == 0 && !same_album(&e->queue, cur, next)) {
        return SEL_QUEUE_NONE;
    }
    return next;
}

/* Early prepare (plan M1.3 I95): drops the prepared next song. The audio thread returns
 * its slot; when it already switched to it the song simply plays on. */
static void cancel_next(sel_engine* e) {
    if (e->nx_slot == SEL_SLOT_NONE) {
        e->nx_id = 0;
        return;
    }
    if (e->audio_slot != e->nx_slot && e->slot[e->nx_slot].busy) {
        sel_message sw;
        memset(&sw, 0, sizeof sw);
        sw.kind = SEL_MSG_CMD;
        sw.id = SEL_RING_SWAP;
        sw.argc = 2;
        sw.args[0].type = SEL_ARG_UINT;
        sw.args[0].v.u = e->nx_slot;
        sw.args[1].type = SEL_ARG_UINT;
        sw.args[1].v.u = 2u;
        sel_spsc_push(&e->swap_q, &sw);
    }
    e->nx_slot = SEL_SLOT_NONE;
    e->nx_id = 0;
}

/* The audio thread switched to the prepared song: the old one ended, the new one opens. */
static void radio_fill(sel_engine* e);

static void commit_next(sel_engine* e) {
    sel_message ev;
    uint32_t idx = sel_queue_index_of(&e->queue, e->nx_id);
    sel_slot* sl = &e->slot[e->nx_slot];
    evt_init(&ev, SEL_EVT_SONG_ENDED);
    evt_str(&ev, k_end_reason_names[e->xf_reason < 5u ? e->xf_reason : (uint32_t)SEL_END_END]);
    emit(e, &ev);
    hist_end(e, 0);
    if (idx < e->queue.count) {
        hist_begin(e, sel_queue_path(&e->queue, idx));
    }
    if (random_on(e) && idx < e->queue.count) {
        order_mark(e, idx);
    }
    e->q_playing = e->nx_id;
    if (idx < e->queue.count) {
        e->q_hint = idx;
    }
    radio_fill(e);
    e->cur = e->nx_slot;
    e->nx_slot = SEL_SLOT_NONE;
    e->nx_id = 0;
    e->songs_opened++;
    e->song_over = 0;
    evt_init(&ev, SEL_EVT_SONG_OPENED);
    evt_tags(&ev, sl->rp_metadata ? sl->rp_metadata(sl->rp_state) : NULL);
    emit(e, &ev);
    emit_position(e, sl);
}

/* Prefetch (I168): while a song plays, the next queue entry, when remote, is fetched into the cache on the
 * library's prefetch thread (sel_library_prefetch, which reuses sel_library_fetch's cache code). One request
 * at a time; a change of the queue or of the next entry replaces or cancels the pending one. */
static int pref_is_on(const sel_engine* e, int id) {
    return strcmp(e->prefs.value[id], "on") == 0;
}

/* Smart downloads (I169, F34): once per (queue version, playing entry, prefs generation) the library plans the songs of
 * the smart playlist and the history that are not cached; its thread fetches them behind the prefetch. */
static void smart_sync(sel_engine* e, int allowed) {
    uint32_t key;

    if (!allowed || !pref_is_on(e, SEL_PREF_LIBRARY_SMART) || e->state.play != SEL_PLAYING) {
        if (e->sm_key != 0) {
            e->sm_key = 0;
            sel_library_prefetch_cancel(e->lib);
        }
        return;
    }
    key = (e->queue.version * 31u + e->q_playing) * 31u + e->prefs.generation + 1u;
    if (key != e->sm_key) {
        char rules[512];
        e->sm_key = key;
        sel_companion_rules(e->registry_count, e->registry_get, rules, sizeof rules);
        (void)sel_library_smart(e->lib, e->prefs.value[SEL_PREF_LIBRARY_SMART_VIEW],
                                (unsigned)strtoul(e->prefs.value[SEL_PREF_LIBRARY_SMART_TOP], NULL, 10), rules);
    }
}

static void prefetch_sync(sel_engine* e) {
    uint32_t next = SEL_QUEUE_NONE;
    int allowed;
    if (e->lib == NULL) {
        return;
    }
    /* library.prefetch=off or library.metered=on: no background transfer at all (I169) */
    allowed = pref_is_on(e, SEL_PREF_LIBRARY_PREFETCH) && !pref_is_on(e, SEL_PREF_LIBRARY_METERED);
    smart_sync(e, allowed);
    if (!allowed) {
        if (e->pf_id != 0) {
            e->pf_id = 0;
            sel_library_prefetch_cancel(e->lib);
        }
        return;
    }
    if (e->state.play == SEL_PLAYING && e->cur != SEL_SLOT_NONE && e->q_playing != 0 && e->repeat != 1u) {
        next = queue_next_index(e);
    }
    if (next < e->queue.count && sel_cache_is_remote(sel_queue_path(&e->queue, next))) {
        if (e->pf_id != e->queue.entries[next].id || e->pf_qver != e->queue.version) {
            char rules[512];
            sel_companion_rules(e->registry_count, e->registry_get, rules, sizeof rules);
            e->pf_id = e->queue.entries[next].id;
            e->pf_qver = e->queue.version;
            sel_library_prefetch(e->lib, sel_queue_path(&e->queue, next), rules);
        }
    } else if (e->pf_id != 0) {
        e->pf_id = 0;
        sel_library_prefetch_cancel(e->lib);
    }
}

/* Prepares the next queue entry into the idle slot shortly before the current song ends
 * (end of the fade, or of max_song_ms), so the switch is gapless or crossfaded. A song
 * with no known end is prepared at once: no duration is used (plan Q20). */
static void prepare_next(sel_engine* e) {
    const sel_slot* sl;
    uint32_t next, idx;
    if (e->nx_slot != SEL_SLOT_NONE) {
        if (e->queue.version != e->nx_qver || e->state.play == SEL_STOPPED || e->repeat == 1u || e->q_playing == 0 ||
            strcmp(e->state.options[SEL_OPT_STOP_AFTER_CURRENT], "on") == 0) {
            cancel_next(e);
        }
        return;
    }
    if (e->state.play != SEL_PLAYING || e->cur == SEL_SLOT_NONE || e->q_playing == 0 || e->repeat == 1u ||
        e->audio_slot != e->cur || strcmp(e->state.options[SEL_OPT_STOP_AFTER_CURRENT], "on") == 0) {
        return;
    }
    sl = &e->slot[e->cur];
    if (sl->ended || e->deferred) {
        return;
    }
    {
        uint64_t lead = (uint64_t)(e->crossfade_ms + 2000u) * e->rate / 1000u;
        uint64_t remain = ~(uint64_t)0;
        if (sl->pol.fade_on) {
            remain = sl->pol.fade_total - sl->pol.fade_pos;
        } else if (e->max_song_ms != 0u) {
            uint64_t end = (uint64_t)e->max_song_ms * e->rate / 1000u + (uint64_t)e->fade_ms * e->rate / 1000u;
            remain = end > sl->song_frames ? end - sl->song_frames : 0u;
        }
        if (remain == ~(uint64_t)0 || remain > lead) { /* end unknown (no duration is used, Q20): no early prepare */
            return;
        }
    }
    next = queue_next_index(e);
    if (next >= e->queue.count || !find_free_slot(e, &idx)) {
        return;
    }
    if (e->nx_id == e->queue.entries[next].id && e->nx_qver == e->queue.version) {
        return; /* this attempt failed already */
    }
    e->nx_id = e->queue.entries[next].id;
    e->nx_qver = e->queue.version;
    if (prepare_slot(e, idx, sel_queue_path(&e->queue, next),
                     e->queue.entries[next].subsong_plus1 ? e->queue.entries[next].subsong_plus1 - 1u : SEL_SUBSONG_DEFAULT, 0,
                     sel_queue_path(&e->queue, next), 1u)) {
        e->nx_slot = idx;
    }
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

/* PLAYLIST_PUBLISH <title> <author> <genre> <filename> (I164): exports the queue as a Hippo `.prg` into the
 * playlist scratch buffer, refuses more than SEL_HIPPO_PUBLISH_MAX bytes (ERROR too_large, no request), POSTs it
 * and reports the parsed reply as LOG lines `publish posts=N id=..` and `publish_url <filename> <url>`. */
typedef struct pl_mem {
    char* buf;
    size_t cap, len;
    int over;
} pl_mem;

static int playlist_mem_sink(void* ctx, const char* bytes, uint32_t len) {
    pl_mem* m = (pl_mem*)ctx;
    if (len > m->cap - m->len) {
        m->over = 1;
        return 0;
    }
    memcpy(m->buf + m->len, bytes, len);
    m->len += len;
    return 1;
}

static void cmd_PLAYLIST_PUBLISH(sel_engine* e, const sel_message* m) {
    sel_playlist pl;
    pl_mem out;
    sel_hippo_publish_reply rep;
    char detail[SEL_LOG_LINE_MAX];
    uint32_t i;
    int rc;

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
            sel_engine_report_error(e, "queue_full", "PLAYLIST_PUBLISH");
            return;
        }
    }
    out.buf = e->pl_file;
    out.cap = (size_t)e->queue.text_bytes + 16u;
    out.len = 0;
    out.over = 0;
    rc = sel_playlist_write(SEL_PL_HIPPO, &pl, playlist_mem_sink, &out, playlist_warn, e);
    if (out.over) {
        sel_engine_report_error(e, "too_large", "PLAYLIST_PUBLISH"); /* does not fit the export buffer either */
        return;
    }
    if (rc != SEL_PL_OK) {
        sel_engine_report_error(e, "bad_format", "PLAYLIST_PUBLISH");
        return;
    }
    rc = sel_hippo_publish(out.buf, out.len, m->args[0].v.s, m->args[1].v.s, m->args[2].v.s, m->args[3].v.s, &rep);
    switch (rc) {
    case SEL_HIPPO_PUB_OK:
        snprintf(detail, sizeof detail, "posts=%u id=%s", sel_hippo_publish_posts(), rep.id);
        emit_log(e, "publish", detail);
        snprintf(detail, sizeof detail, "%s %s", rep.filename, rep.url);
        emit_log(e, "publish_url", detail);
        break;
    case SEL_HIPPO_PUB_TOO_LARGE:
        sel_engine_report_error(e, "too_large", "PLAYLIST_PUBLISH");
        break;
    case SEL_HIPPO_PUB_ARGS:
        sel_engine_report_error(e, "range", "PLAYLIST_PUBLISH");
        break;
    case SEL_HIPPO_PUB_NET:
        sel_engine_report_error(e, "net_failed", "PLAYLIST_PUBLISH");
        break;
    case SEL_HIPPO_PUB_STATUS:
        sel_engine_report_error(e, "publish_rejected", "PLAYLIST_PUBLISH");
        break;
    default:
        sel_engine_report_error(e, "bad_reply", "PLAYLIST_PUBLISH");
        break;
    }
}

/* SAMPLE_LIST / SAMPLE_EXPORT (I98): the replayer's optional RP_SAMPLE_GET hook
 * supplies the sample table; the writers live in src/library/export. The hook
 * runs on the command thread. */
static const sel_slot* sample_slot(sel_engine* e, const char* verb) {
    const sel_slot* sl = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    if (sl == NULL || sl->rp_state == NULL || sl->rp_sample_get == NULL) {
        sel_engine_report_error(e, "unsupported", verb);
        return NULL;
    }
    return sl;
}

static void cmd_SAMPLE_LIST(sel_engine* e, const sel_message* m) {
    const sel_slot* sl = sample_slot(e, "SAMPLE_LIST");
    sel_rp_sample smp;
    uint32_t n = 1, count = 1;
    (void)m;
    if (sl == NULL) {
        return;
    }
    while (n <= count) {
        memset(&smp, 0, sizeof smp);
        if (sl->rp_sample_get(sl->rp_state, n, &smp) && smp.name != NULL) {
            sel_message ev;
            evt_init(&ev, SEL_EVT_SAMPLE);
            evt_uint(&ev, n);
            evt_uint(&ev, smp.length);
            evt_uint(&ev, smp.loop_start);
            evt_uint(&ev, smp.loop_length);
            evt_uint(&ev, smp.rate);
            evt_str(&ev, smp.name);
            emit(e, &ev);
        }
        count = smp.count;
        n++;
    }
}

static int sample_sink(void* ctx, const void* bytes, uint32_t len) {
    return fwrite(bytes, 1, len, (FILE*)ctx) == len;
}

static int path_has_ext(const char* path, const char* ext) {
    size_t a = strlen(path), b = strlen(ext);
    size_t i;
    if (a < b) {
        return 0;
    }
    for (i = 0; i < b; i++) {
        char c = path[a - b + i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + 32);
        }
        if (c != ext[i]) {
            return 0;
        }
    }
    return 1;
}

static void cmd_SAMPLE_EXPORT(sel_engine* e, const sel_message* m) {
    const sel_slot* sl = sample_slot(e, "SAMPLE_EXPORT");
    const char* path = m->args[1].v.s;
    sel_rp_sample smp;
    int svx = path_has_ext(path, ".8svx");
    FILE* f;
    int ok;
    if (sl == NULL) {
        return;
    }
    memset(&smp, 0, sizeof smp);
    if ((!svx && !path_has_ext(path, ".wav")) || !sl->rp_sample_get(sl->rp_state, m->args[0].v.u, &smp)) {
        sel_engine_report_error(e, "range", "SAMPLE_EXPORT");
        return;
    }
    f = fopen(path, "wb");
    if (f == NULL) {
        sel_engine_report_error(e, "open_failed", "SAMPLE_EXPORT");
        return;
    }
    ok = svx ? sel_export_svx(&smp, sample_sink, f) : sel_export_wav(&smp, sample_sink, f);
    if (fclose(f) != 0) {
        ok = 0;
    }
    if (!ok) {
        sel_engine_report_error(e, "open_failed", "SAMPLE_EXPORT");
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
    t[3].tag = SEL_TAG_END;
    t[3].value = 0;
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

/* ---- library queries (plan M3 Phase 2, I149 and I150) ------------------ */

/* The event keeps the code pointer, so a code that is not a literal is copied into a ring first. */
static void report_library_error(sel_engine* e, const char* code, const char* who) {
    char* slot = e->lib_err[e->lib_err_slot++ & 3u];
    copy_str(slot, sizeof e->lib_err[0], code);
    sel_engine_report_error(e, slot, who);
}

/* Runs `text` on the library worker into res_text. 1 on success; on failure the ERROR event is raised. */
static int run_search(sel_engine* e, const char* text, const char* who, uint32_t* fts) {
    sel_lib_job job;

    e->res_count = 0;
    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", who);
        return 0;
    }
    memset(&job, 0, sizeof job);
    job.text = text;
    job.now = (long long)time(NULL);
    job.out = e->res_text;
    job.out_cap = e->queue.text_bytes;
    job.meta = e->res_meta;
    job.meta_cap = e->queue.text_bytes;
    if (!sel_library_run(e->lib, SEL_LIB_REQ_SEARCH, &job)) {
        sel_engine_report_error(e, "library_failed", who);
        return 0;
    }
    e->searches++;
    e->fts_queries += job.used_fts;
    if (job.error[0] != '\0') {
        report_library_error(e, job.error, who);
        return 0;
    }
    if (job.truncated) {
        char n[16];
        snprintf(n, sizeof n, "%u", (unsigned)job.truncated);
        emit_log(e, "result_truncated", n);
    }
    e->res_count = job.count;
    e->res_meta_used = job.meta_used;
    *fts = job.used_fts;
    return 1;
}

const char* sel_engine_result_key(const sel_engine* e, uint32_t i) {
    const char* p = e->res_text;
    if (p == NULL || i >= e->res_count) {
        return NULL;
    }
    while (i-- > 0) {
        p += strlen(p) + 1u;
    }
    return p;
}

static void cmd_LIBRARY_SEARCH(sel_engine* e, const sel_message* m) {
    uint32_t fts = 0;
    sel_message ev;
    uint32_t slot;
    sel_tag* t;

    if (!run_search(e, m->args[0].v.s, "LIBRARY_SEARCH", &fts)) {
        return;
    }
    slot = e->lib_slot++ & 3u;
    t = e->lib_tags[slot];
    snprintf(e->lib_path[slot], sizeof e->lib_path[slot], "%s", m->args[0].v.s);
    t[0].tag = SEL_SRC_PATH;
    t[0].value = (uintptr_t)e->lib_path[slot];
    t[1].tag = SEL_SRC_COUNT;
    t[1].value = e->res_count;
    t[2].tag = SEL_SRC_FTS;
    t[2].value = fts;
    t[3].tag = SEL_TAG_END;
    t[3].value = 0;
    evt_init(&ev, SEL_EVT_LIBRARY_RESULT);
    evt_tags(&ev, t);
    emit(e, &ev);
}

static void cmd_QUEUE_ADD_QUERY(sel_engine* e, const sel_message* m) {
    uint32_t fts = 0, i;

    if (!run_search(e, m->args[0].v.s, "QUEUE_ADD_QUERY", &fts)) {
        return;
    }
    for (i = 0; i < e->res_count; i++) {
        queue_insert(e, e->queue.count, sel_engine_result_key(e, i), "QUEUE_ADD_QUERY");
    }
}

static void cmd_PLAY_QUERY(sel_engine* e, const sel_message* m) {
    uint32_t fts = 0, i;

    if (!run_search(e, m->args[0].v.s, "PLAY_QUERY", &fts)) {
        return;
    }
    sel_history_record(&e->history, &e->queue);
    sel_history_bury(&e->history, &e->queue, SEL_QUEUE_NONE);
    sel_queue_clear(&e->queue);
    e->q_playing = 0;
    e->q_hint = 0;
    for (i = 0; i < e->res_count; i++) {
        queue_insert(e, e->queue.count, sel_engine_result_key(e, i), "PLAY_QUERY");
    }
    emit_queue_changed(e);
    if (e->queue.count > 0) {
        queue_start(e, 0);
    }
}

static void cmd_VIEW_SAVE(sel_engine* e, const sel_message* m) {
    sel_lib_job job;

    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "VIEW_SAVE");
        return;
    }
    memset(&job, 0, sizeof job);
    job.text = m->args[0].v.s;
    job.text2 = m->args[1].v.s;
    job.now = (long long)time(NULL);
    if (!sel_library_run(e->lib, SEL_LIB_REQ_SAVE_VIEW, &job)) {
        sel_engine_report_error(e, "library_failed", "VIEW_SAVE");
    } else if (job.error[0] != '\0') {
        report_library_error(e, job.error, "VIEW_SAVE");
    }
}

/* Emit a LIBRARY_RESULT with a path and a count from the library hooks (scan and fetch). */
static void emit_library_path(sel_engine* e, const char* path, uint32_t count) {
    uint32_t slot = e->lib_slot++ & 3u;
    sel_tag* t = e->lib_tags[slot];
    sel_message r;
    size_t n = strlen(path);

    if (n >= sizeof e->lib_path[slot]) n = sizeof e->lib_path[slot] - 1u;
    memcpy(e->lib_path[slot], path, n);
    e->lib_path[slot][n] = '\0';
    t[0].tag = SEL_SRC_PATH;
    t[0].value = (uintptr_t)e->lib_path[slot];
    t[1].tag = SEL_SRC_COUNT;
    t[1].value = count;
    t[2].tag = SEL_TAG_END;
    t[2].value = 0;
    evt_init(&r, SEL_EVT_LIBRARY_RESULT);
    evt_tags(&r, t);
    emit(e, &r);
}

/* LIBRARY_SCAN <dir>: scans the directory tree (and the archives in it) into the catalogue through
 * cfg.library (I158, I158a, I159); the answer is a LIBRARY_RESULT carrying the scanned path and the file count. */
static void cmd_LIBRARY_SCAN(sel_engine* e, const sel_message* m) {
    uint32_t files = 0;
    int ok;
    sel_local_scan_config scfg;
    sel_local_scan_stats stats;

    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "LIBRARY_SCAN");
        return;
    }
    memset(&scfg, 0, sizeof scfg);
    scfg.host = e->host;
    scfg.lib = e->lib;
    scfg.registry_count = e->registry_count;
    scfg.registry_get = e->registry_get;
    scfg.now = (int64_t)time(NULL);
    ok = sel_local_scan(&scfg, m->args[0].v.s, &stats);
    files = stats.scanned_files;
    e->scan_info.scanned_files = stats.scanned_files;
    e->scan_info.skipped_files = stats.skipped_files;
    e->scan_info.scan_batches = stats.scan_batches;
    e->scan_info.probed = stats.probed;
    e->scan_info.dirs = stats.dirs;
    e->scan_info.archives = stats.archives;
    e->scan_info.archive_members = stats.archive_members;
    e->scan_info.archive_lzx = stats.archive_lzx;
    e->scan_info.depack_pp = stats.depack_pp;
    if (!ok) {
        sel_engine_report_error(e, "scan_failed", m->args[0].v.s);
        return;
    }
    emit_library_path(e, m->args[0].v.s, files);
}

/* Stores the "new this week" count of a modland sync in the DB meta table, so it survives a restart (I161a). */
static void persist_new_this_week(sel_engine* e, uint32_t count) {
    char text[16];
    sel_meta_op op;
    sel_message reply;

    snprintf(text, sizeof text, "%u", (unsigned)count);
    op.key = SEL_MODLAND_META_NEW;
    op.value = text;
    op.cap = sizeof text;
    op.set = 1;
    memset(&reply, 0, sizeof reply);
    sel_library_request(e->lib, SEL_LIB_REQ_META, &op, &reply);
}

/* LIBRARY_SYNC <source> <dir> [now]: syncs a network source's catalogue from the listing and changelogs in
 * <dir> (I161, I162; `modland` and `hvsc`). `now` (epoch seconds) fixes the clock of "new this week". The answer
 * is a LIBRARY_RESULT with the directory and the listing row count; the counters are in sync_info. */
/* LIKE and KEEP mark the song being played in the library (I170). KEEP exempts it from the cache trim. */
static void user_mark(sel_engine* e, const char* who, int (*mark)(sel_library*, const char*, int64_t)) {
    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", who);
    } else if (e->hist_key[0] == '\0') {
        sel_engine_report_error(e, "no_song", who);
    } else if (!mark(e->lib, e->hist_key, (int64_t)time(NULL))) {
        sel_engine_report_error(e, "library_failed", who);
    }
}

static void cmd_LIKE(sel_engine* e, const sel_message* m) {
    (void)m;
    user_mark(e, "LIKE", sel_library_like);
}

static void cmd_KEEP(sel_engine* e, const sel_message* m) {
    (void)m;
    user_mark(e, "KEEP", sel_library_keep);
}

/* FLAG wrong [note] | FLAG skip: flags the song being played (I171). `wrong` keeps the note for
 * sounds-wrong.txt; `skip` keeps the song out of the random order. */
static void cmd_FLAG(sel_engine* e, const sel_message* m) {
    const char* kind = m->args[0].v.s;
    int is_wrong = strcmp(kind, "wrong") == 0;
    if (!is_wrong && strcmp(kind, "skip") != 0) {
        sel_engine_report_error(e, "range", "FLAG");
    } else if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "FLAG");
    } else if (e->hist_key[0] == '\0') {
        sel_engine_report_error(e, "no_song", "FLAG");
    } else if (!sel_library_flag(e->lib, e->hist_key, kind, is_wrong && m->argc > 1 ? m->args[1].v.s : NULL, NULL)) {
        sel_engine_report_error(e, "library_failed", "FLAG");
    } else {
        e->order_qver = 0xFFFFFFFFu; /* a skip flag changes the candidates: rebuild the order on next use */
    }
}

/* RATE <0..5>: rates the song being played (I173). 0 removes the rating. A modland or hvsc song is also pushed to
 * DEViLBOX (PUT, DELETE for 0); when that cannot happen now the rating stays in the library as pending and goes
 * out on the next run. A local song is rated in the library only. */
static void cmd_RATE(sel_engine* e, const sel_message* m) {
    uint32_t stars = m->args[0].v.u;
    char source[16];
    const char* path;
    int remote;

    if (stars > 5u) {
        sel_engine_report_error(e, "range", "RATE");
    } else if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "RATE");
    } else if (e->hist_key[0] == '\0') {
        sel_engine_report_error(e, "no_song", "RATE");
    } else {
        remote = sel_devilbox_item(e->hist_key, source, sizeof source, &path);
        if (!sel_library_rating_set(e->lib, e->hist_key, (int)stars, remote ? SEL_RATING_QUEUE : SEL_RATING_LOCAL)) {
            sel_engine_report_error(e, "library_failed", "RATE");
        } else if (remote) {
            sel_library_rating_push(e->lib); /* the HTTP push runs on the library side, not on this thread (I173c) */
        }
    }
}

/* The device token of the vote service: 128 random bits as 32 hex digits, made once and kept in the pref
 * library.vote_token. The system random source where there is one; elsewhere (Amiga) the host clock, address and time of
 * day mix through xxh32 (never libc clock(): libnix opens timer.device at startup, which vamos cannot run),
 * which is enough for a rate-limit identity that guards nothing. */
static void vote_token_make(const sel_engine* e, char out[33]) {
    static const char hex[] = "0123456789abcdef";
    unsigned char raw[16];
    unsigned i;
    int got = 0;
#if !defined(__amigaos__) && !defined(AMIGA)
    FILE* f = fopen("/dev/urandom", "rb");

    if (f != NULL) {
        got = fread(raw, 1, sizeof raw, f) == sizeof raw;
        fclose(f);
    }
#endif
    if (!got) {
        uint32_t seed[4];

        seed[0] = local_ms(e);
        seed[1] = e->host->clock_ms(e->host);
        seed[2] = (uint32_t)(uintptr_t)out;
        seed[3] = (uint32_t)(uintptr_t)&seed;
        for (i = 0; i < 4; i++) {
            uint32_t h = sel_xxh32(seed, sizeof seed, i + 1u);

            raw[i * 4u] = (unsigned char)(h >> 24);
            raw[i * 4u + 1u] = (unsigned char)(h >> 16);
            raw[i * 4u + 2u] = (unsigned char)(h >> 8);
            raw[i * 4u + 3u] = (unsigned char)h;
        }
    }
    for (i = 0; i < sizeof raw; i++) {
        out[i * 2u] = hex[raw[i] >> 4];
        out[i * 2u + 1u] = hex[raw[i] & 15u];
    }
    out[32] = '\0';
}

/* VOTE [0|5] (I174, design doc section 17 "Remote": DEViLBOX votes): votes for (5), withdraws the vote on (0), or with
 * no argument reads the votes of the song being played, through the HippoPlayer service. The song is named by its
 * UADE key (xxh32 of the file, xxh32.h). The request is queued on the library job thread; the answer is read with
 * sel_library_vote_result (script directive @votes). The user's own choice is kept locally as the sticker "vote",
 * so a missing or unreachable service loses nothing. */
static void cmd_VOTE(sel_engine* e, const sel_message* m) {
    const char* arg = m->argc > 0 ? m->args[0].v.s : NULL;
    const sel_slot* cur = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    char mkey[SEL_UADE_KEY_CHARS + 1u];
    int r = -1;

    if (arg != NULL && strcmp(arg, "5") == 0) {
        r = 5;
    } else if (arg != NULL && strcmp(arg, "0") == 0) {
        r = 0;
    } else if (arg != NULL) {
        sel_engine_report_error(e, "range", "VOTE");
        return;
    }
    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "VOTE");
    } else if (cur == NULL || cur->path[0] == '\0' || e->hist_key[0] == '\0') {
        sel_engine_report_error(e, "no_song", "VOTE");
    } else if (!sel_uade_key_file(cur->path, mkey)) {
        sel_engine_report_error(e, "unreadable", "VOTE");
    } else {
        if (strlen(e->prefs.value[SEL_PREF_LIBRARY_VOTE_TOKEN]) != 32u) {
            char fresh[33];

            vote_token_make(e, fresh);
            if (sel_prefs_set(&e->prefs, "library.vote_token", fresh) != 0) {
                sel_engine_report_error(e, "library_failed", "VOTE");
                return;
            }
            if (e->config_dir[0] != '\0') (void)sel_prefs_save(&e->prefs, e->config_dir);
        }
        if (r >= 0) (void)sel_library_sticker_set(e->lib, e->hist_key, "vote", r == 5 ? "5" : "0");
        snprintf(e->vote_song, sizeof e->vote_song, "%s", e->hist_key);
        snprintf(e->vote_mkey, sizeof e->vote_mkey, "%s", mkey);
        sel_library_vote(e->lib, e->prefs.value[SEL_PREF_LIBRARY_VOTE_TOKEN], mkey, r);
    }
}

/* Position of the open song in ms, the same figure SEEK +n / -n counts from. */
static uint32_t position_ms(const sel_engine* e) {
    const sel_slot* cur = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    return cur == NULL ? 0u : (uint32_t)((uint64_t)cur->song_frames * 1000u / e->rate);
}

/* ANALYSE <song>: renders the song faster than real time through a private engine (the same replayers and
 * mixer as playback) into the desktop loudness analyser and stores the gain under the song key in the library
 * (I187, design doc 6a). <song> is a "local:<path>" key or a plain path. The Amiga never computes loudness. */
#if !defined(SEL_NO_ANALYSE)
#define SEL_ANALYSE_RATE 44100u
#define SEL_ANALYSE_CHUNK 4096u
#define SEL_ANALYSE_MAX_SECONDS 1800u /* a module that never ends is cut here */
static void analyse_sink(void* ctx, const sel_message* evt) {
    int* state = (int*)ctx;
    if (evt->id == SEL_EVT_ERROR) {
        *state = -1;
    } else if (evt->id == SEL_EVT_SONG_ENDED && *state == 0) {
        *state = 1;
    }
}

static void cmd_ANALYSE(sel_engine* e, const sel_message* m) {
    sel_engine_config cfg;
    sel_engine* job;
    sel_message msg;
    sel_loudness* an;
    sel_loudness_result r;
    int16_t buf[2u * SEL_ANALYSE_CHUNK];
    char key[SEL_DEFERRED_PATH_MAX];
    const char* path = m->args[0].v.s;
    uint64_t frames = 0;
    int state = 0;
    clock_t t0;
    double cpu;

    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "ANALYSE");
        return;
    }
    song_key(path, key, sizeof key);
    if (strncmp(path, "local:", 6) == 0) {
        path += 6;
    }
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = e->registry_count;
    cfg.registry_get = e->registry_get;
    job = sel_engine_create(e->host, &cfg);
    an = sel_loudness_new(SEL_ANALYSE_RATE);
    if (job == NULL || an == NULL) {
        sel_loudness_free(an);
        sel_engine_destroy(job);
        sel_engine_report_error(e, "analyse_failed", "alloc");
        return;
    }
    sel_engine_set_mix(job, SEL_ANALYSE_RATE, SEL_MIX_RESAMPLE_LINEAR);
    memset(&msg, 0, sizeof msg);
    msg.kind = SEL_MSG_CMD;
    msg.id = SEL_CMD_OPEN;
    msg.argc = 1;
    msg.args[0].type = SEL_ARG_STR;
    msg.args[0].v.s = path;
    sel_engine_send(job, &msg);
    memset(&msg, 0, sizeof msg);
    msg.kind = SEL_MSG_CMD;
    msg.id = SEL_CMD_PLAY;
    sel_engine_send(job, &msg);
    sel_engine_pump(job, analyse_sink, &state);
    t0 = clock();
    while (state == 0 && frames < (uint64_t)SEL_ANALYSE_MAX_SECONDS * SEL_ANALYSE_RATE) {
        uint32_t n = sel_engine_render(job, buf, SEL_ANALYSE_CHUNK);
        sel_loudness_add_s16(an, buf, n);
        frames += n;
        sel_engine_pump(job, analyse_sink, &state);
    }
    cpu = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
    sel_engine_destroy(job);
    if (state < 0 || frames == 0) {
        sel_loudness_free(an);
        sel_engine_report_error(e, "analyse_failed", path);
        return;
    }
    sel_loudness_result_get(an, &r);
    sel_loudness_free(an);
    if (!sel_library_loudness_set(e->lib, key, path, r.gain_cdb)) {
        sel_engine_report_error(e, "library_failed", "ANALYSE");
        return;
    }
    e->analyse_jobs++;
    e->analyse_gated = (uint32_t)r.gated;
    e->analyse_lufs_cdb = (int32_t)(r.lufs * 100.0 - (r.lufs < 0 ? 0.5 : -0.5));
    e->analyse_gain_cdb = r.gain_cdb;
    e->analyse_peak_cdb = (int32_t)(r.peak_db * 100.0 - (r.peak_db < 0 ? 0.5 : -0.5));
    {
        double secs = (double)frames / (double)SEL_ANALYSE_RATE;
        double speed = cpu > 1e-6 ? secs / cpu : 1000000.0;
        e->analyse_speed = speed > 1000000.0 ? 1000000u : (uint32_t)speed;
    }
}
#else
static void cmd_ANALYSE(sel_engine* e, const sel_message* m) {
    (void)m;
    sel_engine_report_error(e, "no_analyser", "ANALYSE is desktop only");
}
#endif

/* CUE_SET n: stores the playing position as cue n of the song being played (I175, table `cues`). */
static void cmd_CUE_SET(sel_engine* e, const sel_message* m) {
    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "CUE_SET");
    } else if (e->hist_key[0] == '\0' || e->cur == SEL_SLOT_NONE) {
        sel_engine_report_error(e, "no_song", "CUE_SET");
    } else if (!sel_library_cue_set(e->lib, e->hist_key, m->args[0].v.u, position_ms(e))) {
        sel_engine_report_error(e, "library_failed", "CUE_SET");
    } else {
        e->cue_sets++;
    }
}

/* CUE_GOTO n: seeks the song being played to cue n through the SEEK path (SEEKED reports the position). */
static void cmd_CUE_GOTO(sel_engine* e, const sel_message* m) {
    uint32_t pos = 0;
    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "CUE_GOTO");
    } else if (e->hist_key[0] == '\0' || e->cur == SEL_SLOT_NONE) {
        sel_engine_report_error(e, "no_song", "CUE_GOTO");
    } else if (!sel_library_cue_get(e->lib, e->hist_key, m->args[0].v.u, &pos)) {
        sel_engine_report_error(e, "no_such_cue", "CUE_GOTO");
    } else {
        e->cue_gotos++;
        reopen_current(e, SEL_BEGIN_SEEK, SEL_SUBSONG_DEFAULT, pos > 4000000u ? 4000000u : pos, "CUE_GOTO");
    }
}

/* STICKER_*: per-key name/value pairs in the library (I176, D-M3-17). Answers are LIBRARY_RESULT events with
 * SRC_KEY (song key), SRC_NAME (sticker name), SRC_PATH (value) and SRC_COUNT (rows). */
#define SEL_STICKER_ANSWERS 8u
static void sticker_answer(sel_engine* e, const char* key, const char* name, const char* value, uint32_t count) {
    uint32_t slot = e->st_slot++ % SEL_STICKER_ANSWERS;
    sel_tag* t = e->st_tags[slot];
    sel_message r;
    snprintf(e->st_key[slot], sizeof e->st_key[slot], "%s", key);
    snprintf(e->st_name[slot], sizeof e->st_name[slot], "%s", name);
    snprintf(e->st_value[slot], sizeof e->st_value[slot], "%s", value);
    t[0].tag = SEL_SRC_KEY;
    t[0].value = (uintptr_t)e->st_key[slot];
    t[1].tag = SEL_SRC_NAME;
    t[1].value = (uintptr_t)e->st_name[slot];
    t[2].tag = SEL_SRC_PATH;
    t[2].value = (uintptr_t)e->st_value[slot];
    t[3].tag = SEL_SRC_COUNT;
    t[3].value = count;
    t[4].tag = SEL_TAG_END;
    t[4].value = 0;
    evt_init(&r, SEL_EVT_LIBRARY_RESULT);
    evt_tags(&r, t);
    emit(e, &r);
}

static int sticker_ready(sel_engine* e, const char* who) {
    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", who);
        return 0;
    }
    e->sticker_ops++;
    return 1;
}

/* RADIO <seed> | RADIO off (I186, D-M3-10): keeps at least SEL_RADIO_AHEAD songs queued after the playing one,
 * each drawn by sel_library_radio_next (genre, composer, rating steering; skip-flagged songs never). The seed
 * is a song key or a queue path; with nothing playing it is queued and started first. */
#define SEL_RADIO_AHEAD 5u

static void radio_fill(sel_engine* e) {
    uint32_t cur, ahead;

    if (e->radio_seed[0] == '\0' || e->lib == NULL) {
        return;
    }
    cur = e->q_playing == 0 ? SEL_QUEUE_NONE : sel_queue_index_of(&e->queue, e->q_playing);
    ahead = cur == SEL_QUEUE_NONE ? e->queue.count : e->queue.count - cur - 1u;
    while (ahead < SEL_RADIO_AHEAD) {
        char pick[SEL_DEFERRED_PATH_MAX];
        uint32_t before = e->queue.count;

        e->radio_rng = sel_xorshift32(e->radio_rng);
        if (!sel_library_radio_next(e->lib, e->radio_seed, e->radio_rng, e->radio_last, pick, sizeof pick)) {
            break;
        }
        queue_insert(e, e->queue.count, pick, "RADIO");
        if (e->queue.count == before) {
            break;
        }
        strcpy(e->radio_last, pick);
        e->radio_appends++;
        ahead++;
    }
    if (ahead < e->radio_min_ahead) {
        e->radio_min_ahead = ahead;
    }
}

static void cmd_RADIO(sel_engine* e, const sel_message* m) {
    const char* seed = m->args[0].v.s;
    size_t n = strlen(seed);

    if (strcmp(seed, "off") == 0) {
        e->radio_seed[0] = '\0';
        return;
    }
    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "RADIO");
        return;
    }
    if (n == 0 || n >= sizeof e->radio_seed) {
        sel_engine_report_error(e, "range", "RADIO");
        return;
    }
    memcpy(e->radio_seed, seed, n + 1u);
    e->radio_last[0] = '\0';
    e->radio_rng = sel_shuffle_seed(e->rng);
    e->radio_min_ahead = 0xFFFFFFFFu;
    if (e->q_playing == 0) {
        queue_insert(e, e->queue.count, seed, "RADIO");
        if (e->queue.count > 0) {
            queue_start(e, e->queue.count - 1u);
        }
    }
    radio_fill(e);
}

/* Share links (I180, D-M3-11). OPEN <link> parses either form; SHARE song|position|playlist|view writes one. */
static int open_share_link(sel_engine* e, const char* text) {
    sel_share_link l;
    char open_path[SEL_DEFERRED_PATH_MAX];
    sel_message sub;

    if (!sel_share_parse(text, &l)) {
        sel_engine_report_error(e, "bad_link", "OPEN");
        return 0;
    }
    e->links_parsed++;
    memset(&sub, 0, sizeof sub);
    sub.kind = SEL_MSG_CMD;
    sub.argc = 1;
    sub.args[0].type = SEL_ARG_STR;
    if (l.kind == SEL_SHARE_SONG) {
        if (!sel_share_song_open_path(&l, open_path, sizeof open_path)) {
            sel_engine_report_error(e, "bad_link", "OPEN");
            return 0;
        }
        e->q_playing = 0;
        return begin_song(e, SEL_BEGIN_OPEN, open_path, l.has_sub ? l.sub : SEL_SUBSONG_DEFAULT,
                          l.has_t ? (l.t_ms > 4000000u ? 4000000u : l.t_ms) : 0);
    }
    if (l.kind == SEL_SHARE_VIEW) {
        sub.id = SEL_CMD_PLAY_QUERY;
        sub.args[0].v.s = l.path;
        cmd_PLAY_QUERY(e, &sub);
        return 1;
    }
    if (strcmp(l.source, "local") != 0) { /* only local playlist files can be loaded without the library */
        sel_engine_report_error(e, "no_such_playlist", "OPEN");
        return 0;
    }
    sub.id = SEL_CMD_PLAYLIST_LOAD;
    sub.argc = 2;
    sub.args[0].v.s = l.path;
    sub.args[1].type = SEL_ARG_STR;
    sub.args[1].v.s = "replace";
    cmd_PLAYLIST_LOAD(e, &sub);
    return 1;
}

static void share_answer(sel_engine* e, const sel_share_link* l, const char* key) {
    char text[SEL_STICKER_VALUE_MAX], dev[SEL_STICKER_VALUE_MAX];
    if (!sel_share_format(l, text, sizeof text) || !sel_share_format_devilbox(l, dev, sizeof dev)) {
        sel_engine_report_error(e, "range", "SHARE");
        return;
    }
    e->links_made++;
    sticker_answer(e, key, "share", text, 1);
    sticker_answer(e, key, "share_devilbox", dev, 1);
}

static void cmd_SHARE(sel_engine* e, const sel_message* m) {
    const char* what = m->args[0].v.s;
    sel_share_link l;
    const sel_slot* cur = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];

    memset(&l, 0, sizeof l);
    if (strcmp(what, "playlist") == 0 || strcmp(what, "view") == 0) {
        int view = what[0] == 'v';
        if (m->argc < 2 || (!view && !sel_share_key_split(m->args[1].v.s, l.source, sizeof l.source, l.path, sizeof l.path))) {
            sel_engine_report_error(e, "range", "SHARE");
            return;
        }
        l.kind = view ? SEL_SHARE_VIEW : SEL_SHARE_PLAYLIST;
        if (view) snprintf(l.path, sizeof l.path, "%s", m->args[1].v.s);
        share_answer(e, &l, what);
        return;
    }
    if (strcmp(what, "song") != 0 && strcmp(what, "position") != 0) {
        sel_engine_report_error(e, "range", "SHARE");
        return;
    }
    if (cur == NULL || e->hist_key[0] == '\0') {
        sel_engine_report_error(e, "no_song", "SHARE");
        return;
    }
    l.kind = SEL_SHARE_SONG;
    if (sel_cache_is_remote(e->hist_key)) {
        sel_share_key_split(e->hist_key, l.source, sizeof l.source, l.path, sizeof l.path);
    } else {
        snprintf(l.source, sizeof l.source, "local");
        snprintf(l.path, sizeof l.path, "%s", e->hist_key);
    }
    if (cur->subsong != SEL_SUBSONG_DEFAULT && cur->subsong > 0) {
        l.has_sub = 1;
        l.sub = cur->subsong;
    }
    if (what[0] == 'p' && position_ms(e) > 0) {
        l.has_t = 1;
        l.t_ms = position_ms(e);
    }
    share_answer(e, &l, e->hist_key);
}

static void cmd_STICKER_SET(sel_engine* e, const sel_message* m) {
    if (sticker_ready(e, "STICKER_SET") &&
        !sel_library_sticker_set(e->lib, m->args[0].v.s, m->args[1].v.s, m->args[2].v.s)) {
        sel_engine_report_error(e, "library_failed", "STICKER_SET");
    }
}

static void cmd_STICKER_GET(sel_engine* e, const sel_message* m) {
    char value[SEL_STICKER_VALUE_MAX];
    if (!sticker_ready(e, "STICKER_GET")) return;
    if (!sel_library_sticker_get(e->lib, m->args[0].v.s, m->args[1].v.s, value, sizeof value)) {
        sel_engine_report_error(e, "no_such_sticker", m->args[1].v.s);
    } else {
        sticker_answer(e, m->args[0].v.s, m->args[1].v.s, value, 1);
    }
}

static void cmd_STICKER_DEL(sel_engine* e, const sel_message* m) {
    if (sticker_ready(e, "STICKER_DEL") && !sel_library_sticker_del(e->lib, m->args[0].v.s, m->args[1].v.s)) {
        sel_engine_report_error(e, "no_such_sticker", m->args[1].v.s);
    }
}

/* STICKER_LIST key: one answer per sticker in name order (the first SEL_STICKER_ANSWERS of them); SRC_COUNT is
 * the number of stickers the key has. No stickers: no answer. */
static void cmd_STICKER_LIST(sel_engine* e, const sel_message* m) {
    char buf[2048];
    uint32_t rows = 0, n = 0;
    const char* p = buf;
    if (!sticker_ready(e, "STICKER_LIST")) return;
    if (!sel_library_sticker_list(e->lib, m->args[0].v.s, buf, sizeof buf, &rows)) {
        sel_engine_report_error(e, "library_failed", "STICKER_LIST");
        return;
    }
    while (*p != '\0' && n < SEL_STICKER_ANSWERS) {
        const char* value = p + strlen(p) + 1;
        sticker_answer(e, m->args[0].v.s, p, value, rows);
        p = value + strlen(value) + 1;
        n++;
    }
}

/* I178 folder tree (design doc 9c): the library answers a refusal with a code; map it to the error text. */
static void folder_report(sel_engine* e, int code, const char* who) {
    static const char* const names[] = {"library_failed", "no_such_folder", "folder_cycle", "folder_not_empty",
                                        "no_such_playlist", "library_failed"};
    sel_engine_report_error(e, names[code >= 0 && code <= SEL_FOLDER_ERR_FAILED ? code : SEL_FOLDER_ERR_FAILED], who);
}

static int folder_ready(sel_engine* e, const char* who) {
    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", who);
        return 0;
    }
    e->folder_ops++;
    return 1;
}

/* I179: rebuilds the rows of the folders view source from the library tree. Called when the source is
 * selected and after every folder command that changed the tree, so the text UI sees the tree only through
 * the view model. */
static void folders_reload(sel_engine* e) {
    char tree[4096];
    uint32_t lines = 0;
    tree[0] = '\0';
    if (e->lib != NULL && sel_library_folder_tree(e->lib, tree, sizeof tree, &lines) != SEL_FOLDER_OK) {
        tree[0] = '\0';
    }
    e->folder_view_rows += sel_view_set_folders(&e->view, tree);
}

/* FOLDER_ADD name [parent]: creates the folder (parent 0 or absent = root); the answer carries the new id. */
static void cmd_FOLDER_ADD(sel_engine* e, const sel_message* m) {
    uint32_t id = 0;
    int rc;
    char text[16];
    if (!folder_ready(e, "FOLDER_ADD")) return;
    rc = sel_library_folder_add(e->lib, m->args[0].v.s, m->argc > 1 ? (uint32_t)m->args[1].v.u : 0u, &id);
    if (rc != SEL_FOLDER_OK) {
        folder_report(e, rc, "FOLDER_ADD");
        return;
    }
    snprintf(text, sizeof text, "%u", (unsigned)id);
    folders_reload(e);
    sticker_answer(e, "folder", m->args[0].v.s, text, id);
}

static void cmd_FOLDER_MOVE(sel_engine* e, const sel_message* m) {
    int rc;
    if (!folder_ready(e, "FOLDER_MOVE")) return;
    rc = sel_library_folder_move(e->lib, (uint32_t)m->args[0].v.u, (uint32_t)m->args[1].v.u);
    if (rc != SEL_FOLDER_OK) folder_report(e, rc, "FOLDER_MOVE");
    else folders_reload(e);
}

static void cmd_FOLDER_DEL(sel_engine* e, const sel_message* m) {
    int rc;
    if (!folder_ready(e, "FOLDER_DEL")) return;
    rc = sel_library_folder_del(e->lib, (uint32_t)m->args[0].v.u);
    if (rc != SEL_FOLDER_OK) folder_report(e, rc, "FOLDER_DEL");
    else folders_reload(e);
}

static void cmd_PLAYLIST_MOVE(sel_engine* e, const sel_message* m) {
    int rc;
    if (!folder_ready(e, "PLAYLIST_MOVE")) return;
    rc = sel_library_playlist_move(e->lib, (uint32_t)m->args[0].v.u, (uint32_t)m->args[1].v.u);
    if (rc != SEL_FOLDER_OK) folder_report(e, rc, "PLAYLIST_MOVE");
    else folders_reload(e);
}

static void cmd_LIBRARY_SYNC(sel_engine* e, const sel_message* m) {
    sel_local_scan_config scfg;
    sel_modland_sync_stats st;
    sel_hvsc_sync_stats hv;
    sel_hippo_sync_stats hp;
    int64_t now;
    int ok;
    int is_hvsc, is_hippo;

    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "LIBRARY_SYNC");
        return;
    }
    is_hvsc = strcmp(m->args[0].v.s, "hvsc") == 0;
    is_hippo = strcmp(m->args[0].v.s, "hippo") == 0;
    if (!is_hvsc && !is_hippo && strcmp(m->args[0].v.s, "modland") != 0) {
        sel_engine_report_error(e, "unknown_source", m->args[0].v.s);
        return;
    }
    memset(&scfg, 0, sizeof scfg);
    scfg.host = e->host;
    scfg.lib = e->lib;
    scfg.registry_count = e->registry_count;
    scfg.registry_get = e->registry_get;
    now = m->argc > 2 ? (int64_t)m->args[2].v.u : (int64_t)time(NULL);
    scfg.now = now;
    memset(&st, 0, sizeof st);
    memset(&e->sync_info, 0, sizeof e->sync_info);
    if (is_hippo) {
        ok = sel_hippo_sync(&scfg, m->args[1].v.s, &hp);
        st.listing_rows = hp.lists;
        st.skipped_rows = hp.skipped;
        e->sync_info.hippo_lists = hp.lists;
        e->sync_info.prg_files = hp.prg_files;
        e->sync_info.prg_entries = hp.prg_entries;
        e->sync_info.prg_dividers = hp.prg_dividers;
        e->sync_info.prg_stealth = hp.prg_stealth;
        e->sync_info.prg_subsongs = hp.prg_subsongs;
    } else if (is_hvsc) {
        ok = sel_hvsc_sync(&scfg, m->args[1].v.s, &hv);
        st.listing_rows = hv.listing_rows;
        st.skipped_rows = hv.skipped_rows;
        e->sync_info.songlengths = hv.songlengths;
        e->sync_info.stil_entries = hv.stil_entries;
        e->sync_info.subtunes = hv.subtunes;
    } else if (sel_modland_is_url(m->args[1].v.s)) {
        sel_modland_net_stats ns;

        ok = sel_modland_net_sync(&scfg, m->args[1].v.s, now, &st, &ns);
        e->sync_info.net_zip_bytes = ns.zip_bytes;
        e->sync_info.net_listing_bytes = ns.listing_bytes;
        e->sync_info.net_changelogs_fetched = ns.changelogs_fetched;
        e->sync_info.net_changelogs_missing = ns.changelogs_missing;
        e->sync_info.net_requests = ns.http_requests;
    } else {
        ok = sel_modland_sync(&scfg, m->args[1].v.s, now, &st);
    }
    if (ok && !is_hvsc && !is_hippo) {
        persist_new_this_week(e, st.new_this_week);
    }
    e->sync_info.listing_rows = st.listing_rows;
    e->sync_info.skipped_rows = st.skipped_rows;
    e->sync_info.changelogs = st.changelogs;
    e->sync_info.cl_added = st.cl_added;
    e->sync_info.cl_removed = st.cl_removed;
    e->sync_info.cl_moved = st.cl_moved;
    e->sync_info.cl_updated = st.cl_updated;
    e->sync_info.new_this_week = st.new_this_week;
    if (!ok) {
        sel_engine_report_error(e, "sync_failed", m->args[1].v.s);
        return;
    }
    emit_library_path(e, m->args[1].v.s, st.listing_rows);
}

/* LIBRARY_FETCH <source>:<path>[#md5=<hex>]: fetches the file and its companions into the library's
 * file cache through cfg.library (I165, I166, I166a). The answer is a LIBRARY_RESULT carrying the cache
 * path and the number of files delivered; a wrong md5 on every mirror answers ERROR md5_mismatch. */
static void cmd_LIBRARY_FETCH(sel_engine* e, const sel_message* m) {
    char out[SEL_BROWSE_PATH_MAX];
    uint32_t files = 0;
    char rules[512];
    int rc;

    if (e->lib == NULL) {
        sel_engine_report_error(e, "no_library", "LIBRARY_FETCH");
        return;
    }
    sel_companion_rules(e->registry_count, e->registry_get, rules, sizeof rules);
    rc = sel_library_fetch(e->lib, m->args[0].v.s, rules, out, sizeof out, &files);
    if (rc < 0) {
        sel_engine_report_error(e, "md5_mismatch", m->args[0].v.s);
        return;
    }
    if (rc == 0) {
        sel_engine_report_error(e, "fetch_failed", m->args[0].v.s);
        return;
    }
    emit_library_path(e, out, files);
}

/* ---- view ------------------------------------------------------------ */

/* Row callback of the view (I174a): the stored vote count of a song key, as text. The count is the sticker "votes" of
 * the song key, the same per-song store as the sticker "vote" (the user's own choice). */
static int view_votes(void* ctx, const char* path, char* out) {
    sel_engine* e = ctx;
    char v[SEL_STICKER_VALUE_MAX];

    if (!sel_library_sticker_get(e->lib, path, "votes", v, sizeof v)) return 0;
    snprintf(out, SEL_VIEW_VOTES_MAX, "%s", v);
    return 1;
}

/* Pump thread: a finished vote request that carries a count is stored as the sticker "votes" of the song it was made
 * for, so the playlist row shows it (and it survives a restart). A failed or refused request keeps the stored value. */
static void vote_store(sel_engine* e) {
    sel_vote_result vr;

    if (e->lib == NULL || e->vote_song[0] == '\0') return;
    sel_library_vote_result(e->lib, &vr);
    if (vr.seq == e->vote_seen) return;
    e->vote_seen = vr.seq;
    if (vr.status == SEL_VOTE_OK && strcmp(vr.mkey, e->vote_mkey) == 0) {
        char text[SEL_VIEW_VOTES_MAX];

        snprintf(text, sizeof text, "%u", vr.count);
        if (sel_library_sticker_set(e->lib, e->vote_song, "votes", text)) e->votes_generation++;
    }
}

static void view_src(const sel_engine* e, sel_view_src* s) {
    s->queue = e->view.pub.source == SEL_VIEW_QUERY ? &e->qview : &e->queue;
    s->browse = &e->browse;
    s->meta = e->view.pub.source == SEL_VIEW_QUERY ? e->qmeta : NULL;
    s->meta_off = e->qmeta_off;
    s->meta_rows = e->qmeta_rows;
    s->playing = e->q_playing;
    s->title_format = e->prefs.value[SEL_PREF_VIEW_TITLE_FORMAT];
    s->prefs_generation = e->prefs.generation;
    s->votes = e->lib != NULL ? view_votes : NULL;
    s->votes_ctx = (void*)e;
    s->votes_generation = e->votes_generation;
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

/* Copies the last result's metadata with an offset per row, so the query view shows
 * title, artist, format and duration instead of keys. Rows past the buffer have none. */
static void snapshot_meta(sel_engine* e) {
    uint32_t i, at = 0;

    memcpy(e->qmeta, e->res_meta, e->res_meta_used);
    e->qmeta_rows = e->res_count;
    for (i = 0; i < e->res_count; i++) {
        uint32_t f, start = at;
        for (f = 0; f < 4u && at < e->res_meta_used; f++) {
            at += (uint32_t)strlen(e->qmeta + at) + 1u;
        }
        e->qmeta_off[i] = f == 4u ? start : UINT32_MAX;
    }
}

/* VIEW_SOURCE query lists the rows of the last library search; queue and browse switch back.
 * The query list is a snapshot, so later searches do not move a view that is showing it. */
static void cmd_VIEW_SOURCE(sel_engine* e, const sel_message* m) {
    const char* what = m->args[0].v.s;
    sel_view_src s;
    uint32_t i;

    if (strcmp(what, "playlist") == 0) {
        what = "queue"; /* the playlist being played is the queue; playlist folders arrive with I19x */
    } else if (strcmp(what, "home") == 0) {
        uint32_t fts = 0;
        /* Home view before I191: the songs with a play in the history table, as a query snapshot.
         * Without a library it is an empty list. I191 adds recently played order, jump back in and new this week. */
        if (e->lib == NULL) {
            e->res_count = 0;
            e->res_meta_used = 0;
        } else if (!run_search(e, "plays:>0", "VIEW_SOURCE", &fts)) {
            return;
        }
        what = "query";
    }
    if (strcmp(what, "query") == 0) {
        sel_queue_clear(&e->qview);
        snapshot_meta(e);
        for (i = 0; i < e->res_count; i++) {
            uint32_t id;
            if (!sel_queue_insert(&e->qview, SEL_QUEUE_NONE, sel_engine_result_key(e, i), &id)) {
                sel_engine_report_error(e, "queue_full", "VIEW_SOURCE");
                break;
            }
        }
    } else if (strcmp(what, "folders") == 0) {
        if (e->lib == NULL) {
            sel_engine_report_error(e, "no_library", "VIEW_SOURCE");
            return;
        }
        folders_reload(e);
    } else if (strcmp(what, "queue") != 0 && strcmp(what, "browse") != 0) {
        sel_engine_report_error(e, "unsupported", "VIEW_SOURCE");
        return;
    }
    if (strcmp(what, "query") == 0) {
        e->view.cursor_of[SEL_VIEW_QUERY] = 0; /* a new result list starts at its first row */
        e->view.id_of[SEL_VIEW_QUERY] = 0;
        if (e->view.pub.source == SEL_VIEW_QUERY) {
            e->view.pub.cursor = 0;
            e->view.pub.cursor_id = 0;
        }
    }
    view_src(e, &s);
    e->view.active = 1;
    sel_view_show(&e->view, &s, what);
    view_src(e, &s);
    sel_view_refresh(&e->view, &s);
    e->view.force_changed = 1;
}

const char* sel_view_source_label(uint32_t source) {
    return source == SEL_VIEW_QUEUE ? "queue" : source == SEL_VIEW_BROWSE ? "browse" : source == SEL_VIEW_QUERY ? "query" : source == SEL_VIEW_FOLDERS ? "folders" : "unknown";
}

const char* sel_engine_view_cursor_key(const sel_engine* e) {
    const sel_view_state* v = &e->view.pub;
    const sel_queue* q = v->source == SEL_VIEW_QUERY ? &e->qview : &e->queue;
    uint32_t at;

    if (v->source == SEL_VIEW_BROWSE || v->source == SEL_VIEW_FOLDERS || v->row_count == 0) {
        return NULL;
    }
    at = sel_queue_index_of(q, v->cursor_id);
    return at == SEL_QUEUE_NONE ? NULL : sel_queue_path(q, at);
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
    if (v->source == SEL_VIEW_FOLDERS) {
        /* A playlist row loads the playlist into the queue and starts its first song; a folder row has
         * nothing to start (the tree is always shown open). */
        const sel_view_row* r = &e->view.frows[at];
        char keys[4096];
        uint32_t n = 0, before = e->queue.count;
        const char* k;
        if (r->kind != SEL_ROW_PLAYLIST || e->lib == NULL ||
            sel_library_playlist_keys(e->lib, r->id, keys, sizeof keys, &n) != SEL_FOLDER_OK) {
            return;
        }
        for (k = keys; *k != '\0'; k += strlen(k) + 1u) {
            queue_insert(e, e->queue.count, k, "VIEW_ACTIVATE");
        }
        if (e->queue.count > before) {
            queue_start(e, before);
        }
        return;
    }
    if (v->source != SEL_VIEW_BROWSE) {
        uint32_t qi;
        if (sel_view_enter(&e->view, &s, &qi)) {
            if (v->source == SEL_VIEW_QUERY) {
                /* A query row plays like PLAY_QUERY of one song: it joins the queue and starts. */
                uint32_t before = e->queue.count;
                queue_insert(e, before, sel_queue_path(&e->qview, qi), "VIEW_ACTIVATE");
                if (e->queue.count > before) {
                    queue_start(e, before);
                }
                return;
            }
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
    e->rp_host = *host;
    e->rp_host.config_get = rp_config_get;
    e->rp_host.ctx = e;
    e->state.volume = 100;
    sel_loudness_gain_set(&e->loud, 0, SEL_GAIN_UNITY);
    sel_timer_init(&e->timer);
    (void)local_ms(e); /* primes the C library time zone state outside the render path */
    e->loops = 1;
    e->fade_ms = 3000;
    e->silence_ms = 5000;
    copy_str(e->state.options[SEL_OPT_REPEAT], SEL_OPTION_VALUE_MAX, "off");
    copy_str(e->state.options[SEL_OPT_LOOPS], SEL_OPTION_VALUE_MAX, "1");
    copy_str(e->state.options[SEL_OPT_FADE_MS], SEL_OPTION_VALUE_MAX, "3000");
    copy_str(e->state.options[SEL_OPT_MAX_SONG_MS], SEL_OPTION_VALUE_MAX, "0");
    copy_str(e->state.options[SEL_OPT_SILENCE_MS], SEL_OPTION_VALUE_MAX, "5000");
    copy_str(e->state.options[SEL_OPT_CROSSFADE], SEL_OPTION_VALUE_MAX, "0");
    e->nx = SEL_SLOT_NONE;
    e->nx_slot = SEL_SLOT_NONE;
    e->state.mixed_path = 1;
    e->rate = 44100;
    e->mixed_path = 1;
    e->resampler = SEL_MIX_RESAMPLE_LINEAR;
    sel_mix_stage_init(&e->stage, e->rate);
    sel_viz_tap_init(&e->viz);
    e->state.viz = sel_viz_current(&e->viz);
    {
        /* The 256-point FFT is skipped below a 68030; an absent probe means a desktop. */
        int64_t cpu = cfg ? sel_tag_find(cfg->machine, SEL_MACHINE_CPU, SEL_CPU_DESKTOP) : SEL_CPU_DESKTOP;
        e->viz_spectrum = cpu == SEL_CPU_UNKNOWN || cpu >= SEL_CPU_68030;
    }
    if (cfg) {
        e->registry_count = cfg->registry_count;
        e->registry_get = cfg->registry_get;
        e->lib = cfg->library;
    }
    {
        /* FAST_RAM is a uintptr_t: a 4 GB or 8 GB host would wrap to 0 in a plain uint32_t cast and get the
         * smallest queue. Saturate instead. */
        unsigned long long fast = cfg ? (unsigned long long)sel_tag_find(cfg->machine, SEL_MACHINE_FAST_RAM, 0) : 0;
        sel_queue_default_sizes(fast > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)fast, &queue_cap, &queue_text);
    }
    if (cfg && cfg->queue_capacity) {
        queue_cap = cfg->queue_capacity;
    }
    if (cfg && cfg->queue_text_bytes) {
        queue_text = cfg->queue_text_bytes;
    }
    sel_prefs_init(&e->prefs);
    if (cfg) {
    }
    if (cfg && cfg->config_dir && strlen(cfg->config_dir) < sizeof e->config_dir) {
        strcpy(e->config_dir, cfg->config_dir);
        (void)sel_prefs_load(&e->prefs, e->config_dir);
        sel_devilbox_set_config_dir(e->config_dir);
        sel_library_rating_push(e->lib); /* ratings left over from a run without the server (I173, I173c) */
    }
    e->state.led_on = strcmp(e->prefs.value[SEL_PREF_OUTPUT_LED], "on") == 0;
    e->state.stereo_width = (uint32_t)strtoul(e->prefs.value[SEL_PREF_MIX_STEREO_WIDTH], NULL, 10);
    if (e->state.stereo_width > 100u) {
        e->state.stereo_width = 100u;
    }
    sel_engine_set_stage(e, e->state.led_on, e->state.stereo_width);
    copy_str(e->state.output, SEL_OUTPUT_NAME_MAX, "null");
    e->cur = SEL_SLOT_NONE;
    e->audio_slot = SEL_SLOT_NONE;
    e->act = SEL_SLOT_NONE;
    if (!sel_spsc_init(&e->cmd_q, host, cmd_depth) || !sel_spsc_init(&e->evt_q, host, evt_depth) ||
        !sel_spsc_init(&e->swap_q, host, SEL_SWAP_RING_DEPTH) ||
        !sel_spsc_init(&e->ret_q, host, SEL_SWAP_RING_DEPTH) ||
        !sel_queue_init(&e->queue, host, queue_cap, queue_text) ||
        !sel_queue_init(&e->qview, host, queue_cap, queue_text) ||
        !sel_history_init(&e->history, host, queue_cap, queue_text) ||
        (e->order = host->mem_alloc(host, (size_t)queue_cap * sizeof *e->order, SEL_MEM_FAST)) == NULL ||
        (e->order_seen = host->mem_alloc(host, queue_cap, SEL_MEM_FAST)) == NULL ||
        (e->pl_entries = host->mem_alloc(host, (size_t)queue_cap * sizeof *e->pl_entries, SEL_MEM_FAST)) == NULL ||
        (e->pl_text = host->mem_alloc(host, (size_t)queue_text + 16u, SEL_MEM_FAST)) == NULL ||
        (e->pl_file = host->mem_alloc(host, (size_t)queue_text + 16u, SEL_MEM_FAST)) == NULL ||
        (e->res_text = host->mem_alloc(host, (size_t)queue_text + 16u, SEL_MEM_FAST)) == NULL ||
        (e->res_meta = host->mem_alloc(host, (size_t)queue_text + 16u, SEL_MEM_FAST)) == NULL ||
        (e->qmeta = host->mem_alloc(host, (size_t)queue_text + 16u, SEL_MEM_FAST)) == NULL ||
        (e->qmeta_off = host->mem_alloc(host, (size_t)queue_cap * sizeof *e->qmeta_off, SEL_MEM_FAST)) == NULL ||
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
        if (e->res_text) {
            e->host->mem_free(e->host, e->res_text);
        }
        if (e->res_meta) {
            e->host->mem_free(e->host, e->res_meta);
        }
        if (e->qmeta) {
            e->host->mem_free(e->host, e->qmeta);
        }
        if (e->qmeta_off) {
            e->host->mem_free(e->host, e->qmeta_off);
        }
        sel_queue_free(&e->qview, e->host);
        sel_dsp_chain_free(&e->dsp);
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
    e->state.mixed_path = e->mixed_path;
    if (sel_viz_acquire(&e->viz)) {
        sel_viz_analyse(&e->viz, (int)e->viz_spectrum);
    }
    e->state.viz = sel_viz_current(&e->viz);
    if (e->pat_ack == e->pat_req && e->pat_res.seq != e->pat_pub.seq) {
        e->pat_pub = e->pat_res;
    }
    e->state.pattern = &e->pat_pub;
    memset(song, 0, sizeof *song);
    if (sl == NULL || sl->rp_state == NULL) {
        e->panel_state = NULL;
        e->panel_rows = 0;
    } else if (sl->rp_state != e->panel_state) {
        /* I185: the catalogue rows of a newly opened song come from the library, on this (pump) thread only. */
        e->panel_state = sl->rp_state;
        e->panel_rows = 0;
        e->panel[0] = '\0';
        if (e->lib != NULL && sl->path != NULL) {
            e->panel_loads++;
            if (!sel_library_song_panel(e->lib, sl->path, e->panel, sizeof e->panel, &e->panel_rows)) {
                e->panel_rows = 0;
            }
        }
    }
    e->state.panel = e->panel_rows > 0 ? e->panel : NULL;
    e->state.panel_rows = e->panel_rows;
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
    timer_pump(e);
    reclaim_slots(e);
    if (e->deferred) {
        begin_song(e, e->deferred, e->deferred_path, e->deferred_subsong, e->deferred_seek);
    }
    if (e->nx_slot != SEL_SLOT_NONE && e->audio_slot == e->nx_slot) {
        commit_next(e);
    } else {
        prepare_next(e);
    }
    prefetch_sync(e);
    if (e->lib != NULL && (e->pf_id != 0 || e->sm_key != 0)) {
        sel_library_prefetch_poll(e->lib); /* builds without threads run the parked request here, after playback started */
    }
    if (e->cur != SEL_SLOT_NONE) {
        sel_slot* sl = &e->slot[e->cur];
        int advanced = 0;
        if (sl->ended && !sl->reported && e->nx_slot == SEL_SLOT_NONE) {
            sel_message ev;
            uint32_t reason = sl->pol.reason != SEL_END_NONE ? sl->pol.reason : (uint32_t)SEL_END_END;
            uint32_t next;
            sl->reported = 1;
            if (e->repeat == 1u && reason == SEL_END_END && e->q_playing != 0 &&
                queue_start(e, sel_queue_index_of(&e->queue, e->q_playing))) {
                /* REPEAT one: the song restarts without ending; the finished round counts as a play. */
                hist_end(e, 0);
                advanced = 1;
            } else {
                next = queue_next_index(e);
                evt_init(&ev, SEL_EVT_SONG_ENDED);
                evt_str(&ev, k_end_reason_names[reason]);
                emit(e, &ev);
                hist_end(e, 0);
                if (strcmp(e->state.options[SEL_OPT_STOP_AFTER_CURRENT], "on") == 0) {
                    /* One-shot: stop here, the next PLAY or NEXT continues with the following entry. */
                    set_option(e, SEL_OPT_STOP_AFTER_CURRENT, "off");
                    e->q_hint = next < e->queue.count ? next : 0;
                    e->q_playing = 0;
                    e->song_over = 1;
                    set_play(e, SEL_STOPPED);
                } else if (next < e->queue.count && queue_start(e, next)) {
                    advanced = 1;
                } else {
                    queue_finish(e);
                }
            }
        }
        if (!advanced && sl->rp_state != NULL && sl->song_frames / e->rate != sl->pos_emitted) {
            emit_position(e, sl);
        }
        if (!advanced && sl->rp_state != NULL && e->state.play == SEL_PLAYING) {
            scrobble_check(e, sl);
        }
    }
    refresh_snapshot(e);
    vote_store(e);
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
    sel_dsp_chain_set_rate(&e->dsp, e->host, e->rate);
    {
        sel_mix_stage old = e->stage;
        sel_mix_stage_init(&e->stage, e->rate);
        e->stage.paula_output = old.paula_output;
        e->stage.led_frames = old.led_frames;
        e->stage.width_frames = old.width_frames;
        sel_mix_stage_set_width(&e->stage, old.width);
        sel_mix_stage_set_led(&e->stage, old.led_on);
    }
}

void sel_engine_set_stage(sel_engine* e, uint32_t led_on, uint32_t width_percent) {
    sel_mix_stage_set_led(&e->stage, led_on);
    sel_mix_stage_set_width(&e->stage, width_percent);
}

void sel_engine_set_paula_output(sel_engine* e, uint32_t on) {
    e->stage.paula_output = on != 0u;
}

void sel_engine_set_voice_sink(sel_engine* e, const sel_voices* sink) {
    e->voice_sink = sink;
}

/* Routing table (plan M2 phase 3, I121): a VOICE-kind replayer (voice_count > 0) whose voices fit the output's
 * backend (voice_max, 0 = no limit) plays through it; every other replayer, RENDER kind included, goes through the mixer. */
uint32_t sel_engine_voice_route(uint32_t voice_count, int has_sink, uint32_t voice_max) {
    if (!has_sink || voice_count == 0u || (voice_max != 0u && voice_count > voice_max)) {
        return SEL_ROUTE_MIXER;
    }
    return SEL_ROUTE_DIRECT;
}

uint32_t sel_engine_routed_direct(const sel_engine* e) {
    return e->route_direct;
}

void sel_engine_set_output(sel_engine* e, const sel_tag* out_hooks) {
    e->mixed_path = sel_tag_find(out_hooks, SEL_OUT_VOICES, 0) == 0;
    e->voice_max = (uint32_t)sel_tag_find(out_hooks, SEL_OUT_VOICE_MAX, 0);
    if (sel_tag_find(out_hooks, SEL_OUT_PAULA_HW, 0) != 0) {
        sel_engine_set_paula_output(e, 1);
    }
    if (sel_tag_find(out_hooks, SEL_OUT_HEADPHONES, 0) != 0 && e->mixed_path &&
        strcmp(e->prefs.value[SEL_PREF_DSP_CROSSFEED], "off") != 0) {
        sel_message m;
        memset(&m, 0, sizeof m);
        m.kind = SEL_MSG_CMD;
        m.id = SEL_CMD_DSP;
        m.argc = 2;
        m.args[0].type = SEL_ARG_STR;
        m.args[0].v.s = "crossfeed";
        m.args[1].type = SEL_ARG_STR;
        m.args[1].v.s = "on";
        cmd_DSP(e, &m);
    }
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
    out->dup_redirects = e->dup_redirects;
    out->dup_played = e->dup_played;
    out->history_writes = e->history_writes;
    out->scrobble_due = e->scrobble_due;
    out->skip_excluded = e->skip_excluded;
    out->analyse_jobs = e->analyse_jobs;
    out->analyse_speed = e->analyse_speed;
    out->analyse_gated = e->analyse_gated;
    out->analyse_lufs_cdb = e->analyse_lufs_cdb;
    out->analyse_gain_cdb = e->analyse_gain_cdb;
    out->analyse_peak_cdb = e->analyse_peak_cdb;
    out->cue_sets = e->cue_sets;
    out->cue_gotos = e->cue_gotos;
    out->sticker_ops = e->sticker_ops;
    out->folder_ops = e->folder_ops;
    out->links_parsed = e->links_parsed;
    out->links_made = e->links_made;
    out->radio_appends = e->radio_appends;
    out->rating_pushes = sel_devilbox_pushes();
    out->rating_batches = sel_devilbox_batches();
    out->vote_posts = sel_devilbox_vote_posts();
    out->rating_pending = 0;
    if (e->lib != NULL) {
        char none[4];
        uint32_t rows = 0;

        if (sel_library_rating_pending(e->lib, none, sizeof none, &rows)) out->rating_pending = rows;
    }
    out->radio_min_ahead = e->radio_min_ahead;
    out->folder_view_rows = e->folder_view_rows;
    out->panel_loads = e->panel_loads;
    out->library_searches = e->searches;
    out->library_fts_queries = e->fts_queries;
    out->result_count = e->res_count;
    out->rp_config_applied = e->rp_config_applied;
    out->undo_depth = e->history.undo_count;
    out->redo_depth = e->history.redo_count;
    out->dsp_blocks = e->dsp.blocks;
    out->led_on = e->stage.led_on;
    out->led_frames = e->stage.led_frames;
    out->led_cutoff_hz = SEL_LED_CUTOFF_HZ;
    out->width_percent = e->stage.width;
    out->width_frames = e->stage.width_frames;
    out->width_mono = e->stage.width == 0u;
    {
        const sel_slot* ls = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
        out->loudness_source = ls != NULL && ls->loud_source != NULL ? ls->loud_source : "none";
        out->loudness_gain_q16 = ls != NULL && ls->loud_gain_q16 != 0u ? ls->loud_gain_q16 : (uint32_t)SEL_GAIN_UNITY;
    }
    out->limiter_hits = e->loud.hits;
    out->loudness_frames = e->loud.frames;
    out->leveler_frames = e->lev_frames;
    out->leveler_writes = e->lev_writes;
    out->xfade_frames = e->xfade_frames;
    out->slots_mixed = e->slots_mixed;
    out->mixed_path = e->mixed_path;
    out->mutes = e->mutes;
    out->fades = e->fades;
    out->crossfeed_on = (uint32_t)sel_dsp_chain_is_on(&e->dsp, "crossfeed");
    out->sleep_armed = e->timer.sleep_left_ms != 0u;
}

/* Closes one 20 ms block: reads and clears the mixer's per-voice taps, steps the
 * VU levels, copies the scopes oldest first and publishes the frame. */
static void viz_close(sel_engine* e, sel_slot* sl) {
    sel_viz_frame* f = sel_viz_begin(&e->viz, e->rate);
    sel_mixer* m = (sl != NULL && sl->rp_render == NULL) ? &sl->mixer : NULL;
    uint32_t count = m ? m->voice_count : 0;
    uint32_t k;
    if (count > SEL_VIZ_MAX_VOICES) count = SEL_VIZ_MAX_VOICES;
    f->voice_count = count;
    for (k = 0; k < SEL_VIZ_MAX_VOICES; k++) {
        int16_t peak = 0;
        if (k < count) {
            sel_mixer_voice* v = &m->voices[k];
            uint32_t head = m->viz_head & (SEL_MIXER_SCOPE - 1u);
            uint32_t first = SEL_MIXER_SCOPE - head;
            peak = (int16_t)v->viz_peak;
            v->viz_peak = 0;
            f->pitch_q8[k] = v->playing ? v->pitch_q8 : 0;
            f->note_on[k] = (uint8_t)(v->viz_kick != 0);
            v->viz_kick = 0;
            memcpy(f->scope[k], &m->viz_scope[k][head], (size_t)first * sizeof(int16_t));
            memcpy(&f->scope[k][first], m->viz_scope[k], (size_t)head * sizeof(int16_t));
        }
        e->viz.vu[k] = sel_viz_vu_step(e->viz.vu[k], peak);
        f->peak[k] = peak;
        f->vu[k] = e->viz.vu[k];
    }
    sel_viz_publish(&e->viz);
}

/* Feeds the rendered, volume-scaled output to the post-mix ring and closes a
 * block each time rate / 50 frames have gone by, playing or not. */
static void viz_feed(sel_engine* e, sel_slot* sl, const int16_t* stereo, uint32_t frames) {
    uint32_t off = 0;
    while (off < frames) {
        uint32_t n = sel_viz_block_left(&e->viz, e->rate);
        if (n > frames - off) n = frames - off;
        if (sel_viz_pcm_push(&e->viz, stereo + (size_t)off * 2u, n)) {
            viz_close(e, sl);
        }
        off += n;
    }
}

/* Mixes the open song: the scheduler turns each tick's length into frames,
 * the mixer fills them. The tick that reports ENDED plays out, then the
 * song is flagged ended for the next pump (events are produced on the
 * command side only, keeping the event queue single-producer). */
/* Applies the end policy to the n frames just mixed at buf (design section 3, loop
 * detect, fade and silence): silence detection on the raw samples, the max-length
 * trigger, then the linear fade in 64-frame gain steps. Returns the frames kept;
 * sets sl->ended and sl->pol.reason when the song ends inside the chunk. Audio thread. */
static uint32_t policy_chunk(sel_engine* e, sel_slot* sl, int16_t* buf, uint32_t n) {
    sel_playpol* p = &sl->pol;
    uint32_t before = sl->song_frames - n;
    uint32_t keep = n;
    uint32_t reason = SEL_END_NONE;
    uint32_t fade_at = p->fade_on ? 0u : n;
    uint32_t i;
    if (e->silence_ms != 0u) {
        uint32_t need = (uint32_t)((uint64_t)e->silence_ms * e->rate / 1000u);
        for (i = 0; i < n; i++) {
            int32_t l = buf[i * 2u], r = buf[i * 2u + 1u];
            if (l > -SEL_SILENCE_LEVEL && l < SEL_SILENCE_LEVEL && r > -SEL_SILENCE_LEVEL && r < SEL_SILENCE_LEVEL) {
                if (++p->silent >= need) {
                    keep = i + 1u;
                    reason = SEL_END_SILENCE;
                    break;
                }
            } else {
                p->silent = 0;
            }
        }
    }
    if (!p->fade_on && e->max_song_ms != 0u) {
        uint64_t max_frames = (uint64_t)e->max_song_ms * e->rate / 1000u;
        if ((uint64_t)before + n > max_frames) {
            uint32_t off = max_frames > before ? (uint32_t)(max_frames - before) : 0u;
            if (off < keep) {
                p->fade_on = 1;
                e->fades++;
                p->fade_pos = 0;
                p->fade_total = (uint32_t)((uint64_t)e->fade_ms * e->rate / 1000u);
                p->fade_reason = SEL_END_MAX_LENGTH;
                fade_at = off;
            }
        }
    }
    if (p->fade_on) {
        uint32_t gain = 0;
        for (i = fade_at; i < keep; i++) {
            if (p->fade_pos >= p->fade_total) {
                keep = i;
                reason = p->fade_reason;
                break;
            }
            if (i == fade_at || (p->fade_pos & 63u) == 0u) {
                gain = (uint32_t)(((uint64_t)(p->fade_total - p->fade_pos) << 16) / p->fade_total);
            }
            buf[i * 2u] = (int16_t)(((int32_t)buf[i * 2u] * (int32_t)gain) >> 16);
            buf[i * 2u + 1u] = (int16_t)(((int32_t)buf[i * 2u + 1u] * (int32_t)gain) >> 16);
            p->fade_pos++;
        }
        if (reason == SEL_END_NONE && p->fade_pos >= p->fade_total) {
            reason = p->fade_reason;
        }
    }
    if (reason != SEL_END_NONE) {
        p->reason = reason;
        sl->ended = 1;
    }
    sl->song_frames -= n - keep;
    return keep;
}

/* A LOOPED tick: counts the pass; the pass that reaches `loops` starts the fade
 * (or ends the song at once when fade_ms is 0). REPEAT one never ends on a loop. */
static void policy_loop(sel_engine* e, sel_slot* sl) {
    sel_playpol* p = &sl->pol;
    if (e->repeat == 1u || p->fade_on) {
        return;
    }
    p->loops_seen++;
    if (p->loops_seen < e->loops) {
        return;
    }
    if (e->fade_ms == 0u) {
        sl->last_tick = 1;
        p->reason = SEL_END_LOOP;
        return;
    }
    p->fade_on = 1;
    e->fades++;
    p->fade_pos = 0;
    p->fade_total = (uint32_t)((uint64_t)e->fade_ms * e->rate / 1000u);
    p->fade_reason = SEL_END_LOOP;
}

int sel_engine_fade(const sel_engine* e, uint32_t* pos, uint32_t* total) {
    const sel_slot* sl = e->audio_slot < SEL_SLOT_COUNT ? &e->slot[e->audio_slot] : NULL;
    if (sl == NULL || !sl->pol.fade_on) {
        return 0;
    }
    *pos = sl->pol.fade_pos;
    *total = sl->pol.fade_total;
    return 1;
}

/* Hands slot `idx` back to the pump (RP_CLOSE runs there). Audio thread. */
static void return_slot(sel_engine* e, uint32_t idx) {
    sel_message back;
    memset(&back, 0, sizeof back);
    back.kind = SEL_MSG_CMD;
    back.id = SEL_RING_RETURN;
    back.argc = 1;
    back.args[0].type = SEL_ARG_UINT;
    back.args[0].v.u = idx;
    sel_spsc_push(&e->ret_q, &back);
}

/* Renders up to `frames` frames of one song into stereo, with the end policy applied;
 * fewer frames mean the song ended. Audio thread. */
static uint32_t render_slot(sel_engine* e, sel_slot* sl, int16_t* stereo, uint32_t frames) {
    uint32_t done = 0;
    {
        uint32_t want_mute = e->state.mute_mask;
        if (sl->rp_render == NULL) {
            sl->mixer.mute_mask = want_mute;
        } else if (sl->rp_mute != NULL && sl->applied_mute != want_mute) {
            uint32_t k;
            for (k = 0; k < 32u; k++) {
                if (((sl->applied_mute ^ want_mute) >> k) & 1u) {
                    sl->rp_mute(sl->rp_state, k, (want_mute >> k) & 1u);
                }
            }
            sl->applied_mute = want_mute;
        }
        {
            uint32_t pq = e->state.pitch_q16 != 0u ? e->state.pitch_q16 : 65536u;
            uint32_t tq = (uint32_t)(((uint64_t)(100 + e->state.tempo_percent) << 16) / 100u);
            sl->mixer.pitch_q16 = pq;
            if (sl->rp_set_speed != NULL && (sl->applied_pitch != pq || sl->applied_tempo != tq)) {
                sl->rp_set_speed(sl->rp_state, pq, tq);
                sl->applied_pitch = pq;
                sl->applied_tempo = tq;
            }
        }
        while (sl->rp_render != NULL && done < frames && !sl->ended) {
            uint32_t want = frames - done;
            uint32_t got = sl->rp_render(sl->rp_state, stereo + (size_t)done * 2u, want);
            sl->rp_render_calls++;
            sl->rendered_frames += got;
            sl->song_frames += got;
            got = policy_chunk(e, sl, stereo + (size_t)done * 2u, got);
            done += got;
            sample_position(sl);
            if (got < want && !sl->ended) { /* short read: the song ended */
                sl->ended = 1;
                sl->pol.reason = SEL_END_END;
            }
        }
        while (sl->rp_render == NULL && done < frames && !sl->ended) {
            uint32_t n;
            if (sl->frames_left == 0) {
                sel_tick_result r;
                sl->rp_tick(sl->rp_state, &r);
                sl->ticks++;
                sl->frames_left = sel_scheduler_frames(&sl->sched, r.length);
                if (e->state.tempo_percent != 0) { /* TEMPO: tick rate is (100 + percent) percent */
                    sl->frames_left =
                        (uint32_t)(((uint64_t)sl->frames_left * 100u) / (uint32_t)(100 + e->state.tempo_percent));
                    if (sl->frames_left == 0u) {
                        sl->frames_left = 1u;
                    }
                }
                if (r.flags & SEL_TICK_ENDED) {
                    sl->last_tick = 1;
                    sl->pol.reason = SEL_END_END;
                } else if (r.flags & SEL_TICK_LOOPED) {
                    policy_loop(e, sl);
                }
            }
            n = frames - done < sl->frames_left ? frames - done : sl->frames_left;
            sel_mixer_mix(&sl->mixer, stereo + (size_t)done * 2u, n);
            sl->frames_left -= n;
            sl->song_frames += n;
            n = policy_chunk(e, sl, stereo + (size_t)done * 2u, n);
            done += n;
            if (!sl->ended && sl->frames_left == 0 && sl->last_tick) {
                sl->last_tick = 0;
                sl->ended = 1;
            }
        }
    }
    return done;
}

#define SEL_XF_BLOCK 512u

/* The outgoing song ended (or its crossfade finished): the prepared song takes over. */
static void switch_to_next(sel_engine* e) {
    e->xf_reason = e->slot[e->act].pol.reason != SEL_END_NONE ? e->slot[e->act].pol.reason : (uint32_t)SEL_END_END;
    return_slot(e, e->act);
    e->act = e->nx;
    e->nx = SEL_SLOT_NONE;
    e->xf_on = 0;
    e->audio_slot = e->act;
}

/* Mixes up to `frames` frames: the current song, then (gapless) the prepared song right
 * after it, and during a crossfade both with an equal-power Q16 gain pair (design
 * section 5). The outgoing song's own fade still multiplies, so the gain stays continuous. */
static uint32_t render_mix(sel_engine* e, int16_t* stereo, uint32_t frames) {
    static int16_t scratch[SEL_XF_BLOCK * 2u]; /* only the audio thread touches it */
    uint32_t done = 0;
    while (done < frames && e->act < SEL_SLOT_COUNT) {
        sel_slot* sl = &e->slot[e->act];
        uint32_t want = frames - done;
        int16_t* out = stereo + (size_t)done * 2u;
        uint32_t n;
        if (e->nx < SEL_SLOT_COUNT && !e->xf_on && e->crossfade_ms != 0u && sl->pol.fade_on) {
            uint32_t xf = (uint32_t)((uint64_t)e->crossfade_ms * e->rate / 1000u);
            uint32_t rem = sl->pol.fade_total - sl->pol.fade_pos;
            if (rem <= xf) {
                e->xf_on = 1;
                e->xf_pos = 0;
                e->xf_total = rem;
            } else if (rem - xf < want) {
                want = rem - xf; /* the crossfade starts exactly where the remaining fade equals it */
            }
        }
        if (e->xf_on) {
            sel_slot* in = &e->slot[e->nx];
            uint32_t i, m;
            if (want > SEL_XF_BLOCK) {
                want = SEL_XF_BLOCK;
            }
            if (want > e->xf_total - e->xf_pos) {
                want = e->xf_total - e->xf_pos;
            }
            n = render_slot(e, sl, out, want);
            m = render_slot(e, in, scratch, want);
            slot_gain(e, sl, out, n);
            slot_gain(e, in, scratch, m);
            for (i = n; i < want; i++) { /* a short outgoing song leaves silence under its tail */
                out[i * 2u] = 0;
                out[i * 2u + 1u] = 0;
            }
            for (i = m; i < want; i++) {
                scratch[i * 2u] = 0;
                scratch[i * 2u + 1u] = 0;
            }
            for (i = 0; i < want; i++) {
                uint32_t idx = (uint32_t)(((uint64_t)(e->xf_pos + i) * SEL_XFADE_STEPS) / e->xf_total);
                int32_t go = (int32_t)sel_xfade_out_q16[idx];
                int32_t gi = (int32_t)sel_xfade_out_q16[SEL_XFADE_STEPS - idx];
                out[i * 2u] = (int16_t)(((int32_t)out[i * 2u] * go + (int32_t)scratch[i * 2u] * gi) >> 16);
                out[i * 2u + 1u] = (int16_t)(((int32_t)out[i * 2u + 1u] * go + (int32_t)scratch[i * 2u + 1u] * gi) >> 16);
            }
            e->xf_pos += want;
            e->xfade_frames += want;
            e->slots_mixed = 2u;
            done += want;
            if (e->xf_pos >= e->xf_total || sl->ended) {
                switch_to_next(e);
            }
            continue;
        }
        n = render_slot(e, sl, out, want);
        slot_gain(e, sl, out, n);
        done += n;
        if (sl->ended && e->nx < SEL_SLOT_COUNT) {
            switch_to_next(e); /* gapless: the next song fills the rest of this block */
        } else if (n < want) {
            break;
        }
    }
    if (done != 0u && e->slots_mixed == 0u) {
        e->slots_mixed = 1u;
    }
    return done;
}

static void answer_pattern(sel_engine* e, const sel_slot* sl);

uint32_t sel_engine_render(sel_engine* e, int16_t* stereo, uint32_t frames) {
    uint32_t done = 0;
    sel_message sw;
    sel_slot* sl;
    SEL_THREAD_AUDIO(e);
    e->render_calls++;
    e->loud_mixed = 0;
    e->loud_ref = SEL_GAIN_UNITY;
    if (stereo == NULL || frames == 0) {
        return frames;
    }
    while (sel_spsc_pop(&e->swap_q, &sw)) {
        uint32_t flag = sw.args[1].v.u;
        if (flag == 1u) { /* keep as the next song */
            e->nx = sw.args[0].v.u;
        } else if (flag == 2u) { /* drop the next song */
            if (e->nx == sw.args[0].v.u) {
                return_slot(e, e->nx);
                e->nx = SEL_SLOT_NONE;
                e->xf_on = 0;
            }
        } else {
            if (e->act < SEL_SLOT_COUNT) {
                return_slot(e, e->act);
            }
            e->xf_on = 0;
            e->act = sw.args[0].v.u;
            e->audio_slot = e->act;
        }
    }
    sel_timer_poll(&e->timer, local_ms(e), e->state.play == SEL_PLAYING,
                   (uint32_t)((uint64_t)SEL_SLEEP_RAMP_MS * e->rate / 1000u));
    {
        sl = e->act < SEL_SLOT_COUNT ? &e->slot[e->act] : NULL;
        if (sl != NULL && e->state.play == SEL_PLAYING) {
            e->loud_ref = loud_reference(e, frames);
            done = render_mix(e, stereo, frames);
            sel_timer_apply(&e->timer, stereo, done);
            if (e->state.volume != 100) {
                uint32_t i;
                for (i = 0; i < done * 2u; i++) {
                    stereo[i] = (int16_t)(((int32_t)stereo[i] * e->state.volume) / 100);
                }
            }
        }
    }
    if (done < frames) {
        memset(stereo + (size_t)done * 2u, 0, (size_t)(frames - done) * 2u * sizeof(int16_t));
    }
    sel_mix_stage_process(&e->stage, stereo, frames);
    sel_dsp_chain_process(&e->dsp, stereo, frames);
    e->loud.active = e->loud_mixed;
    e->loud.gain_q16 = e->loud_ref;
    sel_loudness_gain_process(&e->loud, stereo, frames);
    viz_feed(e, sl, stereo, frames);
    if (e->pat_ack != e->pat_req) {
        answer_pattern(e, sl);
    }
    return frames;
}

uint32_t sel_engine_render_calls(const sel_engine* e) {
    return e->render_calls;
}

/* Audio thread, after a render call: answers an outstanding pattern window request from the
 * slot being mixed. The replayer is read here because the audio thread owns it. */
static void answer_pattern(sel_engine* e, const sel_slot* sl) {
    sel_pattern_view* v = &e->pat_res;
    uint32_t req = e->pat_req, rows = e->pat_rows, ch;
    sel_rp_cell col[SEL_PATTERN_ROWS];
    memset(v, 0, sizeof *v);
    v->order = e->pat_order;
    v->row_lo = e->pat_row_lo;
    if (sl != NULL && sl->rp_state != NULL && sl->rp_tracker_info != NULL && sl->rp_tracker_cells != NULL &&
        sl->rp_tracker_info(sl->rp_state, v->order, &v->info) && v->info.channels > 0u) {
        v->valid = 1;
        v->row_n = rows > SEL_PATTERN_ROWS ? SEL_PATTERN_ROWS : rows;
        v->channels = v->info.channels > SEL_PATTERN_CHANNELS ? SEL_PATTERN_CHANNELS : v->info.channels;
        for (ch = 0; ch < v->channels; ch++) {
            uint32_t n = sl->rp_tracker_cells(sl->rp_state, v->order, ch, v->row_lo, v->row_lo + v->row_n, col,
                                              v->row_n);
            uint32_t r;
            for (r = 0; r < n && r < v->row_n; r++) {
                v->cell[r][ch] = col[r];
            }
        }
    }
    v->seq = req;
    e->pat_ack = req;
}

uint32_t sel_engine_pattern_request(sel_engine* e, uint32_t order, uint32_t row_lo, uint32_t rows) {
    if (e->pat_ack != e->pat_req) {
        return 0;
    }
    e->pat_order = order;
    e->pat_row_lo = row_lo;
    e->pat_rows = rows;
    e->pat_req++;
    return e->pat_req;
}

/* Tracker pattern data of the open song. Calls the replayer on the caller's
 * thread, so the caller must be the thread that owns the replayer: the
 * script driver (one thread renders and pumps) today. A front end with a
 * separate audio thread needs the request path of M1.3 Phase 5. */
int sel_engine_tracker_info(const sel_engine* e, uint32_t order, sel_rp_tracker_info* out) {
    const sel_slot* sl = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    if (sl == NULL || sl->rp_state == NULL || sl->rp_tracker_info == NULL) {
        return 0;
    }
    return sl->rp_tracker_info(sl->rp_state, order, out);
}

uint32_t sel_engine_tracker_cells(const sel_engine* e, uint32_t order, uint32_t channel, uint32_t row_lo,
                                  uint32_t row_hi, sel_rp_cell* out, uint32_t cap) {
    const sel_slot* sl = e->cur == SEL_SLOT_NONE ? NULL : &e->slot[e->cur];
    if (sl == NULL || sl->rp_state == NULL || sl->rp_tracker_cells == NULL) {
        return 0;
    }
    return sl->rp_tracker_cells(sl->rp_state, order, channel, row_lo, row_hi, out, cap);
}

void sel_engine_sync_info(const sel_engine* e, sel_sync_info* out) {
    *out = e->sync_info;
}

void sel_engine_scan_info(const sel_engine* e, sel_scan_info* out) {
    *out = e->scan_info;
}
