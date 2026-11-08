/* I292b: spectrum analysis over the whole ring and peak falloff. Modes: whole falloff. Host test, may use float to build input. */
#include <selecta/viz.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__); \
            printf("\n"); \
            failures++; \
        } \
    } while (0)

#define RATE 44100u
#define BLOCK 882u

static const sel_viz_frame* run_block(sel_viz_tap* t, const int16_t* stereo, int analyse) {
    uint32_t left = sel_viz_block_left(t, RATE);
    if (sel_viz_pcm_push(t, stereo, left)) {
        sel_viz_begin(t, RATE);
        sel_viz_publish(t);
    }
    if (analyse) {
        sel_viz_acquire(t);
        sel_viz_analyse(t, 1);
    }
    return sel_viz_current(t);
}

/* One block of a 1 kHz tone starting at sample `pos`; period > 0 keeps it on for the first `gate` frames of each period. */
static void fill(int16_t* buf, uint32_t pos, uint32_t period, uint32_t gate, int silent) {
    uint32_t i;
    for (i = 0; i < BLOCK; i++) {
        uint32_t p = pos + i;
        int on = !silent && (period == 0 || p % period < gate);
        buf[i * 2] = buf[i * 2 + 1] = on ? (int16_t)lrint(30000.0 * sin(2.0 * 3.14159265358979 * 1000.0 * p / 44100.0)) : 0;
    }
}

static int jump_max(sel_viz_tap* t, uint32_t period, uint32_t gate) {
    static int16_t buf[BLOCK * 2u];
    int16_t prev[SEL_VIZ_BANDS];
    int worst = 0;
    uint32_t b, i;
    memset(prev, 0, sizeof prev);
    sel_viz_tap_init(t);
    for (b = 0; b < 40u; b++) {
        const sel_viz_frame* f;
        fill(buf, b * BLOCK, period, gate, 0);
        f = run_block(t, buf, 1);
        if (b >= 6u) {
            for (i = 0; i < SEL_VIZ_BANDS; i++) {
                int d = abs((int)f->band[i] - (int)prev[i]);
                if (d > worst) worst = d;
            }
        }
        memcpy(prev, f->band, sizeof prev);
    }
    return worst;
}

static void test_whole(void) {
    static sel_viz_tap t;
    static int16_t ring[SEL_VIZ_PCM_FRAMES * 2u];
    int16_t band[SEL_VIZ_BANDS];
    uint32_t i;
    int steady, gated, peak = 0;
    /* A constant tone is the same block after block. */
    steady = jump_max(&t, 0, 0);
    CHECK(steady < 1500, "constant tone: bars move by %d between blocks, want < 1500", steady);
    /* A tone gated on for 100 of every 600 frames: whichever 256 frames sit at the newest end, the bars must not follow. */
    gated = jump_max(&t, 600, 100);
    CHECK(gated < 4000, "gated tone: bars jump by %d between blocks, want < 4000 (one window follows the gate)", gated);
    /* A tone only in the first part of the ring (the oldest 400 frames) still shows. */
    memset(ring, 0, sizeof ring);
    for (i = 0; i < 400u; i++) {
        ring[i * 2] = ring[i * 2 + 1] = (int16_t)lrint(30000.0 * sin(2.0 * 3.14159265358979 * 1000.0 * i / 44100.0));
    }
    sel_viz_spectrum(ring, SEL_VIZ_PCM_FRAMES, RATE, band);
    for (i = 0; i < SEL_VIZ_BANDS; i++) {
        if (band[i] > peak) peak = band[i];
    }
    CHECK(peak > 3000 && band[8] == peak, "tone in the first part of the block: peak %d in band 8 (%d), want > 3000", peak, band[8]);
}

static void test_falloff(void) {
    int16_t level[2] = {20000, 0}, raw[2] = {0, 30000};
    int16_t a[1] = {30000}, b[1] = {30000}, zero[1] = {0};
    static sel_viz_tap t1, t2;
    static int16_t buf[BLOCK * 2u];
    uint32_t k;
    int p1 = 0, v1 = 0, v2 = 0;
    /* pure: rise is instant, fall is SEL_VIZ_FALL_PER_SECOND * dt */
    sel_viz_falloff(level, raw, 2, 20);
    CHECK(level[0] == 20000 - (int)(SEL_VIZ_FALL_PER_SECOND / 50u), "fall in 20 ms: %d", level[0]);
    CHECK(level[1] == 30000, "rise not instant: %d", level[1]);
    sel_viz_falloff(a, zero, 1, 600);
    CHECK(a[0] == 30000 - (int)(SEL_VIZ_FALL_PER_SECOND * 6u / 10u), "fall in 600 ms: %d", a[0]);
    sel_viz_falloff(a, zero, 1, 5000);
    CHECK(a[0] == 0, "falls below zero: %d", a[0]);
    /* frame-rate independent: 600 ms as 30 ticks of 20 ms and as 6 ticks of 100 ms */
    a[0] = 30000;
    for (k = 0; k < 30u; k++) sel_viz_falloff(a, zero, 1, 20);
    for (k = 0; k < 6u; k++) sel_viz_falloff(b, zero, 1, 100);
    CHECK(a[0] == b[0], "20 ms ticks end at %d, 100 ms ticks at %d", a[0], b[0]);
    /* through the tap: tap 1 analyses every block, tap 2 every 5th; 5 tone blocks then 10 silent ones */
    sel_viz_tap_init(&t1);
    sel_viz_tap_init(&t2);
    for (k = 1; k <= 15u; k++) {
        const sel_viz_frame* f;
        fill(buf, k * BLOCK, 0, 0, k > 5u);
        f = run_block(&t1, buf, 1);
        if (k == 1u) CHECK(f->band[8] > 10000, "first tone block: band %d, the rise is not instant", f->band[8]);
        if (k == 5u) p1 = f->band[8];
        if (k == 15u) v1 = f->band[8];
        f = run_block(&t2, buf, k % 5u == 0);
        if (k == 15u) {
            v2 = f->band[8];
            CHECK(f->falloff_runs == 2u, "tap 2 falloff ran %u times, want 2 (3 analyses, the first only seeds)", (unsigned)f->falloff_runs);
        }
    }
    CHECK(p1 > 15000, "tone level %d too low for the test", p1);
    CHECK(v1 > 0 && v1 < p1 - 2000, "10 silent blocks: band %d from peak %d, want a smooth fall", v1, p1);
    CHECK(abs(v1 - v2) <= 700, "every block %d vs every 5th block %d: the fall depends on the tick rate", v1, v2);
}

int main(int argc, char** argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "whole") == 0) test_whole();
        else if (strcmp(argv[i], "falloff") == 0) test_falloff();
        else {
            printf("unknown mode %s\n", argv[i]);
            return 2;
        }
    }
    if (failures) return 1;
    printf("ok\n");
    return 0;
}
