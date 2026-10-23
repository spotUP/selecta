/* T48 (phase-tap) and the viz unit checks of T40: phase ring and correlation,
 * VU ballistics, spectrum bands. Modes on the command line: phase vu spectrum.
 * Host test: it may use float to build its input, the code under test may not. */
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

static uint32_t lcg_state = 12345u;
static int16_t noise(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return (int16_t)((int32_t)(lcg_state >> 16) - 32768) / 2;
}

/* Pushes `frames` stereo frames, closes the block, publishes, acquires and analyses. */
static const sel_viz_frame* run_block(sel_viz_tap* t, const int16_t* stereo, uint32_t frames, uint32_t rate) {
    uint32_t left = sel_viz_block_left(t, rate);
    int done = 0;
    uint32_t off = 0;
    while (off < frames) {
        uint32_t n = frames - off < left ? frames - off : left;
        done = sel_viz_pcm_push(t, stereo + (size_t)off * 2u, n);
        off += n;
        if (done) {
            sel_viz_begin(t, rate);
            sel_viz_publish(t);
            left = sel_viz_block_left(t, rate);
        } else {
            left -= n;
        }
    }
    sel_viz_acquire(t);
    sel_viz_analyse(t, 1);
    return sel_viz_current(t);
}

static void test_phase(void) {
    static sel_viz_tap tap;
    static int16_t buf[SEL_VIZ_PCM_FRAMES * 2u];
    const sel_viz_frame* f;
    uint32_t i;
    int diag = 1, anti = 1;
    /* mono: points on the diagonal */
    sel_viz_tap_init(&tap);
    for (i = 0; i < SEL_VIZ_PCM_FRAMES; i++) buf[i * 2] = buf[i * 2 + 1] = noise();
    f = run_block(&tap, buf, SEL_VIZ_PCM_FRAMES, 44100);
    CHECK(f->seq >= 1, "no frame published (seq %u)", (unsigned)f->seq);
    for (i = 0; i < SEL_VIZ_PCM_FRAMES; i++) diag &= f->pcm[i * 2] == f->pcm[i * 2 + 1] && f->pcm[i * 2] == buf[i * 2];
    CHECK(diag, "mono ring is not the diagonal L=R of the input");
    CHECK(f->correlation == 32767, "mono correlation %d, want 32767", f->correlation);
    /* inverted right channel: anti-diagonal */
    sel_viz_tap_init(&tap);
    for (i = 0; i < SEL_VIZ_PCM_FRAMES; i++) {
        buf[i * 2] = noise();
        buf[i * 2 + 1] = (int16_t)-buf[i * 2];
    }
    f = run_block(&tap, buf, SEL_VIZ_PCM_FRAMES, 44100);
    for (i = 0; i < SEL_VIZ_PCM_FRAMES; i++) anti &= f->pcm[i * 2 + 1] == -f->pcm[i * 2] && f->pcm[i * 2] == buf[i * 2];
    CHECK(anti, "inverted ring is not the anti-diagonal L=-R");
    CHECK(f->correlation == -32767, "inverted correlation %d, want -32767", f->correlation);
    /* uncorrelated noise */
    sel_viz_tap_init(&tap);
    for (i = 0; i < SEL_VIZ_PCM_FRAMES * 2u; i++) buf[i] = noise();
    f = run_block(&tap, buf, SEL_VIZ_PCM_FRAMES, 44100);
    CHECK(abs(f->correlation) < 2000, "uncorrelated correlation %d, want |c| < 2000", f->correlation);
    /* silence */
    memset(buf, 0, sizeof buf);
    sel_viz_tap_init(&tap);
    f = run_block(&tap, buf, SEL_VIZ_PCM_FRAMES, 44100);
    CHECK(f->correlation == 0, "silence correlation %d, want 0", f->correlation);
    CHECK(sel_viz_correlation(buf, SEL_VIZ_PCM_FRAMES) == 0, "pure function: silence");
}

static void test_vu(void) {
    int16_t level = 0;
    uint32_t n = 0;
    level = sel_viz_vu_step(level, 20000);
    CHECK(level == 20000, "attack is instant: %d", level);
    level = sel_viz_vu_step(level, 0);
    CHECK(level == 20000 - 20000 / 8, "release is level - level/8: %d", level);
    level = sel_viz_vu_step(level, 30000);
    CHECK(level == 30000, "attack over a falling level: %d", level);
    CHECK(sel_viz_vu_step(8, 0) == 7, "8 falls by 1");
    CHECK(sel_viz_vu_step(1, 0) == 0, "1 falls to 0");
    CHECK(sel_viz_vu_step(0, 0) == 0, "0 holds 0");
    while (level > 0 && n < 1000) {
        level = sel_viz_vu_step(level, 0);
        n++;
    }
    CHECK(level == 0 && n < 120, "decays to 0 in %u blocks", (unsigned)n);
}

static int argmax(const int16_t* b) {
    int i, best = 0;
    for (i = 1; i < (int)SEL_VIZ_BANDS; i++) {
        if (b[i] > b[best]) best = i;
    }
    return best;
}

static void test_spectrum(void) {
    static int16_t buf[SEL_VIZ_PCM_FRAMES * 2u];
    int16_t band[SEL_VIZ_BANDS];
    static const struct {
        double hz;
        int want;
    } cases[] = {{1000.0, 8}, {4000.0, 12}};
    uint32_t c, i;
    for (c = 0; c < 2; c++) {
        for (i = 0; i < SEL_VIZ_PCM_FRAMES; i++) {
            buf[i * 2] = buf[i * 2 + 1] = (int16_t)lrint(30000.0 * sin(2.0 * 3.14159265358979 * cases[c].hz * i / 44100.0));
        }
        sel_viz_spectrum(buf, SEL_VIZ_PCM_FRAMES, 44100, band);
        CHECK(argmax(band) == cases[c].want, "%.0f Hz peaks in band %d, want %d", cases[c].hz, argmax(band), cases[c].want);
        CHECK(band[cases[c].want] > 16000, "%.0f Hz band level %d too low", cases[c].hz, band[cases[c].want]);
        CHECK(band[(cases[c].want + 8) % 16] < band[cases[c].want] / 8, "far band not quiet");
    }
    memset(buf, 0, sizeof buf);
    sel_viz_spectrum(buf, SEL_VIZ_PCM_FRAMES, 44100, band);
    for (i = 0; i < SEL_VIZ_BANDS; i++) CHECK(band[i] == 0, "silence band %u = %d", (unsigned)i, band[i]);
}

int main(int argc, char** argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "phase") == 0) test_phase();
        else if (strcmp(argv[i], "vu") == 0) test_vu();
        else if (strcmp(argv[i], "spectrum") == 0) test_spectrum();
        else {
            printf("unknown mode %s\n", argv[i]);
            return 2;
        }
    }
    if (failures) return 1;
    printf("ok\n");
    return 0;
}
