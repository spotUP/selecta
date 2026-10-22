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

SEL_EXTERN_C_END

#endif /* SELECTA_REPLAYER_H */
