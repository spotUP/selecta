/* RED stub (plan row I26): the real mixer arrives with I27. */
#include "mixer.h"

#include <string.h>

void sel_mixer_init(sel_mixer* m, uint32_t rate, uint32_t voice_count, uint32_t resampler) {
    memset(m, 0, sizeof *m);
    m->rate = rate;
    m->voice_count = voice_count;
    m->resampler = resampler;
}
void sel_mixer_mix(sel_mixer* m, int16_t* stereo, uint32_t frames) {
    (void)m;
    memset(stereo, 0x55, (size_t)frames * 4u);
}
uint32_t sel_mixer_period_step(uint32_t period, uint32_t rate) {
    (void)period;
    (void)rate;
    return 0;
}
uint32_t sel_mixer_hz_step(uint32_t hz_q8, uint32_t rate) {
    (void)hz_q8;
    (void)rate;
    return 0;
}
uint32_t sel_mixer_default_resampler(const sel_tag* machine) {
    (void)machine;
    return SEL_MIX_RESAMPLE_NONE;
}
