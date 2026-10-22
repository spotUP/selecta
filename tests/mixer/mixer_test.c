/* T12 mixer-unit: period step, resampler vectors, clipping and the
 * loop / queue semantics of nuade_mixer.c settle(). Behaviour is tested where
 * it lives; the reachability test of the whole engine is T13.
 * Usage: mixer_test [group...]   groups: period none linear cubic sinc clip loop
 * With no argument every group runs. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mixer.h"
#include "vectors.h"

static int failures;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            failures++;                                   \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);   \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static int wanted(int argc, char** argv, const char* group) {
    int i;
    if (argc < 2) return 1;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], group) == 0) return 1;
    }
    return 0;
}

static void setup(sel_mixer* m, uint32_t voices, uint32_t resampler) {
    sel_mixer_init(m, VEC_RATE, voices, resampler);
}

/* Voice 0 at full volume, centre, step = num/den source frames per frame. */
static void unit_voice(sel_mixer* m, uint32_t voice, uint32_t hz_q8) {
    const sel_voices* v = sel_mixer_voices(m);
    v->set_volume(v, voice, 65536u);
    v->set_pan(v, voice, 0);
    v->set_pitch_hz(v, voice, hz_q8);
}

static void check_run(const char* name, uint32_t resampler, const int16_t* expect) {
    static const int16_t src[VEC_SRC_LEN] = VEC_SRC;
    sel_mixer m;
    sel_sample s;
    int16_t out[VEC_FRAMES * 2];
    uint32_t i;
    memset(&s, 0, sizeof s);
    s.data = src;
    s.format = SEL_SAMPLE_S16;
    s.length = VEC_SRC_LEN;
    setup(&m, 1, resampler);
    unit_voice(&m, 0, VEC_RATE * 128u);
    sel_mixer_voices(&m)->play(sel_mixer_voices(&m), 0, &s, 0);
    sel_mixer_mix(&m, out, VEC_FRAMES);
    for (i = 0; i < VEC_FRAMES; i++) {
        CHECK(out[i * 2] == expect[i] && out[i * 2 + 1] == expect[i],
              "%s frame %u: got %d/%d want %d", name, (unsigned)i, out[i * 2], out[i * 2 + 1], expect[i]);
    }
}

static void test_period(void) {
    uint32_t step = sel_mixer_period_step(428u, 44100u);
    uint32_t want = (uint32_t)((((uint64_t)3546895u) << 16) / ((uint64_t)428u * 44100u));
    CHECK(step == want, "period step %u != %u", (unsigned)step, (unsigned)want);
    CHECK(step == 12315u, "period 428 at 44100 Hz must be 12315, got %u", (unsigned)step);
    /* 24.8 pitch: 44100 Hz at rate 44100 is exactly one source frame per output frame. */
    CHECK(sel_mixer_hz_step(44100u * 256u, 44100u) == 65536u, "hz step for rate == pitch");
    CHECK(sel_mixer_hz_step(22050u * 256u, 44100u) == 32768u, "hz step for half rate");
    /* set_period through the voices table lands on the same step (observable as position). */
    {
        static const int16_t src[64] = {0};
        sel_mixer m;
        sel_sample s;
        sel_voice_state st;
        const sel_voices* v;
        memset(&s, 0, sizeof s);
        s.data = src;
        s.format = SEL_SAMPLE_S16;
        s.length = 64;
        setup(&m, 1, SEL_MIX_RESAMPLE_NONE);
        v = sel_mixer_voices(&m);
        v->set_period(v, 0, 428u);
        v->play(v, 0, &s, 0);
        {
            int16_t out[2 * 10];
            sel_mixer_mix(&m, out, 10);
        }
        v->get_state(v, 0, &st);
        /* 10 frames * 12315 / 65536 = 1.87 source frames. */
        CHECK(st.position == 1u, "position after 10 frames at period 428: %u", (unsigned)st.position);
    }
}

static void test_none(void) {
    static const int16_t e[VEC_FRAMES] = VEC_NONE;
    check_run("none", SEL_MIX_RESAMPLE_NONE, e);
}

static void test_linear(void) {
    static const int16_t e[VEC_FRAMES] = VEC_LINEAR;
    check_run("linear", SEL_MIX_RESAMPLE_LINEAR, e);
}

static void test_cubic(void) {
    static const int16_t e[VEC_FRAMES] = VEC_CUBIC;
    static const int16_t src[4] = {0, 1000, 2000, 3000};
    sel_sample s;
    memset(&s, 0, sizeof s);
    s.data = src;
    s.format = SEL_SAMPLE_S16;
    s.length = 4;
    check_run("cubic", SEL_MIX_RESAMPLE_CUBIC, e);
    CHECK(sel_resample(&s, 1, 0x4000u, SEL_MIX_RESAMPLE_CUBIC) == VEC_CUBIC_Q, "cubic t=0.25 got %d",
          (int)sel_resample(&s, 1, 0x4000u, SEL_MIX_RESAMPLE_CUBIC));
}

