/* Replayer plugin hooks, stored in the descriptor's PLUGIN_TAGS list under
 * the RP_* tags (design doc section 4). A VOICE replayer owns no clock: the
 * engine scheduler calls tick, the replayer drives sel_voices, the length it
 * returns is in the replayer's own timebase. No float crosses this
 * boundary. */
#ifndef SELECTA_REPLAYER_H
#define SELECTA_REPLAYER_H

#include <selecta/voices.h>

SEL_EXTERN_C_BEGIN

enum sel_rp_kind { SEL_RP_KIND_VOICE = 1, SEL_RP_KIND_RENDER = 2, SEL_RP_KIND_HW = 3 };

enum sel_probe_result { SEL_PROBE_NO = 0, SEL_PROBE_YES = 1, SEL_PROBE_UNSURE = 2 };

/* sel_tick_result.flags */
enum { SEL_TICK_LOOPED = 1, SEL_TICK_ENDED = 2 };

/* The whole song in memory. `data` stays valid until RP_CLOSE returns. */
typedef struct sel_song_source {
    const uint8_t* data;
    size_t size;
    const char* path; /* may be NULL */
} sel_song_source;

typedef struct sel_rp_info {
    uint32_t voice_count;
    uint32_t subsong_count;
    uint32_t default_subsong;
    uint32_t timebase_hz; /* unit of sel_tick_result.length */
} sel_rp_info;

typedef struct sel_tick_result {
    uint32_t length; /* until the next tick, in timebase units */
    uint32_t flags;  /* SEL_TICK_* */
} sel_tick_result;

typedef int (*sel_rp_probe_fn)(const uint8_t* data, size_t size, const char* name); /* RP_PROBE */
/* RP_OPEN: returns the replayer state or NULL; fills `info`. */
typedef void* (*sel_rp_open_fn)(const sel_song_source* src, const sel_host* host, sel_rp_info* info);
typedef void (*sel_rp_close_fn)(void* state);                                       /* RP_CLOSE */
/* RP_START: returns 1 on success. `voices` stays valid until close. */
typedef int (*sel_rp_start_fn)(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices);
typedef void (*sel_rp_tick_fn)(void* state, sel_tick_result* out);                  /* RP_TICK */
/* RP_METADATA: SEL_META_* tags, valid until close. */
typedef const sel_tag* (*sel_rp_metadata_fn)(void* state);

/* RP_RENDER (RENDER kind only; RP_START gets voices = NULL): interleaved
 * stereo int16 at the rate given to RP_START; returns frames written, less
 * than `frames` only at song end. */
typedef uint32_t (*sel_rp_render_fn)(void* state, int16_t* out, uint32_t frames);
/* RP_SEEK: optional. Returns 1 when the replayer seeked natively. */
typedef int (*sel_rp_seek_fn)(void* state, uint32_t ms);
/* RP_POSITION: optional. Fills order/row/pattern; 0 = not reported. */
typedef struct sel_rp_position {
    uint32_t order, row, pattern;
} sel_rp_position;
typedef int (*sel_rp_position_fn)(void* state, sel_rp_position* out);

/* RP_TRACKER_INFO: optional. The pattern layout for one order. `orders` is
 * the length of the order list, `patterns` the number of distinct patterns
 * (0 = the replayer does not know), `rows` the rows of the pattern played at
 * `order`, `channels` its columns. Returns 1 when `order` exists. */
typedef struct sel_rp_tracker_info {
    uint32_t orders, patterns, rows, channels;
} sel_rp_tracker_info;
typedef int (*sel_rp_tracker_info_fn)(void* state, uint32_t order, sel_rp_tracker_info* out);

/* One pattern cell. note: one scale for every adapter, libopenmpt's: 0 = none,
 * 1..120 with C-0 = 1, so a MOD period of 428 is note 61 (C-5) whichever
 * adapter plays it; an adapter whose engine counts differently converts. 254 = note cut, 255 = note off; instrument 0 = none; volume the volume column (0 = none);
 * effect and param as the format stores them (0, 0 = none). */
typedef struct sel_rp_cell {
    uint8_t note, instrument, volume, effect, param;
} sel_rp_cell;

/* RP_TRACKER_CELLS: optional. Writes the cells of `channel` for rows
 * row_lo .. row_hi - 1 of the pattern at `order` into `out` (at most `cap`),
 * in row order; returns how many. A row outside the pattern reads as an empty
 * cell. Like RP_POSITION it runs on the thread that owns the replayer
 * instance (the audio thread between render calls, or the pump thread while
 * the slot is unpublished). */
typedef uint32_t (*sel_rp_tracker_cells_fn)(void* state, uint32_t order, uint32_t channel, uint32_t row_lo,
                                            uint32_t row_hi, sel_rp_cell* out, uint32_t cap);

/* RP_MUTE: optional, RENDER kind only. Mutes (`muted` 1) or unmutes (0) the
 * replayer's channel `channel` (0-based). A RENDER replayer mixes itself, so
 * the engine cannot silence a voice in its mixer; this hook is the only way.
 * VOICE-kind replayers need none: the engine mixer skips muted voices. Runs on
 * the audio thread between render calls. */
typedef void (*sel_rp_mute_fn)(void* state, uint32_t channel, uint32_t muted);

/* RP_SET_SPEED: optional, RENDER kind only. Sets the replayer's pitch factor
 * and tempo factor (both Q16, 65536 = 1.0) for PITCH and TEMPO. VOICE-kind
 * replayers need none: the engine scales the mixer step and the tick interval.
 * Runs on the audio thread between render calls. */
typedef void (*sel_rp_set_speed_fn)(void* state, uint32_t pitch_q16, uint32_t tempo_q16);

/* One sample of the open song (RP_SAMPLE_GET). `data` points at `length` frames
 * of `channels` interleaved signed PCM, 8-bit (`bits` 8) or 16-bit native
 * endian (`bits` 16), owned by the replayer and valid until the song closes.
 * loop_length 0 = no loop. `rate` is the base rate in Hz (8287 for MOD-family
 * samples, the PAL C-3 rate). `count` is the number of sample slots, numbered
 * 1..count. */
typedef struct sel_rp_sample {
    const void* data;
    uint32_t length, loop_start, loop_length, rate, bits, channels, count;
    const char* name; /* replayer-owned, valid while the song is open (events keep the pointer) */
} sel_rp_sample;

/* RP_SAMPLE_GET: optional. Fills `out` for sample slot `n` (1-based) and
 * returns 1, or returns 0 when the slot is empty or beyond `count`; `out->count`
 * is set on every call. Runs on the thread that owns the replayer instance, like
 * RP_TRACKER_CELLS. */
typedef int (*sel_rp_sample_get_fn)(void* state, uint32_t n, sel_rp_sample* out);

SEL_EXTERN_C_END

#endif /* SELECTA_REPLAYER_H */
