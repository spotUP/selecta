/* I1019a: SOURCE_SEARCH * "<q>" fans out over every enabled searchable source, one library task per source, in registry order.
 * Four stand-in sources with a search hook (a: two hits, b: disabled by the library.sources_off pref, c: unreachable, d: empty)
 * plus the real modulespl, switched off by the same pref so no network is touched. Through the real entry point (message in,
 * pump out) the test proves: the hooks ran in registry order (sentinel: the call log), the disabled source never ran, each
 * source's hits and its search_done count arrive (a: 2, d: 0), the unreachable one reports search_failed and the others still
 * answer; then a newer query sent while the first query's task blocks supersedes it: the stale task's hits never reach the
 * event stream (generation check in search_collect), its queued sources never run, and the new query answers in full.
 * I1035: seven searchable sources are registered (five enabled): the fifth enabled one still answers (the old cap of 4 left it out).
 * With the generation check removed the stale hits (path ".../slow") arrive and the test fails. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/source.h>

#include "db/db.h"
#include "source.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static atomic_int g_in_slow, g_release;
static char g_log[256]; /* "<id>:<query>;" per hook call, in call order; written by the job thread, read after it ended */
static char g_stale_seen;
static char g_done_log[256];
static unsigned g_failed, g_rows_a;

static void nap(long ms) {
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = ms * 1000000L;
    nanosleep(&ts, NULL);
}

static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

static void log_call(const char* id, const char* q) { /* I1044: the hooks run on parallel threads */
    size_t n;

    pthread_mutex_lock(&g_log_lock);
    n = strlen(g_log);
    snprintf(g_log + n, sizeof g_log - n, "%s:%s;", id, q);
    pthread_mutex_unlock(&g_log_lock);
}

/* 1 when `log` holds exactly the ';'-terminated tokens of `want` (a ';'-terminated list), each once, in any order. */
static int same_tokens(const char* log, const char* want) {
    char tok[64];
    const char* p = want;

    if (strlen(log) != strlen(want)) return 0;
    while (*p != '\0') {
        const char* end = strchr(p, ';');
        size_t n = (size_t)(end - p) + 1u;
        const char* hit;

        memcpy(tok, p, n);
        tok[n] = '\0';
        hit = strstr(log, tok);
        if (hit == NULL || strstr(hit + 1, tok) != NULL || (hit != log && hit[-1] != ';')) return 0;
        p += n;
    }
    return 1;
}

/* I1044: every source runs at once, so for the "slow" query all of them hold until released (none lands before the newer query). */
static void hold_if_slow(const char* q) {
    int waited = 0;

    while (strcmp(q, "slow") == 0 && !atomic_load(&g_release) && waited < 3000) {
        nap(1);
        waited++;
    }
}

static int hook_a(const sel_host* host, const char* q, sel_search_sink* out) {
    char p[64];
    int i;

    (void)host;
    log_call("i1019a", q);
    if (strcmp(q, "slow") == 0) { /* stands in for a network wait */
        int waited = 0;

        atomic_store(&g_in_slow, 1);
        while (!atomic_load(&g_release) && waited < 3000) {
            nap(1);
            waited++;
        }
    }
    for (i = 0; i < 2; i++) {
        snprintf(p, sizeof p, "%d/%s", i, q);
        if (!out->row(out->ctx, p, "Title", "Artist", "mod")) break;
    }
    return 2;
}

static int hook_c(const sel_host* host, const char* q, sel_search_sink* out) {
    (void)host;
    (void)out;
    log_call("i1019c", q);
    hold_if_slow(q);
    return -1;
}

static int hook_d(const sel_host* host, const char* q, sel_search_sink* out) {
    (void)host;
    (void)out;
    log_call("i1019d", q);
    hold_if_slow(q);
    return 0;
}

/* I1035: two more searchable sources, so the enabled count (a, c, d, e, f = 5, plus the disabled b and modulespl) passes the old fixed
 * cap of 4. The fifth enabled source in registry order is i1019f; with a cap of 4 its bucket is never made and it never runs. */
static int hook_e(const sel_host* host, const char* q, sel_search_sink* out) {
    (void)host;
    log_call("i1019e", q);
    hold_if_slow(q);
    return out->row(out->ctx, "e/1", "Title", "Artist", "mod") ? 1 : 0;
}

static int hook_f(const sel_host* host, const char* q, sel_search_sink* out) {
    (void)host;
    log_call("i1019f", q);
    hold_if_slow(q);
    return out->row(out->ctx, "f/1", "Title", "Artist", "mod") ? 1 : 0;
}

static int hook_b(const sel_host* host, const char* q, sel_search_sink* out) {
    (void)host;
    (void)out;
    log_call("i1019b", q);
    return 0;
}

