/* I195c: a STOP sent while the pump pops must never lose its net cancel (plan 2026-10-25-m3-library.md, Phase 11).
 * One thread sends STOP in a tight loop, the main thread pumps. The engine counts every dispatch of a canceller that
 * had another canceller queued behind it with no cancel pending (net_cancel_lost); it must stay 0. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/protocol.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define ROUNDS 300000u

static sel_engine* engine;
static volatile int sender_done;

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
}

static void* sender(void* arg) {
    sel_message m;
    unsigned i;
    (void)arg;
    memset(&m, 0, sizeof m);
    m.kind = SEL_MSG_CMD;
    m.id = SEL_CMD_STOP;
    for (i = 0; i < ROUNDS; i++) {
        while (!sel_engine_send(engine, &m)) {
        }
    }
    sender_done = 1;
    return NULL;
}

int main(void) {
    pthread_t t;
    sel_engine_stats st;
    engine = sel_engine_create(sel_host_default(), NULL);
    if (!engine) {
        fputs("engine create failed\n", stderr);
        return 2;
    }
    if (pthread_create(&t, NULL, sender, NULL) != 0) {
        return 2;
    }
    while (!sender_done) {
        sel_engine_pump(engine, sink, NULL);
    }
    sel_engine_pump(engine, sink, NULL);
    pthread_join(t, NULL);
    sel_engine_stats_get(engine, &st);
    sel_engine_destroy(engine);
    printf("cancel_race lost=%u\n", st.net_cancel_lost);
    if (st.net_cancel_lost != 0) {
        puts("FAIL: cancels lost to the send/pump race");
        return 1;
    }
    puts("cancel_race_ok");
    return 0;
}
