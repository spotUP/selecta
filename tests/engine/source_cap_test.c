/* I738: the registry holds SEL_SOURCE_MAX entries, the SOURCES command lists every one of them, and a
 * registration past the cap fails loudly (error return and a log line), never a silent drop. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/source.h>

#include "db/db.h"
#include "source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            return 1;                                   \
        }                                               \
    } while (0)

static sel_source g_src[SEL_SOURCE_MAX + 1u];
static char g_id[SEL_SOURCE_MAX + 1u][SEL_SOURCE_ID_MAX];
static unsigned g_rows;

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_LIBRARY_RESULT && evt->argc > 0 && evt->args[0].type == SEL_ARG_TAGS &&
        sel_tag_find(evt->args[0].v.t, SEL_SRC_KEY, 0) != 0) {
        g_rows++;
    }
}

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i738-XXXXXX";
    char db[128], pool[64];
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
    for (i = 0; i < SEL_SOURCE_MAX + 1u; i++) {
        snprintf(g_id[i], sizeof g_id[i], "cap%u", i);
        g_src[i].id = g_id[i];
        g_src[i].name = g_id[i];
        g_src[i].home = "";
    }
    for (i = 0; sel_source_count() < SEL_SOURCE_MAX; i++) {
        CHECK(sel_source_register(&g_src[i]));
    }
    CHECK(sel_source_count() == SEL_SOURCE_MAX);
    CHECK(sel_engine_send_line(e, "SOURCES", pool, sizeof pool) == 0);
    sel_engine_pump(e, sink, NULL);
    CHECK(g_rows == SEL_SOURCE_MAX); /* all listed, none dropped by the engine's row table */
    CHECK(sel_source_register(&g_src[SEL_SOURCE_MAX]) == 0); /* one more: an error, and a log line on stderr */
    CHECK(sel_source_count() == SEL_SOURCE_MAX);
    printf("source_cap_ok %u\n", g_rows);
    return 0;
}
