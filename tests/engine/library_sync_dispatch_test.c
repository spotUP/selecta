/* I517 phase 1: LIBRARY_SYNC goes through the source's own `sync` (plan 2026-10-26-source-plugins.md, D1, D2, D5).
 * A registered test source counts its calls, reads the host's net and catalogue services and reports stats as tags.
 * The test drives the engine command channel and asserts the call count, the argument and clock the source got, the
 * stats record, the persisted flag path and the error answers. With the old per-source if-chain in cmd_LIBRARY_SYNC
 * the call count stays 0 and the test fails. */
#include <selecta/catalogue.h>
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/net.h>
#include <selecta/source.h>

#include "db/db.h"
#include "source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned g_calls;
static unsigned g_fail;
static char g_arg[64];
static int64_t g_now;
static int g_has_net, g_has_catalogue, g_began, g_committed;
static int64_t g_cat_now;

static int test_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const sel_catalogue* cat = host->catalogue ? host->catalogue(host) : NULL;
    const sel_net* net = host->net ? host->net(host) : NULL;
    sel_catalogue_song s;

    g_calls++;
    snprintf(g_arg, sizeof g_arg, "%s", arg);
    g_now = now;
    g_has_net = net != NULL && net->get_file != NULL;
    g_has_catalogue = cat != NULL;
    if (cat != NULL) {
        g_cat_now = cat->now(host);
        memset(&s, 0, sizeof s);
        s.path = "a/b.mod";
        s.name = "b";
        s.artist = "a";
        s.format = "mod";
        g_began = cat->begin(host, "i517test");
        if (g_began) {
            cat->song(host, &s);
            g_committed = cat->commit(host);
        }
    }
    stats[0].tag = SEL_SYNC_LISTING_ROWS;
    stats[0].value = 7;
    stats[1].tag = SEL_SYNC_CL_ADDED;
    stats[1].value = 3;
    stats[2].tag = SEL_TAG_END;
    stats[2].value = 0;
    (void)cap;
    return !g_fail;
}

static const sel_source g_src = {
    "i517test", "I517 test", "", "Counts its sync calls.", NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, test_sync, SEL_SOURCE_SYNCS, NULL,
};

static unsigned g_results, g_errors;
static char g_error[64];

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_LIBRARY_RESULT) {
        g_results++;
    } else if (evt->id == SEL_EVT_ERROR) {
        g_errors++;
        snprintf(g_error, sizeof g_error, "%s", evt->args[0].v.s);
    }
}

static int run(sel_engine* e, const char* line) {
    char pool[256];

    g_results = g_errors = 0;
    g_error[0] = 0;
    if (sel_engine_send_line(e, line, pool, sizeof pool) != 0) {
        return 0;
    }
    sel_engine_pump(e, sink, NULL);
    return 1;
}

#define CHECK(c)                                                      \
    do {                                                              \
        if (!(c)) {                                                   \
            printf("FAIL line %d: %s\n", __LINE__, #c);               \
            return 1;                                                 \
        }                                                             \
    } while (0)

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i517-XXXXXX";
    char db[128];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    sel_sync_info si;
    unsigned i, seen_rows = 0, seen_added = 0;
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
    CHECK(sel_source_register(&g_src));

    /* The dispatch: one call, with the arg and clock of the command. */
    CHECK(run(e, "LIBRARY_SYNC i517test /some/dir 12345"));
    CHECK(g_calls == 1);
    CHECK(strcmp(g_arg, "/some/dir") == 0);
    CHECK(g_now == 12345);
    CHECK(g_cat_now == 12345);
    CHECK(g_has_net && g_has_catalogue && g_began && g_committed);
    CHECK(g_results == 1 && g_errors == 0);

    /* The stats travel as tags and the named fields are filled from them. */
    sel_engine_sync_info(e, &si);
    for (i = 0; si.stats[i].tag != SEL_TAG_END; i++) {
        seen_rows += si.stats[i].tag == SEL_SYNC_LISTING_ROWS && si.stats[i].value == 7;
        seen_added += si.stats[i].tag == SEL_SYNC_CL_ADDED && si.stats[i].value == 3;
    }
    CHECK(seen_rows == 1 && seen_added == 1);
    CHECK(si.listing_rows == 7 && si.cl_added == 3);

    /* A failing sync answers sync_failed. */
    g_fail = 1;
    CHECK(run(e, "LIBRARY_SYNC i517test /x 1"));
    CHECK(g_calls == 2 && g_errors == 1 && strcmp(g_error, "sync_failed") == 0 && g_results == 0);

    /* No sync, no source: unknown_source and no call. */
    CHECK(run(e, "LIBRARY_SYNC local /x 1"));
    CHECK(g_errors == 1 && strcmp(g_error, "unknown_source") == 0);
    CHECK(run(e, "LIBRARY_SYNC nosuch /x 1"));
    CHECK(g_errors == 1 && strcmp(g_error, "unknown_source") == 0);
    CHECK(g_calls == 2);

    sel_engine_destroy(e);
    sel_library_close(lib);
    unlink(path);
    unlink(db);
    puts("library_sync_dispatch_ok");
    return 0;
}
