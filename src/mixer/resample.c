/* Resamplers (design doc section 5). All integer. Taps outside the sample
 * wrap into the loop, read silence after a one-shot's end and clamp to
 * frame 0 before the start. */
#include "mixer.h"

#include "sinc_table.h"

static int32_t tap(const sel_sample* s, uint32_t i) {
    uint32_t end = s->loop_length != 0 ? s->loop_start + s->loop_length : s->length;
    if (i >= end) {
        if (s->loop_length == 0) return 0;
        i = s->loop_start + (i - end) % s->loop_length;
    }
    if (s->format == SEL_SAMPLE_S16) return ((const int16_t*)s->data)[i];
    return (int32_t)((const int8_t*)s->data)[i] * 256;
}

/* Frame i + d with d in -3..4, clamped at frame 0. */
static int32_t tap_rel(const sel_sample* s, uint32_t pos, int32_t d) {
    if (d < 0 && pos < (uint32_t)-d) return tap(s, 0);
    return tap(s, (uint32_t)((int32_t)pos + d));
}

int32_t sel_resample(const sel_sample* s, uint32_t pos, uint32_t frac, uint32_t resampler) {
    int32_t t = (int32_t)(frac >> 1); /* Q15 fraction */
    switch (resampler) {
    case SEL_MIX_RESAMPLE_LINEAR: {
        int32_t a = tap(s, pos);
        int32_t b = tap_rel(s, pos, 1);
        return a + (((b - a) * t) >> 15);
    }
    case SEL_MIX_RESAMPLE_CUBIC: {
        /* Catmull-Rom, Q15 weights, each sum 32768. */
        int32_t t2 = (t * t) >> 15;
        int32_t t3 = (t2 * t) >> 15;
        int32_t w0 = (-t3 + 2 * t2 - t) / 2;
        int32_t w1 = (3 * t3 - 5 * t2 + 2 * 32768) / 2;
        int32_t w2 = (-3 * t3 + 4 * t2 + t) / 2;
        int32_t w3 = (t3 - t2) / 2;
        return (w0 * tap_rel(s, pos, -1) + w1 * tap(s, pos) + w2 * tap_rel(s, pos, 1) + w3 * tap_rel(s, pos, 2)) >> 15;
    }
    case SEL_MIX_RESAMPLE_SINC: {
        const int32_t* row = sel_resample_sinc_row(frac >> 8);
        int32_t sum = 0;
        int32_t k;
        for (k = 0; k < 8; k++) sum += row[k] * tap_rel(s, pos, k - 3);
        return sum >> 15;
    }
    default:
        return tap(s, pos);
    }
}

const int32_t* sel_resample_sinc_row(uint32_t phase) {
    return sel_sinc_table[phase & 255u];
}
