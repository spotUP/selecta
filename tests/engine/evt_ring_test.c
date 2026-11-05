/* I1082c: the engine -> UI event ring. A producer thread (the pump thread of a paced shell) pushes 10 000 events through
 * the shell sink while the main thread (the UI) drains the ring in its turns. Every event must reach on_event on the UI
 * thread, in order, none lost, with its string and tag-list string intact although the producer overwrites its buffers
 * the moment the sink returns. */
#include "../../src/engine/evtring.h"

#include <selecta/host.h>
#include <selecta/protocol.h>
#include <selecta/tags.h>

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

#define EVENTS 10000u

static pthread_t g_ui;
static volatile int g_done;
static uint32_t g_seen, g_wrong_thread, g_bad_order, g_bad_text;

static void on_event(void* ctx, const sel_message* e) {
    char want[64];
    uint32_t n = g_seen;

    (void)ctx;
    g_wrong_thread += !pthread_equal(pthread_self(), g_ui);
    snprintf(want, sizeof want, "event-%u-payload", n);
    if (n % 2u == 0u) {
        g_bad_order += e->id != SEL_EVT_STATE;
        g_bad_text += e->argc != 1 || strcmp(e->args[0].v.s, want) != 0;
    } else {
        const sel_tag* t = e->args[0].v.t;
        g_bad_order += e->id != SEL_EVT_SONG_OPENED;
        g_bad_text += t == NULL || sel_tag_find(t, SEL_META_TITLE, 0) == 0 ||
                      strcmp((const char*)sel_tag_find(t, SEL_META_TITLE, 0), want) != 0 || sel_tag_find(t, SEL_META_YEAR, 0) != n;
    }
    g_seen++;
}

static void yield_idle(void* ctx) {
    (void)ctx;
    sched_yield();
}

static void* producer(void* arg) {
    sel_evt_ring* r = (sel_evt_ring*)arg;
    char text[64];
    sel_tag tags[3];
    sel_message m;
    uint32_t i;

    for (i = 0; i < EVENTS; i++) {
        memset(&m, 0, sizeof m);
        m.kind = SEL_MSG_EVT;
        snprintf(text, sizeof text, "event-%u-payload", i);
        if (i % 2u == 0u) {
            m.id = SEL_EVT_STATE;
            m.argc = 1;
            m.args[0].type = SEL_ARG_STR;
            m.args[0].v.s = text;
        } else {
            m.id = SEL_EVT_SONG_OPENED;
            m.argc = 1;
            tags[0].tag = SEL_META_TITLE;
            tags[0].value = (uintptr_t)text;
            tags[1].tag = SEL_META_YEAR;
            tags[1].value = i;
            tags[2].tag = SEL_TAG_END;
            tags[2].value = 0;
            m.args[0].type = SEL_ARG_TAGS;
            m.args[0].v.t = tags;
        }
        sel_evt_ring_sink(r, &m);
        memset(text, 'X', sizeof text - 1u); /* the caller's buffers are free once the sink returns */
        memset(tags, 0, sizeof tags);
    }
    g_done = 1;
    return NULL;
}

int main(void) {
    sel_evt_ring r;
    pthread_t th;
    uint32_t turns = 0;

    g_ui = pthread_self();
    if (!sel_evt_ring_init(&r, sel_host_default(), 64u, 512u, on_event, NULL)) {
        puts("FAIL: ring init");
        return 2;
    }
    r.idle = yield_idle;
    pthread_create(&th, NULL, producer, &r);
    while (!g_done) {
        sel_evt_ring_drain(&r);
        turns++;
        sched_yield();
    }
    pthread_join(th, NULL);
    sel_evt_ring_drain(&r);
    printf("evt_ring seen=%u pushed=%u drained=%u dropped=%u wrong_thread=%u bad_order=%u bad_text=%u turns=%u\n", g_seen, r.pushed,
           r.drained, r.dropped, g_wrong_thread, g_bad_order, g_bad_text, turns);
    sel_evt_ring_free(&r, sel_host_default());
    if (g_seen != EVENTS || r.pushed != EVENTS || r.dropped != 0 || g_wrong_thread != 0 || g_bad_order != 0 || g_bad_text != 0) {
        puts("FAIL: events lost, reordered, corrupted or delivered off the UI thread");
        return 1;
    }
    puts("evt_ring_ok");
    return 0;
}
