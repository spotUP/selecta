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
#include "db.h"

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

static int library_browse(int argc, char** argv) {
    browse_sink sink;
    sel_engine_config cfg;
    sel_engine* e;
    char line[600];
    char pool[1024];

    if (argc < 2) {
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

/* `library stats [--machine <fake>] [--db <path>]` (T62): opens the library DB (heap, worker,
 * schema), asks the worker for stats twice and prints one sentinel line. SEL_ALLOC_FREEZE and
 * SEL_ALLOC_THAW bracket the two requests: after start the library allocates nothing. */
static int library_stats(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const sel_tag* machine = NULL;
    const char* db_path = ":memory:";
    sel_library* lib;
    sel_message r1;
    sel_message r2;
    int i;

    for (i = 0; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--machine") == 0) {
            machine = sel_machine_fake(argv[i + 1]);
            if (machine == NULL) {
                fprintf(stderr, "library stats: unknown machine %s\n", argv[i + 1]);
                return 2;
            }
        } else if (strcmp(argv[i], "--db") == 0) {
            db_path = argv[i + 1];
        } else {
            fputs("usage: selecta-cli library stats [--machine <name>] [--db <path>]\n", stderr);
            return 2;
        }
    }
    if (machine == NULL) {
        machine = sel_machine_probe(host);
    }
    lib = sel_library_open(host, machine, 0, db_path);
    if (lib == NULL) {
        puts("db_open=0");
        return 1;
    }
    fputs("SEL_ALLOC_FREEZE\n", stderr);
    fflush(stderr);
    i = sel_library_call(lib, SEL_LIB_REQ_STATS, &r1) && sel_library_call(lib, SEL_LIB_REQ_STATS, &r2);
    fputs("SEL_ALLOC_THAW\n", stderr);
    fflush(stderr);
    if (!i || r2.args[SEL_LIB_STAT_RC].v.i != 0) {
        puts("db_open=0");
        sel_library_close(lib);
        return 1;
    }
    printf("db_open=1 schema_version=%d sqlite_version=%s fts5=%d heap_bytes=%u heap_allocs=%u worker_requests=%d"
           " fts_hits=%d songs=%d account=%u\n",
           r2.args[SEL_LIB_STAT_SCHEMA].v.i, sel_library_sqlite_version(), r2.args[SEL_LIB_STAT_FTS5].v.i,
           sel_library_heap_size(lib), sel_library_heap_allocs(lib), r2.args[SEL_LIB_STAT_SERVED].v.i,
           r2.args[SEL_LIB_STAT_FTS_HITS].v.i, r2.args[SEL_LIB_STAT_SONGS].v.i, sel_library_account(lib));
    sel_library_close(lib);
    return 0;
}

int sel_cli_library(int argc, char** argv) {
    if (argc >= 1 && strcmp(argv[0], "browse") == 0) {
        return library_browse(argc, argv);
    }
    if (argc >= 1 && strcmp(argv[0], "stats") == 0) {
        return library_stats(argc - 1, argv + 1);
    }
    fputs("usage: selecta-cli library browse <dir> | stats [--machine <name>] [--db <path>]\n", stderr);
    return 2;
}
