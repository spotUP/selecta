/* I906a: the debug UI-wait guard (design 2026-11-01-ui-thread-library-waits.md section 3). A thread marked with
 * sel_library_watch(1) that makes a blocking library request prints `library_ui_wait: req=<id>` plus a backtrace and is counted
 * (sel_library_ui_guard_hits). In nowait mode (the request defers) and inside sel_library_ui_allow(1) the guard stays quiet; the
 * allowed wait is counted separately. Fixture: a thread holds the library call lock, like a running sync job, so the watched
 * request really waits. Release builds compile the guard out: the test then only reports ok. */
#include <pthread.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/registry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"

enum { HOLD_MS = 200 };

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void* holder(void* arg) {
    sel_library* lib = arg;

    pthread_mutex_lock(&lib->call_lock);
    usleep(HOLD_MS * 1000);
    pthread_mutex_unlock(&lib->call_lock);
    return NULL;
}

/* One library request while another thread owns the call lock. nowait: the request defers instead of waiting. */
static void busy_request(sel_library* lib, int nowait) {
    sel_message reply;
    pthread_t th;
    int was;

    pthread_create(&th, NULL, holder, lib);
    usleep(50 * 1000);
    was = sel_library_nowait(nowait);
    (void)sel_library_call(lib, SEL_LIB_REQ_STATS, &reply);
    sel_library_nowait(was);
    pthread_join(th, NULL);
}

static int err_has(const char* path, const char* needle) {
    char buf[65536];
    size_t n;
    FILE* f = fopen(path, "r");

    if (f == NULL) return 0;
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    return strstr(buf, needle) != NULL;
}

int main(void) {
#if defined(NDEBUG) || defined(SEL_LIBRARY_NO_THREAD)
    printf("library_ui_guard_ok (release: guard compiled out)\n");
    return 0;
#else
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_uiguard_XXXXXX";
    char db[256], err[300];
    sel_library* lib;
    uint32_t h0, a0;
    int was;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    snprintf(err, sizeof err, "%s/err.txt", dir);
    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    setenv("SELECTA_LIB_UI_GUARD", "log", 1);
    CHECK(freopen(err, "w", stderr) != NULL);

    /* not watched: silent */
    h0 = sel_library_ui_guard_hits();
    busy_request(lib, 0);
    CHECK(sel_library_ui_guard_hits() == h0);

    /* watched, blocking, worker busy: printed and counted */
    sel_library_watch(1);
    busy_request(lib, 0);
    CHECK(sel_library_ui_guard_hits() == h0 + 1);
    fflush(stderr);
    CHECK(err_has(err, "library_ui_wait: req="));

    /* nowait: the request defers, no hit */
    busy_request(lib, 1);
    CHECK(sel_library_ui_guard_hits() == h0 + 1);

    /* allowed: counted as an allowed wait, not a hit */
    a0 = sel_library_allowed_waits();
    was = sel_library_ui_allow(1);
    busy_request(lib, 0);
    sel_library_ui_allow(was);
    CHECK(sel_library_ui_guard_hits() == h0 + 1);
    CHECK(sel_library_allowed_waits() == a0 + 1);

    sel_library_watch(0);
    sel_library_close(lib);
    if (g_fail == 0) printf("library_ui_guard_ok\n");
    return g_fail ? 1 : 0;
#endif
}
