/* I1400b: one slow network search source must not hold the whole search open (modulespl took 9.7 s, the spinner stayed 10-18 s).
 * Through messages and the pump: a source that stays silent for 3 s (and ignores cancel) is searched with a 300 ms
 * timeout. PASS: its section has landed (the search is no longer busy) well before the slow source returns, and the
 * slow answer is dropped. Without the timeout in search_collect the slow section is still running after 1.5 s and the test fails. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/source.h>

#include "db/db.h"
#include "source.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static atomic_int g_slow_done, g_slow_started;

static void nap(long ms) {
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static int slow_search(const sel_host* host, const char* q, sel_search_sink* out) {
    (void)host;
    (void)q;
    (void)out;
    atomic_store(&g_slow_started, 1);
    nap(3000);
    atomic_store(&g_slow_done, 1);
    return 1;
}

static const sel_source g_slow = {
    "i1400slow", "I1400 slow", "", "Stays silent.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, slow_search, NULL,
};

static unsigned g_done_events;

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
    g_done_events++;
}

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            return 1;                                   \
        }                                               \
    } while (0)

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i1400b-XXXXXX";
    char db[128], line[] = "SOURCE_SEARCH i1400slow \"enigma\"", buf[256];
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
    CHECK(sel_source_register(&g_slow));
    sel_engine_set_search_timeout_ms(e, 300);
    CHECK(sel_engine_send_line(e, line, buf, sizeof buf) == 0);
    sel_engine_pump(e, sink, NULL);
    for (i = 0; i < 100 && !atomic_load(&g_slow_started); i++) {
        sel_engine_pump(e, sink, NULL);
        nap(10);
    }
    CHECK(atomic_load(&g_slow_started) == 1);
    CHECK(sel_engine_scan_busy(e)); /* the slow search is in flight */
    for (i = 0; i < 150 && sel_engine_scan_busy(e); i++) { /* 1.5 s, half of the slow source's 3 s */
        sel_engine_pump(e, sink, NULL);
        nap(10);
    }
    CHECK(!sel_engine_scan_busy(e)); /* the search finished while the slow source still runs */
    CHECK(atomic_load(&g_slow_done) == 0);
    nap(3200);
    sel_engine_pump(e, sink, NULL); /* the late answer is joined and dropped */
    sel_engine_destroy(e);
    sel_library_close(lib);
    unlink(path);
    unlink(db);
    puts("search_source_timeout_ok");
    return 0;
}