static void test_sinc(void) {
    static const int16_t dc[16] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
                                   1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    static const int16_t ramp[16] = {0,    1000, 2000, 3000, 4000,  5000,  6000,  7000,
                                     8000, 9000, 10000, 11000, 12000, 13000, 14000, 15000};
    sel_sample s;
    uint32_t p, k;
    int32_t half;
    memset(&s, 0, sizeof s);
    s.format = SEL_SAMPLE_S16;
    s.length = 16;
    /* every phase row sums to 1.0 in Q15 */
    for (p = 0; p < 256; p++) {
        int32_t sum = 0;
        const int32_t* row = sel_resample_sinc_row(p);
        CHECK(row != NULL, "sinc row %u missing", (unsigned)p);
        if (row == NULL) return;
        for (k = 0; k < 8; k++) sum += row[k];
        CHECK(sum == 32768, "sinc row %u sums to %d", (unsigned)p, (int)sum);
    }
    /* phase 0 is the identity: 0 0 0 32768 0 0 0 0 */
    {
        const int32_t* row = sel_resample_sinc_row(0);
        for (k = 0; k < 8; k++) CHECK(row[k] == (k == 3 ? 32768 : 0), "sinc row 0 tap %u = %d", (unsigned)k, (int)row[k]);
    }
    s.data = dc;
    CHECK(sel_resample(&s, 8, 0x8000u, SEL_MIX_RESAMPLE_SINC) == 1000, "sinc keeps DC");
    CHECK(sel_resample(&s, 8, 0x1300u, SEL_MIX_RESAMPLE_SINC) == 1000, "sinc keeps DC at phase 19");
    s.data = ramp;
    CHECK(sel_resample(&s, 8, 0u, SEL_MIX_RESAMPLE_SINC) == 8000, "sinc whole position is the sample");
    half = sel_resample(&s, 7, 0x8000u, SEL_MIX_RESAMPLE_SINC);
    CHECK(half >= 7497 && half <= 7503, "sinc ramp midpoint 7500 +-3, got %d", (int)half);
}

/* Mix `frames` mono-equivalent frames of voice 0 and compare with expect (left == right). */
static void expect_run(sel_mixer* m, const int16_t* expect, uint32_t frames, const char* what) {
    int16_t out[16 * 2];
    uint32_t i;
    sel_mixer_mix(m, out, frames);
    for (i = 0; i < frames; i++) {
        CHECK(out[i * 2] == expect[i] && out[i * 2 + 1] == expect[i], "%s frame %u: got %d/%d want %d", what,
              (unsigned)i, out[i * 2], out[i * 2 + 1], expect[i]);
    }
}

static void test_clip(void) {
    static const int16_t hi[4] = {32767, 32767, 32767, 32767};
    static const int16_t lo[4] = {-32768, -32768, -32768, -32768};
    static const int8_t b[2] = {1, 2};
    sel_mixer m;
    sel_sample s;
    int16_t out[8 * 2];
    uint32_t n, i;
    memset(&s, 0, sizeof s);
    s.format = SEL_SAMPLE_S16;
    s.length = 4;
    s.loop_start = 0;
    s.loop_length = 4;
    for (i = 0; i < 2; i++) {
        s.data = i == 0 ? hi : lo;
        setup(&m, 32, SEL_MIX_RESAMPLE_NONE);
        for (n = 0; n < 32; n++) {
            unit_voice(&m, n, VEC_RATE * 256u);
            sel_mixer_voices(&m)->play(sel_mixer_voices(&m), n, &s, 0);
        }
        sel_mixer_mix(&m, out, 8);
        for (n = 0; n < 8 * 2; n++) {
            CHECK(out[n] == (i == 0 ? 32767 : -32768), "32 voices %s: sample %u = %d", i == 0 ? "high" : "low",
                  (unsigned)n, out[n]);
        }
    }
    /* S8 is scaled to 16 bit: 1 -> 256 -> 64 */
    memset(&s, 0, sizeof s);
    s.data = b;
    s.format = SEL_SAMPLE_S8;
    s.length = 2;
    setup(&m, 1, SEL_MIX_RESAMPLE_NONE);
    unit_voice(&m, 0, VEC_RATE * 256u);
    sel_mixer_voices(&m)->play(sel_mixer_voices(&m), 0, &s, 0);
    {
        static const int16_t e[2] = {64, 128};
        expect_run(&m, e, 2, "s8");
    }
}

