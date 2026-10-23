/* T53 unit part: the LED filter -3 dB point on a swept sine (generated here,
 * deterministic) at three render rates, and the width stage identities.
 * The whole-engine reachability test is mixer-led-stereo (tests/m2/mixer.cmake). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stage.h"

static int failures;

#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

#define N 8192

/* RMS gain of the LED filter at `hz`, skipping the settling first half. */
static double gain_at(uint32_t rate, double hz) {
    static int16_t buf[N * 2], ref[N * 2];
    sel_mix_stage s;
    double in = 0, out = 0;
    uint32_t i;
    sel_mix_stage_init(&s, rate);
    sel_mix_stage_set_led(&s, 1);
    for (i = 0; i < N; i++) {
        int16_t v = (int16_t)lrint(16000.0 * sin(2.0 * M_PI * hz * (double)i / (double)rate));
        buf[i * 2] = v;
        buf[i * 2 + 1] = v;
    }
    memcpy(ref, buf, sizeof buf);
    sel_mix_stage_process(&s, buf, N);
    for (i = N / 2; i < N; i++) {
        in += (double)ref[i * 2] * ref[i * 2];
        out += (double)buf[i * 2] * buf[i * 2];
    }
    CHECK(s.led_frames == N, "led_frames %u", (unsigned)s.led_frames);
    return sqrt(out / in);
}

static void led(void) {
    static const uint32_t rates[] = {44100, 48000, 22050};
    unsigned r;
    for (r = 0; r < 3; r++) {
        double f, found = 0;
        /* sweep 100 Hz upward in 5 Hz steps; the first frequency at or below -3.01 dB */
        for (f = 100; f < 9000 && f < rates[r] / 2.0 - 100; f += 5) {
            if (gain_at(rates[r], f) <= 0.70711) {
                found = f;
                break;
            }
        }
        printf("rate %u: -3 dB at %.0f Hz\n", (unsigned)rates[r], found);
        CHECK(found >= 3275 * 0.95 && found <= 3275 * 1.05, "rate %u -3 dB point %.0f Hz outside 3275 +-5%%",
              (unsigned)rates[r], found);
    }
    CHECK(gain_at(44100, 12000) < 0.1, "12 kHz not attenuated at least 20 dB");
    CHECK(gain_at(44100, 200) > 0.99, "200 Hz not passed");
}

static void width(void) {
    int16_t a[8] = {1000, -300, 32767, -32768, 5, 7, -9, 12}, orig[8], b[8];
    sel_mix_stage s;
    int i;
    memcpy(orig, a, sizeof a);
    sel_mix_stage_init(&s, 44100);
    sel_mix_stage_process(&s, a, 4);
    CHECK(memcmp(a, orig, sizeof a) == 0 && s.width_frames == 0, "width 100 must be a bypass");
    sel_mix_stage_set_width(&s, 0);
    sel_mix_stage_process(&s, a, 4);
    for (i = 0; i < 4; i++) CHECK(a[i * 2] == a[i * 2 + 1], "width 0 frame %d not mono", i);
    CHECK(s.width_frames == 4, "width_frames %u", (unsigned)s.width_frames);
    memcpy(b, orig, sizeof b);
    sel_mix_stage_set_width(&s, 50);
    sel_mix_stage_process(&s, b, 4);
    CHECK(b[0] == 675 && b[1] == 25, "width 50 frame 0 is %d %d, want 675 25", b[0], b[1]);
    /* the Paula flag bypasses both stages */
    memcpy(b, orig, sizeof b);
    s.paula_output = 1;
    sel_mix_stage_set_led(&s, 1);
    sel_mix_stage_process(&s, b, 4);
    CHECK(memcmp(b, orig, sizeof b) == 0, "paula_output must bypass both stages");
}

int main(void) {
    led();
    width();
    printf(failures ? "FAILED\n" : "OK\n");
    return failures != 0;
}
