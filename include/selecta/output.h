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

SEL_EXTERN_C_END

#endif /* SELECTA_OUTPUT_H */
