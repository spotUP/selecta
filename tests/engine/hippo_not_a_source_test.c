/* I1064: HippoPlayer only serves shared playlists, it is not a Sources category. With the hippo source registered and its lists synced
 * into argv[1] (tests/m3/hippo_not_a_source.sh runs the offline CLI sync), the engine's SOURCES rows hold no hippo row (engine.c
 * sources_row skips SEL_SOURCE_PLAYLISTS_ONLY, I1058a) while the Playlists folder tree holds the HippoPlayer group.
 * argv[1] = library db. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/source.h>

#include "db/db.h"
#include "source.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            return 1;                                   \
        }                                               \
    } while (0)

static unsigned g_rows, g_hippo_rows;

static void sink(void* ctx, const sel_message* evt) {
    const char* key;
    (void)ctx;
    if (evt->id != SEL_EVT_LIBRARY_RESULT || evt->argc == 0 || evt->args[0].type != SEL_ARG_TAGS) return;
    key = (const char*)sel_tag_find(evt->args[0].v.t, SEL_SRC_KEY, 0);
    if (key == NULL) return;
    g_rows++;
    if (strcmp(key, "hippo") == 0) g_hippo_rows++;
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    char pool[64], tree[8192];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    uint32_t count = 0;

    if (argc < 2) return 2;
    lib = sel_library_open(host, sel_machine_probe(host), 0, argv[1]);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    sel_source_register_builtin();
    CHECK(sel_source_find("hippo") != NULL && (sel_source_find("hippo")->flags & SEL_SOURCE_PLAYLISTS_ONLY) != 0u); /* it is registered */
    CHECK(sel_engine_send_line(e, "SOURCES", pool, sizeof pool) == 0);
    sel_engine_pump(e, sink, NULL);
    CHECK(g_rows > 0u); /* the command ran and listed the other sources */
    CHECK(g_hippo_rows == 0u); /* but no Sources row for hippo */
    CHECK(sel_library_folder_tree(lib, tree, sizeof tree, &count) == SEL_FOLDER_OK && count > 0u);
    CHECK(strstr(tree, "HippoPlayer") != NULL); /* the Playlists tree holds its group */
    printf("hippo_not_a_source_ok rows=%u hippo_rows=%u folders=%u\n", g_rows, g_hippo_rows, count);
    sel_engine_destroy(e);
    sel_library_close(lib);
    return 0;
}
