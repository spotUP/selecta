/* Output stages of the mixer (plan M2 Phase 5, I127 and I128): the Amiga LED
 * filter and the stereo width. Integer only, no allocation. The stages model
 * Paula's analogue side, so they run only when the output is not real Paula:
 * `paula_output` is the flag the Paula output sets (I-row for plugins/outputs/paula
 * wires it through sel_engine_set_paula_output); while it is 1 both stages are
 * bypassed and their counters stay put. */
#ifndef SELECTA_MIXER_STAGE_H
#define SELECTA_MIXER_STAGE_H

#include <stdint.h>

#include <selecta/voices.h>

SEL_EXTERN_C_BEGIN

#define SEL_LED_CUTOFF_HZ 3275u /* -3 dB point of the A500 LED filter */
#define SEL_WIDTH_FULL 100u     /* width percent that leaves the input unchanged */

typedef struct sel_mix_stage {
    uint32_t rate;
    uint32_t led_on;
    uint32_t width;        /* percent, 0 (mono) to SEL_WIDTH_FULL */
    uint32_t paula_output; /* 1: real Paula output, both stages bypassed */
    int32_t a0, b1, b2;    /* low-pass biquad, Q28; a1 = 2 * a0, a2 = a0 */
    int32_t x1[2], x2[2], y1[2], y2[2]; /* per channel history */
    uint32_t led_frames;   /* frames that ran through the LED filter */
    uint32_t width_frames; /* frames that ran through the width stage */
} sel_mix_stage;

/* Zeroes the stage, designs the filter for `rate`, width = SEL_WIDTH_FULL, LED off. */
void sel_mix_stage_init(sel_mix_stage* s, uint32_t rate);
void sel_mix_stage_set_led(sel_mix_stage* s, uint32_t on);
void sel_mix_stage_set_width(sel_mix_stage* s, uint32_t percent);
/* Processes `frames` interleaved L/R frames in place; counts only active stages. */
void sel_mix_stage_process(sel_mix_stage* s, int16_t* stereo, uint32_t frames);

SEL_EXTERN_C_END

#endif /* SELECTA_MIXER_STAGE_H */
