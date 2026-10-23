/* DSP plugin ABI (design doc section 5, plan M1.3 I99). A DSP plugin works in
 * place on interleaved 16-bit stereo frames, after the mixer and before the
 * output. Plugin hooks (tags DSP_*) are listed under SEL_PLUGIN_TAGS of a
 * descriptor of kind SEL_PLUGIN_KIND_DSP. No float anywhere: Q15 integers.
 *
 * The engine holds a fixed chain of SEL_DSP_CHAIN_SLOTS slots. It calls
 * process on the audio thread (no allocation, no blocking); open, param and
 * close run on the pump thread. */
#ifndef SELECTA_DSP_H
#define SELECTA_DSP_H

#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/tags.h>

#include <stdint.h>

#define SEL_DSP_CHAIN_SLOTS 4u

/* Returns the plugin's state, or NULL when it cannot run at this rate. */
typedef void* (*sel_dsp_open_fn)(const sel_host* host, uint32_t rate, uint32_t channels);
/* Processes `frames` interleaved stereo frames in place. */
typedef void (*sel_dsp_process_fn)(void* state, int16_t* stereo, uint32_t frames);
typedef void (*sel_dsp_close_fn)(void* state);
/* Sets one plugin parameter; the ids are plugin specific. */
typedef void (*sel_dsp_param_fn)(void* state, uint32_t id, int32_t value);

/* Crossfeed parameters, in Q15 where a gain: 1 delay in frames (14 at 44100 Hz),
 * 2 low-pass one-pole coefficient a, 3 bleed gain, 4 direct gain. */
enum {
    SEL_DSP_CROSSFEED_P_DELAY = 1,
    SEL_DSP_CROSSFEED_P_LOWPASS = 2,
    SEL_DSP_CROSSFEED_P_BLEED = 3,
    SEL_DSP_CROSSFEED_P_DIRECT = 4
};
#define SEL_DSP_CROSSFEED_MAX_DELAY 63

/* Coefficients for one sample rate, generated at build time by
 * tools/gen_dsp_coef.c (src/engine/dsp_chain.c holds the table). */
typedef struct sel_dsp_crossfeed_coef {
    int32_t rate;
    int32_t delay;
    int32_t lowpass;
    int32_t bleed;
    int32_t direct;
} sel_dsp_crossfeed_coef;

/* Fills `out` for `rate` (the nearest table rate when there is no exact row).
 * Returns 0 only when `rate` is 0. */
int sel_dsp_crossfeed_coef_get(uint32_t rate, sel_dsp_crossfeed_coef* out);
/* Hands every coefficient to a plugin's param hook. */
void sel_dsp_crossfeed_apply(sel_dsp_param_fn param, void* state, const sel_dsp_crossfeed_coef* c);

#endif
