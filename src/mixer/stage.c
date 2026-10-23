/* LED filter and stereo width (stage.h). The filter is a 2-pole Butterworth
 * low-pass designed by the bilinear transform in Q28 integer arithmetic, so
 * any render rate gets its -3 dB point at SEL_LED_CUTOFF_HZ without floating
 * point on the 68k. */
#include "stage.h"

#include <string.h>

#define Q 28
#define ONE ((int64_t)1 << Q)
#define PI_Q28 843314857
#define SQRT2_Q28 379625062

static int16_t clip16(int32_t x) {
    return (int16_t)(x > 32767 ? 32767 : (x < -32768 ? -32768 : x));
}

/* tan(pi * hz / rate) in Q28 by Taylor series (sin / cos); rate > 2 * hz. */
static int64_t tan_q28(uint32_t hz, uint32_t rate) {
    int64_t x = ((int64_t)PI_Q28 * (int64_t)hz) / (int64_t)rate;
    int64_t x2 = (x * x) >> Q;
    int64_t st = x, sn = x, ct = ONE, cs = ONE;
    int k;
    for (k = 1; k <= 9; k++) {
        st = -((st * x2) >> Q) / ((int64_t)(2 * k) * (int64_t)(2 * k + 1));
        sn += st;
        ct = -((ct * x2) >> Q) / ((int64_t)(2 * k - 1) * (int64_t)(2 * k));
        cs += ct;
    }
    return (sn << Q) / cs;
}

void sel_mix_stage_init(sel_mix_stage* s, uint32_t rate) {
    int64_t k, k2, den, norm;
    memset(s, 0, sizeof *s);
    s->rate = rate;
    s->width = SEL_WIDTH_FULL;
    if (rate <= 2u * SEL_LED_CUTOFF_HZ) {
        return; /* no room below Nyquist: the filter stays an identity pass */
    }
    k = tan_q28(SEL_LED_CUTOFF_HZ, rate);
    k2 = (k * k) >> Q;
    den = ONE + ((k * SQRT2_Q28) >> Q) + k2;
    norm = (ONE << Q) / den;
    s->a0 = (int32_t)((k2 * norm) >> Q);
    s->b1 = (int32_t)(((2 * (k2 - ONE)) * norm) >> Q);
    s->b2 = (int32_t)((((ONE - ((k * SQRT2_Q28) >> Q)) + k2) * norm) >> Q);
}

void sel_mix_stage_set_led(sel_mix_stage* s, uint32_t on) {
    s->led_on = on != 0u;
    memset(s->x1, 0, sizeof s->x1);
    memset(s->x2, 0, sizeof s->x2);
    memset(s->y1, 0, sizeof s->y1);
    memset(s->y2, 0, sizeof s->y2);
}

void sel_mix_stage_set_width(sel_mix_stage* s, uint32_t percent) {
    s->width = percent > SEL_WIDTH_FULL ? SEL_WIDTH_FULL : percent;
}

void sel_mix_stage_process(sel_mix_stage* s, int16_t* stereo, uint32_t frames) {
    uint32_t i;
    if (s->paula_output || frames == 0u) {
        return;
    }
    if (s->led_on && s->a0 != 0) {
        for (i = 0; i < frames; i++) {
            uint32_t c;
            for (c = 0; c < 2u; c++) {
                int32_t x0 = stereo[i * 2u + c];
                int64_t acc = (int64_t)s->a0 * ((int64_t)x0 + 2 * (int64_t)s->x1[c] + (int64_t)s->x2[c]) -
                              (int64_t)s->b1 * s->y1[c] - (int64_t)s->b2 * s->y2[c];
                int32_t y = (int32_t)((acc + (ONE >> 1)) >> Q);
                if (y > 32767) y = 32767;
                if (y < -32768) y = -32768;
                s->x2[c] = s->x1[c];
                s->x1[c] = x0;
                s->y2[c] = s->y1[c];
                s->y1[c] = y;
                stereo[i * 2u + c] = (int16_t)y;
            }
        }
        s->led_frames += frames;
    }
    if (s->width < SEL_WIDTH_FULL) {
        for (i = 0; i < frames; i++) {
            int32_t l = stereo[i * 2u], r = stereo[i * 2u + 1u];
            int32_t mid = l + r;
            int32_t side = ((l - r) * (int32_t)s->width) / (int32_t)SEL_WIDTH_FULL;
            stereo[i * 2u] = clip16((mid + side) >> 1);
            stereo[i * 2u + 1u] = clip16((mid - side) >> 1);
        }
        s->width_frames += frames;
    }
}
