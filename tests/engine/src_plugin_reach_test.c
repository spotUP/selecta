/* I517 phase 3 reachability (plan 2026-10-26-source-plugins.md, Reachability): a source that exists only as a plugin
 * (tests/sources/testsrc) reaches the product through the engine command channel. The library walks the plugin
 * registry in sel_source_register_builtin; nothing else in core names the source.
 *   SOURCES lists testsrc with its name and kind; LIBRARY_SYNC testsrc x runs the plugin's sync (sentinel 1) and
 *   writes 3 songs through the catalogue service; SOURCES then counts 3.
 * argv[1] is the library database to use; the dui half of the test (src_plugin_reach.cmake) opens it afterwards. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/source.h>

#include "db/db.h"
#include "source.h"

#include <stdio.h>
#include <string.h>

extern unsigned sel_testsrc_sync_calls;

static unsigned g_errors, g_results, g_test_rows;
static char g_name[64], g_kind[16];
static int g_count = -1, g_enabled = -1;

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_ERROR) {
        g_errors++;
    } else if (evt->id == SEL_EVT_LIBRARY_RESULT) {
        const sel_tag* t = evt->args[0].v.t;
        const char* key = (const char*)sel_tag_find(t, SEL_SRC_KEY, 0);

        g_results++;
        if (key != NULL && strcmp(key, "testsrc") == 0) {
            g_test_rows++;
            snprintf(g_name, sizeof g_name, "%s", (const char*)sel_tag_find(t, SEL_SRC_NAME, (uintptr_t)""));
            snprintf(g_kind, sizeof g_kind, "%s", (const char*)sel_tag_find(t, SEL_SRC_KIND, (uintptr_t)""));
            g_count = (int)sel_tag_find(t, SEL_SRC_COUNT, 0);
            g_enabled = (int)sel_tag_find(t, SEL_SRC_ENABLED, 0);
        }
    }
}

static int run(sel_engine* e, const char* line) {
    char pool[256];

    g_errors = g_results = g_test_rows = 0;
    g_count = g_enabled = -1;
    g_name[0] = g_kind[0] = '\0';
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

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;

    CHECK(argc > 1);
    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, argv[1]);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);

    CHECK(sel_testsrc_sync_calls == 0);
    CHECK(run(e, "SOURCES"));
    CHECK(g_test_rows == 1 && strcmp(g_name, "Test Source") == 0 && strcmp(g_kind, "remote") == 0);
    CHECK(g_enabled == 1 && g_count == 0);

    CHECK(run(e, "LIBRARY_SYNC testsrc x 1"));
    CHECK(g_errors == 0);
    CHECK(sel_testsrc_sync_calls == 1);

    CHECK(run(e, "SOURCES"));
    CHECK(g_test_rows == 1 && g_count == 3);

    sel_engine_destroy(e);
    sel_library_close(lib);
    puts("src_plugin_reach_ok");
    return 0;
}