#define SRC(ID, HOOK) { ID, ID, "", "I1019a stand-in.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, HOOK, NULL }
static const sel_source g_a = SRC("i1019a", hook_a), g_b = SRC("i1019b", hook_b), g_c = SRC("i1019c", hook_c), g_d = SRC("i1019d", hook_d),
                       g_e = SRC("i1019e", hook_e), g_f = SRC("i1019f", hook_f);

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_ERROR) {
        g_failed += evt->argc > 0 && strcmp(evt->args[0].v.s, "search_failed") == 0;
    } else if (evt->id == SEL_EVT_LIBRARY_RESULT) {
        const sel_tag* t = evt->args[0].v.t;
        const char* kind = (const char*)sel_tag_find(t, SEL_SRC_KIND, (uintptr_t)"");
        const char* key = (const char*)sel_tag_find(t, SEL_SRC_KEY, (uintptr_t)"");
        const char* path = (const char*)sel_tag_find(t, SEL_SRC_PATH, (uintptr_t)"");

        if (strcmp(kind, "search") == 0) {
            if (strstr(path, "/slow") != NULL) g_stale_seen = 1;
            g_rows_a += strcmp(key, "i1019a") == 0;
        } else if (strcmp(kind, "search_done") == 0) {
            size_t n = strlen(g_done_log);

            snprintf(g_done_log + n, sizeof g_done_log - n, "%s=%u;", key, (unsigned)sel_tag_find(t, SEL_SRC_COUNT, 0));
        }
    }
}

static int send_line(sel_engine* e, const char* line) {
    static char pool[8][256];
    static unsigned next;

    return sel_engine_send_line(e, line, pool[next++ & 7u], sizeof pool[0]) == 0;
}

static void drain(sel_engine* e) {
    unsigned i;

    sel_engine_pump(e, sink, NULL); /* dispatches the queued command */
    for (i = 0; i < 5000 && sel_engine_scan_busy(e); i++) {
        sel_engine_pump(e, sink, NULL);
        nap(1);
    }
    sel_engine_pump(e, sink, NULL);
}

#define CHECK(c)                                                      \
    do {                                                              \
        if (!(c)) {                                                   \
            printf("FAIL line %d: %s\n  log=%s\n  done=%s\n", __LINE__, #c, g_log, g_done_log); \
            return 1;                                                 \
        }                                                             \
    } while (0)

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i1019a-XXXXXX";
    char db[128];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    unsigned i;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    close(fd);
    snprintf(db, sizeof db, "%s.db", path);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    sel_source_register_builtin();
    CHECK(sel_source_register(&g_a) && sel_source_register(&g_b) && sel_source_register(&g_c) && sel_source_register(&g_d) &&
          sel_source_register(&g_e) && sel_source_register(&g_f)); /* registered after the engine started: the buckets grow at the query */
    CHECK(send_line(e, "PREF_SET library.sources_off \"modulespl,i1019b\""));
    sel_engine_pump(e, sink, NULL);

    /* Order, disabled skipped, failed and empty buckets. */
    CHECK(send_line(e, "SOURCE_SEARCH * \"abc\""));
    drain(e);
    CHECK(same_tokens(g_log, "i1019a:abc;i1019c:abc;i1019d:abc;i1019e:abc;i1019f:abc;")); /* I1044: all start at once, order is arrival; i1019b (disabled) and modulespl never ran */
    CHECK(g_rows_a == 2);
    CHECK(same_tokens(g_done_log, "i1019a=2;i1019d=0;i1019e=1;i1019f=1;")); /* the unreachable source gives no search_done */
    CHECK(g_failed == 1);

    /* Cancel mid-search: a new query while the first query's task blocks. */
    g_log[0] = '\0';
    g_done_log[0] = '\0';
    g_rows_a = 0;
    g_failed = 0;
    CHECK(send_line(e, "SOURCE_SEARCH * \"slow\""));
    sel_engine_pump(e, sink, NULL);
    for (i = 0; i < 2000 && !atomic_load(&g_in_slow); i++) nap(1);
    CHECK(atomic_load(&g_in_slow) == 1);
    CHECK(send_line(e, "SOURCE_SEARCH * \"fast\""));
    sel_engine_pump(e, sink, NULL);
    atomic_store(&g_release, 1);
    drain(e);
    CHECK(g_stale_seen == 0); /* the stale task's hits were dropped */
    CHECK(same_tokens(g_log, "i1019a:slow;i1019c:slow;i1019d:slow;i1019e:slow;i1019f:slow;i1019a:fast;i1019c:fast;i1019d:fast;i1019e:fast;i1019f:fast;")); /* I1044: the stale query's sources all started at once; none of their answers landed */
    CHECK(same_tokens(g_done_log, "i1019a=2;i1019d=0;i1019e=1;i1019f=1;") && g_rows_a == 2);
    CHECK(g_failed == 1);

    sel_engine_destroy(e);
    sel_library_close(lib);
    unlink(path);
    unlink(db);
    puts("source_search_all_ok");
    return 0;
}
