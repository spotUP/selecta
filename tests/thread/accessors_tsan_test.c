/* I1082e group D: the state-read accessors that used to read engine memory from the UI thread (stats_get, sync_progress,
 * sources_pending, view_cursor_key, view_row_path) answer from the published snapshot. The engine thread edits the queue and
 * rebuilds the view 10 000 times; the UI thread acquires the newest snapshot and calls every accessor. A reader that touched the
 * live engine (e->queue, e->view, slot state) would race with the pump, which TSan reports (red when the aux copy is reverted).
 * Sentinel: row paths and cursor keys that came back must start with /nonexistent/acc_ and the counter must be above zero, so the
 * test fails if the accessors silently answer nothing. Run under -fsanitize=thread for the proof; the plain run checks the data. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/protocol.h>

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

#define ROUNDS 10000u

static volatile int g_stop;
static uint32_t g_bad, g_paths, g_cursors, g_walks;

static int stopped(void) {
    return __atomic_load_n(&g_stop, __ATOMIC_ACQUIRE);
}

static void walk(sel_engine* e) {
    char out[512];
    sel_engine_stats st;
    sel_sync_progress sp;
    const sel_state* s = sel_engine_state(e);
    const char* ck;
    uint32_t i;

    memset(&st, 0, sizeof st);
    sel_engine_stats_get(e, &st);
    if (st.replayer != NULL && strlen(st.replayer) > 64u) g_bad++;
    if (st.dup_played != NULL && strlen(st.dup_played) > 255u) g_bad++;
    sel_engine_sync_progress(e, &sp);
    if (sp.active > 1u || strlen(sp.source) >= sizeof sp.source) g_bad++;
    if (sel_engine_sources_pending(e) != 0) g_bad++;
    ck = sel_engine_view_cursor_key(e);
    if (ck != NULL) {
        g_cursors++;
        if (strncmp(ck, "/nonexistent/acc_", 17) != 0) g_bad++;
    }
    for (i = 0; s->view != NULL && i < s->view->row_n; i++) {
        if (sel_engine_view_row_path(e, i, out, sizeof out)) {
            g_paths++;
            if (strncmp(out, "/nonexistent/acc_", 17) != 0) g_bad++;
        }
    }
}

static void* ui_thread(void* arg) {
    sel_engine* e = (sel_engine*)arg;
    while (!stopped()) {
        (void)sel_engine_ui_acquire(e);
        walk(e);
        g_walks++;
        sched_yield();
    }
    (void)sel_engine_ui_acquire(e);
    walk(e);
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
    char line[160], pool[256];
    unsigned i;

    memset(&cfg, 0, sizeof cfg);
    cfg.queue_capacity = 256u;
    e = sel_engine_create(sel_host_default(), &cfg);
    if (e == NULL || !sel_engine_publish_enable(e, 1u)) {
        puts("FAIL: engine create or publish enable");
        return 1;
    }
    sel_engine_send_line(e, "VIEW_WINDOW 64", pool, sizeof pool);
    sel_engine_send_line(e, "VIEW_SOURCE queue", pool, sizeof pool);
    sel_engine_pump(e, sink, NULL);
    pthread_create(&ui, NULL, ui_thread, e);
    for (i = 0; i < ROUNDS; i++) {
        if (i % 3u != 2u) {
            snprintf(line, sizeof line, "QUEUE_ADD /nonexistent/acc_%05u_with_a_longer_path_component.mod", i);
            sel_engine_send_line(e, line, pool, sizeof pool);
        } else if (i > 8u) {
            sel_engine_send_line(e, "QUEUE_REMOVE 3", pool, sizeof pool);
        }
        sel_engine_pump(e, sink, NULL);
    }
    __atomic_store_n(&g_stop, 1, __ATOMIC_RELEASE);
    pthread_join(ui, NULL);
    printf("accessors walks=%u paths=%u cursors=%u bad=%u\n", (unsigned)g_walks, (unsigned)g_paths, (unsigned)g_cursors, (unsigned)g_bad);
    sel_engine_destroy(e);
    if (g_bad != 0u || g_paths == 0u) {
        puts("FAIL: accessors (torn answer, or the snapshot answered no row path)");
        return 1;
    }
    puts("accessors_tsan_ok");
    return 0;
}
