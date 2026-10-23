/* `selecta-cli library browse <dir>` (T57): LIBRARY_BROWSE through the engine,
 * the same path a front end takes. Prints `dir_entries=<n>` (the directory's
 * entries; the ".." row the browse adds is not counted) and exits 1 when the
 * engine answers ERROR. */
#include <selecta/engine.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/tags.h>

#include "cli.h"
#include "local.h"

#include <stdio.h>
#include <string.h>

typedef struct browse_sink {
    unsigned entries;
    int failed;
} browse_sink;

static void on_event(void* ctx, const sel_message* evt) {
    browse_sink* s = ctx;
    if (evt->id == SEL_EVT_ERROR) {
        fprintf(stderr, "ERROR %s\n", evt->args[0].v.s);
        s->failed = 1;
    } else if (evt->id == SEL_EVT_LIBRARY_RESULT) {
        const sel_tag* tags = evt->args[0].v.t;
        const char* path = (const char*)sel_tag_find(tags, SEL_SRC_PATH, 0);
        unsigned count = (unsigned)sel_tag_find(tags, SEL_SRC_COUNT, 0);
        s->entries = count > 0 && path != NULL && !sel_browse_is_root(path) ? count - 1u : count;
    }
}

int sel_cli_library(int argc, char** argv) {
    browse_sink sink;
    sel_engine_config cfg;
    sel_engine* e;
    char line[600];
    char pool[1024];

    if (argc < 2 || strcmp(argv[0], "browse") != 0) {
        fputs("usage: selecta-cli library browse <dir>\n", stderr);
        return 2;
    }
    memset(&sink, 0, sizeof sink);
    memset(&cfg, 0, sizeof cfg);
    sel_registry_init(sel_host_default());
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.machine = sel_machine_probe(sel_host_default());
    e = sel_engine_create(sel_host_default(), &cfg);
    if (e == NULL) {
        fputs("selecta-cli: engine allocation failed\n", stderr);
        return 1;
    }
    snprintf(line, sizeof line, "LIBRARY_BROWSE %s", argv[1]);
    sel_engine_send_line(e, line, pool, sizeof pool);
    sel_engine_pump(e, on_event, &sink);
    printf("dir_entries=%u\n", sink.entries);
    sel_engine_destroy(e);
    return sink.failed ? 1 : 0;
}
