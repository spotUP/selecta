/* Output plugin hooks, stored in the descriptor's PLUGIN_TAGS list under
 * the OUT_* tags (design doc section 10). The engine is the source: the
 * output pulls interleaved stereo int16 frames through sel_pcm_pull. */
#ifndef SELECTA_OUTPUT_H
#define SELECTA_OUTPUT_H

#include <selecta/plugin.h>

SEL_EXTERN_C_BEGIN

/* Fills `frames` stereo frames; returns frames written. */
typedef uint32_t (*sel_pcm_pull)(void* ctx, int16_t* stereo, uint32_t frames);

typedef void* (*sel_out_open_fn)(const sel_host* host, uint32_t rate, uint32_t channels); /* OUT_OPEN */
typedef void (*sel_out_close_fn)(void* state);                                          /* OUT_CLOSE */
typedef void (*sel_out_set_source_fn)(void* state, sel_pcm_pull fn, void* ctx);         /* OUT_SET_SOURCE */
/* OUT_PUMP: M0a-only hook for sinks with no clock of their own; pulls
 * `frames` frames now and returns how many it pulled. */
typedef uint32_t (*sel_out_pump_fn)(void* state, uint32_t frames);
typedef const sel_tag* (*sel_out_stats_fn)(void* state); /* OUT_STATS: OUT_STAT_* tags */
typedef const struct sel_voices* (*sel_out_voice_backend_fn)(void* state); /* OUT_VOICE_BACKEND */

/* OUT_RATE_FOR: the output rate the app should mix and open at on `machine` (a sel_machine tag list). */
typedef uint32_t (*sel_out_rate_for_fn)(const sel_tag* machine);

/* Output devices (I1039). OUT_DEVICES lists the devices an output can play to; the pref output.device holds the uid of the
 * one the user picked ("" = follow the system default). A listing is a snapshot: uid is stable across restarts, name is for
 * people, transport marks AirPlay. */
#define SEL_OUT_DEVICE_UID_MAX 96u
#define SEL_OUT_DEVICE_NAME_MAX 64u
#define SEL_OUT_TRANSPORT_OTHER 0u
#define SEL_OUT_TRANSPORT_BUILTIN 1u
#define SEL_OUT_TRANSPORT_USB 2u
#define SEL_OUT_TRANSPORT_BLUETOOTH 3u
#define SEL_OUT_TRANSPORT_HDMI 4u
#define SEL_OUT_TRANSPORT_AIRPLAY 5u

typedef struct sel_out_device {
    char uid[SEL_OUT_DEVICE_UID_MAX];
    char name[SEL_OUT_DEVICE_NAME_MAX];
    uint32_t transport; /* SEL_OUT_TRANSPORT_* */
    uint32_t is_default; /* 1 = the system default output right now */
} sel_out_device;

/* OUT_DEVICES: fills up to `max` entries, returns how many. Needs no open output. */
typedef uint32_t (*sel_out_devices_fn)(sel_out_device* out, uint32_t max);
/* OUT_LATENCY_QUERY: the open output's real latency in milliseconds (device + stream + safety offset + buffer). */
typedef uint32_t (*sel_out_latency_query_fn)(void* state);

SEL_EXTERN_C_END

#endif /* SELECTA_OUTPUT_H */
