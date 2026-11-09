/* I1231b: the live search result cache, through the real entry point (SOURCE_SEARCH in, pump out).
 * One stand-in live source (no catalogue flag, so it always runs its network hook) answers "jam" with two rows.
 *  - Run 1 runs the hook and stores its answer in library.db (search_cache).
 *  - Run 2 (same query, other case and spacing) holds the hook on a gate. Sentinel: while the hook has completed only once
 *    (run 1), both stored rows are already delivered as search results and the engine stats count one cache hit.
 *  - Opening the gate lets the background refresh finish: it lands its new answer (three rows) in place and a search_done
 *    with count 3 follows.
 * With the cached landing removed from search_collect, run 2 shows no rows before the gate opens and the test fails.
 * Prints the time to the cached rows against the time the live answer took. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/source.h>

#include "db/db.h"
#include "db/db_internal.h"
#include "source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile int g_gate = 1;   /* 0 = the hook waits */
static volatile int g_calls;      /* hook entered */
static volatile int g_completed;  /* hook returned */
static int g_answer = 2;          /* rows the hook answers */
static unsigned g_rows;
static unsigned g_done_n;
static int g_done;
static int g_fail;

static void nap(long ms) {
    struct timespec ts = {0, ms * 1000000L};
    nanosleep(&ts, NULL);
}

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int hook(const sel_host* host, const char* q, sel_search_sink* out) {
    static const char* const path[3] = {"x/jammer.mod", "x/jamming.mod", "x/fresh.mod"};
    int i, n = g_answer;
    (void)host;
    (void)q;
    __atomic_add_fetch(&g_calls, 1, __ATOMIC_SEQ_CST);
    while (!__atomic_load_n(&g_gate, __ATOMIC_SEQ_CST)) {
        nap(1);
    }
    nap(150); /* a network round trip */
    for (i = 0; i < n; i++) {
        if (!out->row(out->ctx, path[i], path[i] + 2, "Live", "MOD")) break;
    }
    __atomic_add_fetch(&g_completed, 1, __ATOMIC_SEQ_CST);
    return i;
}

static const sel_source g_live = {"i1231bl", "i1231bl", "", "I1231b stand-in.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                  0u, NULL, NULL, hook, NULL};

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_LIBRARY_RESULT) {
        const sel_tag* t = evt->args[0].v.t;
        const char* kind = (const char*)sel_tag_find(t, SEL_SRC_KIND, (uintptr_t)"");
        const char* key = (const char*)sel_tag_find(t, SEL_SRC_KEY, (uintptr_t)"");

        if (strcmp(key, "i1231bl") != 0) return;
        if (strcmp(kind, "search") == 0) {
            g_rows++;
        } else if (strcmp(kind, "search_done") == 0) {
            g_done = 1;
            g_done_n = (unsigned)sel_tag_find(t, SEL_SRC_COUNT, 0);
        }
    }
}

static int send_line(sel_engine* e, const char* line) {
    static char pool[4][2048];
    static unsigned next;
    return sel_engine_send_line(e, line, pool[next++ & 3u], sizeof pool[0]) == 0;
}

#define CHECK(c)                                                                                                  \
    do {                                                                                                          \
        if (!(c)) {                                                                                               \
            printf("FAIL line %d: %s (rows=%u done=%d n=%u calls=%d completed=%d)\n", __LINE__, #c, g_rows, g_done, \
                   g_done_n, g_calls, g_completed);                                                               \
            g_fail++;                                                                                             \
        }                                                                                                         \
    } while (0)

/* Pumps until cond() or about 5 s. */
static void pump_until(sel_engine* e, int (*cond)(void)) {
    int i;
    for (i = 0; i < 5000 && !cond(); i++) {
        sel_engine_pump(e, sink, NULL);
        nap(1);
    }
}
static int c_done(void) { return g_done; }
static int c_rows2(void) { return g_rows >= 2u; }

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i1231b-XXXXXX";
    char db[128];
    sel_engine_config cfg;
    sel_engine_stats st;
    sel_engine* e;
    sel_library* lib;
    long t0, live_ms, cached_ms;
    int fd = mkstemp(path);

    if (fd < 0) {
        puts("FAIL mkstemp");
        return 1;
    }
    close(fd);
    snprintf(db, sizeof db, "%s.db", path);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    if (lib == NULL) {
        puts("FAIL open");
        return 1;
    }
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    e = sel_engine_create(host, &cfg);
    if (e == NULL) {
        puts("FAIL engine");
        return 1;
    }
    sel_source_register_builtin();
    if (!sel_source_register(&g_live)) {
        puts("FAIL register");
        return 1;
    }
    { /* only the stand-in answers: every built-in source is switched off, so no network is touched */
        char line[1024] = "PREF_SET library.sources_off \"";
        uint32_t n = sel_source_count(), k;
        for (k = 0; k < n; k++) {
            const sel_source* s = sel_source_get(k);
            if (s != NULL && strcmp(s->id, "i1231bl") != 0) {
                strncat(line, s->id, sizeof line - strlen(line) - 3u);
                strncat(line, ",", sizeof line - strlen(line) - 3u);
            }
        }
        strncat(line, "\"", sizeof line - strlen(line) - 1u);
        CHECK(send_line(e, line));
    }
    sel_engine_pump(e, sink, NULL);

    /* run 1: no cache yet, the hook answers and the answer is stored */
    t0 = now_ms();
    CHECK(send_line(e, "SOURCE_SEARCH * \"jam\""));
    pump_until(e, c_done);
    live_ms = now_ms() - t0;
    CHECK(g_done && g_done_n == 2u && g_rows == 2u && g_completed == 1);
    sel_engine_stats_get(e, &st);
    CHECK(st.search_cache_hits == 0u);
    while (sel_engine_scan_busy(e)) {
        sel_engine_pump(e, sink, NULL);
        nap(1);
    }

    /* run 2: the network hangs; the stored rows must show anyway */
    g_rows = 0;
    g_done = 0;
    g_answer = 3;
    __atomic_store_n(&g_gate, 0, __ATOMIC_SEQ_CST);
    t0 = now_ms();
    CHECK(send_line(e, "SOURCE_SEARCH * \"  JAM \""));
    pump_until(e, c_rows2);
    cached_ms = now_ms() - t0;
    CHECK(g_rows == 2u);                                   /* the stored answer is on screen */
    CHECK(__atomic_load_n(&g_completed, __ATOMIC_SEQ_CST) == 1); /* sentinel: no live search answered run 2 yet */
    CHECK(!g_done);                                        /* the section is still refreshing */
    sel_engine_stats_get(e, &st);
    CHECK(st.search_cache_hits == 1u);

    /* the background refresh finishes and replaces the stored rows in place */
    __atomic_store_n(&g_gate, 1, __ATOMIC_SEQ_CST);
    pump_until(e, c_done);
    CHECK(g_calls == 2 && g_completed == 2);
    CHECK(g_done && g_done_n == 3u && g_rows == 5u);
    printf("live_ms=%ld cached_ms=%ld cache_hits=%u\n", live_ms, cached_ms, (unsigned)st.search_cache_hits);

    sel_engine_destroy(e);
    sel_library_close(lib);
    unlink(path);
    unlink(db);
    if (g_fail != 0) {
        return 1;
    }
    puts("source_search_cache_ok");
    return 0;
}
