/* RED stub (plan row I26): the real resamplers arrive with I27 and I28. */
#include "mixer.h"

static const int32_t zero_row[8] = {0};

int32_t sel_resample(const sel_sample* s, uint32_t pos, uint32_t frac, uint32_t resampler) {
    (void)s;
    (void)pos;
    (void)frac;
    (void)resampler;
    return 0;
}
const int32_t* sel_resample_sinc_row(uint32_t phase) {
    (void)phase;
    return zero_row;
}
