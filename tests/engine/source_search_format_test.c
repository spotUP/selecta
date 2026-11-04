/* I1043: a search hit keeps its whole format name. The engine used to copy it into a 16 byte buffer (SEL_SEARCH_FORMAT_MAX), cutting
 * "Fasttracker 2 (xm)" style labels at 15 characters. Through the real entry point (SOURCE_SEARCH command line in, pump, LIBRARY_RESULT
 * event out): a stand-in source returns the longest label the format table can produce (SEL_FORMAT_LABEL_MAX - 1 characters) and the
 * event's SEL_SRC_FORMAT tag must carry it unchanged. Sentinel: the hook ran and the search row event arrived.
 * With the buffer back at 16 bytes the tag is cut and the test fails. */
#include <selecta/engine.h>
#include <selecta/format_label.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/source.h>

#include "db/db.h"
#include "source.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char g_long[SEL_FORMAT_LABEL_MAX];
static char g_seen[SEL_FORMAT_LABEL_MAX + 8];
static int g_hook_ran, g_rows;

static int hook(const sel_host* host, const char* q, sel_search_sink* out) {
    (void)host;
    (void)q;
    g_hook_ran++;
    out->row(out->ctx, "0/x", "Some Tune", "Some Artist", g_long);
    return 1;
}

static const sel_source g_src = { "i1043src", "Format Stand-in", "", "I1043 stand-in.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, hook, NULL };

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_LIBRARY_RESULT) {
        const sel_tag* t = evt->args[0].v.t;

        if (strcmp((const char*)sel_tag_find(t, SEL_SRC_KIND, (uintptr_t)""), "search") == 0) {
            g_rows++;
            snprintf(g_seen, sizeof g_seen, "%s", (const char*)sel_tag_find(t, SEL_SRC_FORMAT, (uintptr_t)""));
        }
    }
}

static void nap(void) {
    struct timespec ts = { 0, 1000000L };

    nanosleep(&ts, NULL);
}

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i1043-XXXXXX";
    char db[128], pool[256];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    unsigned i;
    int fd = mkstemp(path);

    close(fd);
    snprintf(db, sizeof db, "%s.db", path);
    memset(g_long, 'F', sizeof g_long - 1u);
    g_long[sizeof g_long - 1u] = '\0';
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.queue_capacity = 8u;
    e = sel_engine_create(host, &cfg);
    if (lib == NULL || e == NULL || !sel_source_register(&g_src)) {
        puts("FAIL setup");
        return 1;
    }
    if (sel_engine_send_line(e, "SOURCE_SEARCH i1043src \"x\"", pool, sizeof pool) != 0) {
        puts("FAIL send");
        return 1;
    }
    for (i = 0; i < 5000 && g_rows == 0; i++) {
        sel_engine_pump(e, sink, NULL);
        nap();
    }
    if (g_hook_ran != 1 || g_rows != 1) {
        printf("FAIL sentinel hook_ran=%d rows=%d\n", g_hook_ran, g_rows);
        return 1;
    }
    if (strcmp(g_seen, g_long) != 0) {
        printf("FAIL format cut: sent %zu chars, got %zu ('%s')\n", strlen(g_long), strlen(g_seen), g_seen);
        return 1;
    }
    sel_engine_destroy(e);
    sel_library_close(lib);
    unlink(path);
    unlink(db);
    puts("source_search_format_ok");
    return 0;
}