/* Samples are S16 400, 800, ...; at gain 1/4 they come out 100, 200, ... */
static void test_loop(void) {
    static const int16_t a[4] = {400, 800, 1200, 1600};
    static const int16_t q[3] = {2000, 2400, 2800};
    sel_mixer m;
    sel_sample s, t;
    const sel_voices* v;
    sel_voice_state st;
    memset(&s, 0, sizeof s);
    memset(&t, 0, sizeof t);
    s.data = a;
    s.format = SEL_SAMPLE_S16;
    s.length = 4;
    t.data = q;
    t.format = SEL_SAMPLE_S16;
    t.length = 3;

    /* one-shot end: plays 4 frames, then silence and not playing */
    {
        static const int16_t e[6] = {100, 200, 300, 400, 0, 0};
        setup(&m, 1, SEL_MIX_RESAMPLE_NONE);
        v = sel_mixer_voices(&m);
        unit_voice(&m, 0, VEC_RATE * 256u);
        v->play(v, 0, &s, 0);
        CHECK(v->is_playing(v, 0) == 1, "one-shot playing before mix");
        expect_run(&m, e, 6, "one-shot");
        CHECK(v->is_playing(v, 0) == 0, "one-shot finished");
        v->get_state(v, 0, &st);
        CHECK(st.playing == 0 && st.sample == NULL, "state after end");
    }
    /* loop wrap: indices 0 1 2 3 2 3 2 3 */
    {
        static const int16_t e[8] = {100, 200, 300, 400, 300, 400, 300, 400};
        sel_sample l = s;
        l.loop_start = 2;
        l.loop_length = 2;
        setup(&m, 1, SEL_MIX_RESAMPLE_NONE);
        v = sel_mixer_voices(&m);
        unit_voice(&m, 0, VEC_RATE * 256u);
        v->play(v, 0, &l, 0);
        expect_run(&m, e, 8, "loop");
        CHECK(v->is_playing(v, 0) == 1, "loop keeps playing");
    }
    /* queued sample takes over at the end of a one-shot, then ends */
    {
        static const int16_t e[9] = {100, 200, 300, 400, 500, 600, 700, 0, 0};
        setup(&m, 1, SEL_MIX_RESAMPLE_NONE);
        v = sel_mixer_voices(&m);
        unit_voice(&m, 0, VEC_RATE * 256u);
        v->play(v, 0, &s, 0);
        v->queue(v, 0, &t);
        CHECK(v->is_playing(v, 0) == 1, "queued sample keeps the voice playing");
        expect_run(&m, e, 9, "queue after one-shot");
    }
    /* queued sample wins over the loop of the current one */
    {
        static const int16_t e[8] = {100, 200, 300, 400, 500, 600, 700, 0};
        sel_sample l = s;
        l.loop_start = 2;
        l.loop_length = 2;
        setup(&m, 1, SEL_MIX_RESAMPLE_NONE);
        v = sel_mixer_voices(&m);
        unit_voice(&m, 0, VEC_RATE * 256u);
        v->play(v, 0, &l, 0);
        v->queue(v, 0, &t);
        expect_run(&m, e, 8, "queue over loop");
    }
    /* queue on an idle voice starts at once; play with a start frame */
    {
        static const int16_t e[3] = {200, 300, 400};
        setup(&m, 1, SEL_MIX_RESAMPLE_NONE);
        v = sel_mixer_voices(&m);
        unit_voice(&m, 0, VEC_RATE * 256u);
        v->queue(v, 0, &s);
        CHECK(v->is_playing(v, 0) == 1, "queue on idle voice plays");
        v->play(v, 0, &s, 1);
        expect_run(&m, e, 3, "start frame");
    }
    /* stop silences and drops the queue */
    {
        static const int16_t e[2] = {0, 0};
        setup(&m, 1, SEL_MIX_RESAMPLE_NONE);
        v = sel_mixer_voices(&m);
        unit_voice(&m, 0, VEC_RATE * 256u);
        v->play(v, 0, &s, 0);
        v->queue(v, 0, &t);
        v->stop(v, 0);
        CHECK(v->is_playing(v, 0) == 0, "stopped");
        expect_run(&m, e, 2, "stop");
    }
}

int main(int argc, char** argv) {
    if (wanted(argc, argv, "period")) test_period();
    if (wanted(argc, argv, "none")) test_none();
    if (wanted(argc, argv, "linear")) test_linear();
    if (wanted(argc, argv, "cubic")) test_cubic();
    if (wanted(argc, argv, "sinc")) test_sinc();
    if (wanted(argc, argv, "clip")) test_clip();
    if (wanted(argc, argv, "loop")) test_loop();
    printf("mixer_test failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
