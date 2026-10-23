/* T90 unit part (I189): the 16.16 gain table against libm over the whole stored range, the limiter ceiling on a
 * hot signal, and the bypass. The whole-engine reachability test is loudness-playback (tests/m3/loudness_playback.sh). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "loudness_gain.h"

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

#define N 4096

int main(void) {
    static int16_t buf[N * 2], ref[N * 2];
    sel_loudness_gain g;
    int32_t cdb;
    uint32_t i, ceiling_ok = 1;
    int worst = 0;

    for (cdb = -2400; cdb <= 2400; cdb++) {
        double exact = 65536.0 * pow(10.0, (double)cdb / 2000.0);
        int d = (int)sel_gain_q16_from_cdb(cdb) - (int)floor(exact + 0.5);
        if (d < 0) d = -d;
        if (d > worst) worst = d;
    }
    CHECK(worst <= 1, "gain table off by %d LSB of 16.16", worst);
    CHECK(sel_gain_q16_from_cdb(0) == SEL_GAIN_UNITY, "0 cdb is not unity");
    CHECK(sel_gain_q16_from_cdb(5000) == sel_gain_q16_from_cdb(2400), "gain not clamped high");
    CHECK(sel_gain_q16_from_cdb(-5000) == sel_gain_q16_from_cdb(-2400), "gain not clamped low");

    for (i = 0; i < N; i++) {
        int16_t s = (int16_t)(30000.0 * sin(6.283185307179586 * 440.0 * (double)i / 44100.0));
        buf[i * 2u] = s;
        buf[i * 2u + 1u] = (int16_t)(-s / 2);
    }
    memcpy(ref, buf, sizeof buf);

    memset(&g, 0, sizeof g);
    sel_loudness_gain_set(&g, 0, sel_gain_q16_from_cdb(600));
    sel_loudness_gain_process(&g, buf, N);
    CHECK(memcmp(buf, ref, sizeof buf) == 0 && g.frames == 0 && g.hits == 0, "inactive stage changed the samples");

    sel_loudness_gain_set(&g, 1, sel_gain_q16_from_cdb(1200)); /* +12 dB: far over full scale */
    sel_loudness_gain_process(&g, buf, N);
    CHECK(g.frames == N, "frames sentinel %u", (unsigned)g.frames);
    for (i = 0; i < N * 2u; i++) {
        int v = buf[i] < 0 ? -(int)buf[i] : (int)buf[i];
        if (v > SEL_LIMIT_CEILING) ceiling_ok = 0;
    }
    CHECK(ceiling_ok, "a sample exceeds -1 dBFS (%d)", SEL_LIMIT_CEILING);
    CHECK(g.hits > 0, "limiter never engaged on a +12 dB hot signal");

    /* a quiet signal with a cut passes the limiter untouched and is scaled exactly */
    memset(buf, 0, sizeof buf);
    for (i = 0; i < 64; i++) buf[i] = (int16_t)(1000 + (int)i * 10);
    memcpy(ref, buf, sizeof ref);
    sel_loudness_gain_set(&g, 1, sel_gain_q16_from_cdb(-600));
    g.hits = 0;
    sel_loudness_gain_process(&g, buf, 32);
    for (i = 0; i < 64; i++) {
        int32_t want = (int32_t)(((int64_t)ref[i] * (int64_t)sel_gain_q16_from_cdb(-600) + 32768) >> 16);
        CHECK(buf[i] == want, "sample %u: %d want %d", (unsigned)i, buf[i], want);
    }
    CHECK(g.hits == 0, "limiter engaged on a quiet signal");

    if (failures == 0) puts("loudness_gain_unit_ok");
    return failures != 0;
}
