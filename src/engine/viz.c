/* Stub: the bodies arrive with I86 and I87 (red phase, Gate 2). */
#include <selecta/viz.h>

#include <string.h>

void sel_viz_tap_init(sel_viz_tap* t) {
    memset(t, 0, sizeof *t);
}
uint32_t sel_viz_block_left(sel_viz_tap* t, uint32_t rate) {
    (void)t;
    (void)rate;
    return 1;
}
int sel_viz_pcm_push(sel_viz_tap* t, const int16_t* stereo, uint32_t frames) {
    (void)t;
    (void)stereo;
    (void)frames;
    return 1;
}
sel_viz_frame* sel_viz_begin(sel_viz_tap* t, uint32_t rate) {
    (void)rate;
    return &t->frames[0];
}
void sel_viz_publish(sel_viz_tap* t) {
    (void)t;
}
int16_t sel_viz_vu_step(int16_t level, int16_t peak) {
    (void)level;
    (void)peak;
    return 0;
}
int sel_viz_acquire(sel_viz_tap* t) {
    (void)t;
    return 0;
}
const sel_viz_frame* sel_viz_current(const sel_viz_tap* t) {
    return &t->frames[0];
}
void sel_viz_analyse(sel_viz_tap* t, int with_spectrum) {
    (void)t;
    (void)with_spectrum;
}
int32_t sel_viz_correlation(const int16_t* stereo, uint32_t frames) {
    (void)stereo;
    (void)frames;
    return 0;
}
void sel_viz_spectrum(const int16_t* stereo, uint32_t frames, uint32_t rate, int16_t band[SEL_VIZ_BANDS]) {
    (void)stereo;
    (void)frames;
    (void)rate;
    memset(band, 0, SEL_VIZ_BANDS * sizeof band[0]);
}
