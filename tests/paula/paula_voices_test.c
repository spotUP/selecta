/* T50 voice API conformance of the Paula sink (plan M2 phase 3, I118/I119): chip copy through
 * host->mem_alloc(SEL_MEM_SAMPLE), AUDxPER = round(3546895 / hz) once per note, AUDxVOL = volume_q16 >> 10,
 * loop registers, free on stop. Behaviour is tested here; reachability through the CLI is T51. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../plugins/outputs/paula/paula_voices.h"

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

typedef struct rec {
    uint32_t reg[64];
    uint32_t val[64];
    uint32_t n;
    uint32_t lc_offset[16];
    uint32_t lc_count;
    int sample_allocs, frees;
    const void* last_bytes;
} rec;

static rec R;

static void bus_write(void* ctx, uint32_t reg, uint32_t value) {
    (void)ctx;
    if (R.n < 64) {
        R.reg[R.n] = reg;
        R.val[R.n++] = value;
    }
}

static void bus_lc(void* ctx, uint32_t ch, const void* bytes, uint32_t offset, uint32_t serial) {
    (void)ctx;
    (void)ch;
    (void)serial;
    R.last_bytes = bytes;
    if (R.lc_count < 16) R.lc_offset[R.lc_count++] = offset;
}

static void* h_alloc(const sel_host* h, size_t size, uint32_t kind) {
    (void)h;
    if (kind == SEL_MEM_SAMPLE) R.sample_allocs++;
    return malloc(size);
}

static void h_free(const sel_host* h, void* p) {
    (void)h;
    R.frees++;
    free(p);
}

static uint32_t count_reg(uint32_t reg) {
    uint32_t i, c = 0;
    for (i = 0; i < R.n; i++) {
        if (R.reg[i] == reg) c++;
    }
    return c;
}

static uint32_t last_val(uint32_t reg) {
    uint32_t i, v = 0xFFFFFFFFu;
    for (i = 0; i < R.n; i++) {
        if (R.reg[i] == reg) v = R.val[i];
    }
    return v;
}

int main(void) {
    sel_host host;
    sel_paula p;
    sel_paula_bus bus;
    const sel_voices* v;
    int8_t body[100];
    sel_sample s;
    sel_voice_state st;
    uint32_t i, hz_q8;

    memset(&host, 0, sizeof host);
    host.size = sizeof host;
    host.mem_alloc = h_alloc;
    host.mem_free = h_free;
    bus.write = bus_write;
    bus.set_lc = bus_lc;
    bus.ctx = NULL;
    memset(&R, 0, sizeof R);
    for (i = 0; i < 100; i++) body[i] = (int8_t)(i - 50);
    s.data = body;
    s.format = SEL_SAMPLE_S8;
    s.length = 100;
    s.loop_start = 40;
    s.loop_length = 40;

    sel_paula_init(&p, &host, &bus);
    v = sel_paula_api(&p);
    CHECK(v->size == sizeof(sel_voices), "voices size %u", (unsigned)v->size);

    /* period formula: 3546895 / 428 Hz is ProTracker C-2 */
    hz_q8 = (SEL_PAULA_CLOCK * 256u + 214u) / 428u;
    CHECK(sel_paula_period_from_q8(hz_q8) == 428u, "period for C-2 is %u", (unsigned)sel_paula_period_from_q8(hz_q8));
    CHECK(sel_paula_period_from_q8(0) == 0u, "zero hz");
    CHECK(sel_paula_period_from_q8(0xFFFFFFFFu) == SEL_PAULA_PERIOD_MIN, "high hz clamps to 113");

    v->set_pitch_hz(v, 0, hz_q8);
    v->set_volume(v, 0, 65536u);
    CHECK(last_val(SEL_PAULA_AUD_PER) == 428u, "PER at set_pitch_hz: %u", (unsigned)last_val(SEL_PAULA_AUD_PER));
    CHECK(last_val(SEL_PAULA_AUD_VOL) == 64u, "VOL 65536>>10 is 64, got %u", (unsigned)last_val(SEL_PAULA_AUD_VOL));
    v->play(v, 0, &s, 0);
    for (i = 0; i < 10; i++) v->set_pitch_hz(v, 0, hz_q8); /* a tick re-asserting the same pitch */
    v->set_volume(v, 0, 65536u);
    CHECK(count_reg(SEL_PAULA_AUD_PER) == 1u, "PER written %u times for one note", (unsigned)count_reg(SEL_PAULA_AUD_PER));
    CHECK(count_reg(SEL_PAULA_AUD_VOL) == 1u, "VOL written %u times", (unsigned)count_reg(SEL_PAULA_AUD_VOL));

    /* chip copy: one SEL_MEM_SAMPLE allocation, not the caller's pointer, same bytes */
    CHECK(R.sample_allocs == 1, "sample allocations %d", R.sample_allocs);
    CHECK(R.last_bytes != NULL && R.last_bytes != (const void*)body, "LC points at the caller's memory");
    CHECK(R.last_bytes != NULL && memcmp(R.last_bytes, body, 100) == 0, "chip copy differs from the sample");
    CHECK(v->is_playing(v, 0), "not playing");
    v->get_state(v, 0, &st);
    CHECK(st.playing && st.sample != NULL && st.sample->data != (const void*)body, "state sample is not the copy");
    CHECK(st.pitch_q8 == hz_q8, "state pitch");

    /* loop registers: first pass LEN 50 words from 0, then the loop part LEN 20 words from byte 40 */
    CHECK(count_reg(SEL_PAULA_AUD_LEN) == 2u, "LEN written %u times", (unsigned)count_reg(SEL_PAULA_AUD_LEN));
    CHECK(R.lc_count == 2u && R.lc_offset[0] == 0u && R.lc_offset[1] == 40u, "LC offsets %u %u", (unsigned)R.lc_offset[0],
          (unsigned)R.lc_offset[1]);
    CHECK(last_val(SEL_PAULA_AUD_LEN) == 20u, "loop LEN %u", (unsigned)last_val(SEL_PAULA_AUD_LEN));
    CHECK(count_reg(SEL_PAULA_DMACON) >= 2u, "DMACON writes %u", (unsigned)count_reg(SEL_PAULA_DMACON));

    v->set_volume(v, 0, 32768u);
    CHECK(last_val(SEL_PAULA_AUD_VOL) == 32u, "VOL 32768>>10 is 32, got %u", (unsigned)last_val(SEL_PAULA_AUD_VOL));

    v->stop(v, 0);
    CHECK(!v->is_playing(v, 0), "still playing after stop");
    CHECK(R.frees == 1, "frees %d", R.frees);
    CHECK(last_val(SEL_PAULA_DMACON) == 1u, "DMACON clear on stop");
    sel_paula_shutdown(&p);
    CHECK(R.frees == 1, "double free on shutdown: %d", R.frees);

    if (failures) {
        printf("paula_voices_test: %d failures\n", failures);
        return 1;
    }
    printf("paula_voices_test: ok\n");
    return 0;
}
