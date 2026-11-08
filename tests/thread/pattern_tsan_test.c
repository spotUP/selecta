/* I1082e group G4: pattern_request from the UI thread. The UI thread asks for pattern windows while the engine thread pumps and
 * renders (renders answer the request) and publishes. Before, pattern_request wrote the request fields from the UI thread and
 * the audio side read them unsynchronised; now the request goes through a mailbox the pump drains. Checks: every request number
 * the UI got back is issued in order (no gaps), state.pattern->seq never exceeds the last issued and never goes back, and the
 * sentinel counts answered requests. Run under -fsanitize=thread for the race proof. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/protocol.h>

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

#define ROUNDS 20000u

static volatile int g_stop;
static uint32_t g_issued, g_answered, g_bad, g_walks;

static void* ui_thread(void* arg) {
    sel_engine* e = (sel_engine*)arg;
    uint32_t last_seq = 0;

    while (!__atomic_load_n(&g_stop, __ATOMIC_ACQUIRE)) {
        const sel_state* s;
        uint32_t r, issued;

        (void)sel_engine_ui_acquire(e);
        s = sel_engine_state(e);
        issued = __atomic_load_n(&g_issued, __ATOMIC_RELAXED);
        if (s->pattern != NULL) {
            if (s->pattern->seq < last_seq || s->pattern->seq > issued) g_bad++;
            if (s->pattern->seq != last_seq) g_answered++;
            last_seq = s->pattern->seq;
        }
        r = sel_engine_pattern_request(e, 0u, g_walks % 32u, 16u);
        if (r != 0u) {
            if (r != issued + 1u) g_bad++;
            __atomic_store_n(&g_issued, r, __ATOMIC_RELAXED);
        }
        g_walks++;
        sched_yield();
    }
    return NULL;
}

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
}

int main(void) {
    sel_engine_config cfg;
    sel_engine* e;
    pthread_t ui;
    int16_t pcm[2 * 256];
    unsigned i;

    memset(&cfg, 0, sizeof cfg);
    cfg.queue_capacity = 16u;
    e = sel_engine_create(sel_host_default(), &cfg);
    if (e == NULL || !sel_engine_publish_enable(e, 1u)) {
        puts("FAIL: engine create or publish enable");
        return 1;
    }
    sel_engine_pump(e, sink, NULL);
    pthread_create(&ui, NULL, ui_thread, e);
    for (i = 0; i < ROUNDS; i++) {
        sel_engine_pump(e, sink, NULL);
        (void)sel_engine_render(e, pcm, 256u);
        sched_yield();
    }
    __atomic_store_n(&g_stop, 1, __ATOMIC_RELEASE);
    pthread_join(ui, NULL);
    printf("pattern walks=%u issued=%u answered=%u bad=%u\n", (unsigned)g_walks, (unsigned)g_issued, (unsigned)g_answered, (unsigned)g_bad);
    sel_engine_destroy(e);
    if (g_bad != 0u || g_issued < 10u || g_answered < 10u) {
        puts("FAIL: pattern requests (out of order, unanswered, or none issued)");
        return 1;
    }
    puts("pattern_tsan_ok");
    return 0;
}
