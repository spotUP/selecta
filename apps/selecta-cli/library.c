/* `selecta-cli library browse <dir>` (T57): LIBRARY_BROWSE through the engine,
 * the same path a front end takes. Prints `dir_entries=<n>` (the directory's
 * entries; the ".." row the browse adds is not counted) and exits 1 when the
 * engine answers ERROR. */
#include <selecta/engine.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/tags.h>

#include "catalogue.h"
#ifdef __amigaos__
#include "amiga_start.h"
#endif
#include "cli.h"
#include "local.h"
#include "db.h"
#include "query.h"
#include "source.h"
#include "sources/local_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

typedef struct search_sink {
    int failed;
    unsigned count, fts;
} search_sink;

static void on_search_event(void* ctx, const sel_message* evt) {
    search_sink* s = ctx;
    if (evt->id == SEL_EVT_ERROR) {
        printf("ERROR %s\n", evt->args[0].v.s);
        s->failed = 1;
    } else if (evt->id == SEL_EVT_LIBRARY_RESULT) {
        const sel_tag* tags = evt->args[0].v.t;
        s->count = (unsigned)sel_tag_find(tags, SEL_SRC_COUNT, 0);
        s->fts = (unsigned)sel_tag_find(tags, SEL_SRC_FTS, 0);
    }
}

/* `library search --catalogue <tsv> [--] <query>` (T64): LIBRARY_SEARCH through the engine with the
 * catalogue behind it. Prints `key=<k>` per result in key order, then `src_count=` and `fts_queries=`
 * (1: the FTS5 index served it). An engine ERROR prints `ERROR <code>` alone and exits 1. */
static int library_search(int argc, char** argv) {
    const char* catalogue = NULL;
    const char* db_path = NULL;
    const char* text = NULL;
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    search_sink sink;
    char pool[512];
    char line[600];
    uint32_t i;
    int a;

    for (a = 0; a < argc; a++) {
        if (strcmp(argv[a], "--catalogue") == 0 && a + 1 < argc) {
            catalogue = argv[++a];
        } else if (strcmp(argv[a], "--db") == 0 && a + 1 < argc) {
            db_path = argv[++a];
        } else if (strcmp(argv[a], "--") == 0 && a + 1 < argc) {
            text = argv[++a];
        } else if (text == NULL && argv[a][0] != '-') {
            text = argv[a];
        }
    }
    if ((catalogue == NULL) == (db_path == NULL) || text == NULL) {
        fputs("usage: selecta-cli library search (--catalogue <tsv> | --db <path>) [--] <query>\n", stderr);
        return 2;
    }
    if (db_path != NULL) {
        const sel_host* host = sel_host_default();

        lib = sel_library_open(host, sel_machine_probe(host), 0, db_path);
    } else {
        lib = sel_cli_open_catalogue(catalogue);
    }
    if (lib == NULL) {
        return 1;
    }
    memset(&cfg, 0, sizeof cfg);
    memset(&sink, 0, sizeof sink);
    cfg.machine = sel_machine_probe(sel_host_default());
    cfg.library = lib;
    e = sel_engine_create(sel_host_default(), &cfg);
    if (e == NULL) {
        sel_library_close(lib);
        return 1;
    }
    snprintf(line, sizeof line, "LIBRARY_SEARCH %s", text);
    sel_engine_send_line(e, line, pool, sizeof pool);
    sel_engine_pump(e, on_search_event, &sink);
    if (!sink.failed) {
        for (i = 0; i < sink.count; i++) {
            printf("key=%s\n", sel_engine_result_key(e, i));
        }
        printf("src_count=%u\nfts_queries=%u\n", sink.count, sink.fts);
    }
    sel_engine_destroy(e);
    sel_library_close(lib);
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
#ifdef __amigaos__
    printf("stack_swaps=%u stack_bytes=%lu\n", sel_amiga_stack_swaps(), sel_amiga_stack_bytes());
#endif
    sel_library_close(lib);
    return 0;
}

/* `library query --explain [--now <epoch>] [--view <name>=<query>]... [--] <query>` (T63): runs
 * the query through sel_query_compile and prints `sql=`, `params=` (s:<text> or i:<n> joined by
 * `|`, `-` when none) and `query_terms=`. An error prints `ERROR <code>` and exits 1. Running a
 * query against the DB arrives with LIBRARY_SEARCH (I149). */
#define CLI_VIEWS_MAX 8

typedef struct cli_views {
    const char* name[CLI_VIEWS_MAX];
    size_t name_len[CLI_VIEWS_MAX];
    const char* text[CLI_VIEWS_MAX];
    int count;
} cli_views;

static const char* cli_view_lookup(void* ctx, const char* name) {
    const cli_views* v = ctx;
    int i;
    for (i = 0; i < v->count; i++) {
        if (strlen(name) == v->name_len[i] && strncmp(name, v->name[i], v->name_len[i]) == 0) {
            return v->text[i];
        }
    }
    return NULL;
}

static int library_query(int argc, char** argv) {
    static sel_query q;
    cli_views views;
    long long now = 0;
    const char* text = NULL;
    int explain = 0;
    unsigned i;
    int a;

    memset(&views, 0, sizeof views);
    for (a = 0; a < argc; a++) {
        if (strcmp(argv[a], "--explain") == 0) {
            explain = 1;
        } else if (strcmp(argv[a], "--now") == 0 && a + 1 < argc) {
            now = atoll(argv[++a]);
        } else if (strcmp(argv[a], "--view") == 0 && a + 1 < argc && views.count < CLI_VIEWS_MAX) {
            const char* eq = strchr(argv[++a], '=');
            if (eq == NULL) {
                fputs("library query: --view needs name=query\n", stderr);
                return 2;
            }
            views.name[views.count] = argv[a];
            views.name_len[views.count] = (size_t)(eq - argv[a]);
            views.text[views.count++] = eq + 1;
        } else if (strcmp(argv[a], "--") == 0 && a + 1 < argc) {
            text = argv[++a];
        } else if (argv[a][0] != '-' || text == NULL) {
            text = argv[a];
        }
    }
    if (!explain || text == NULL) {
        fputs("usage: selecta-cli library query --explain [--now <epoch>] [--view <name>=<query>]... [--] <query>\n",
              stderr);
        return 2;
    }
    if (!sel_query_compile(text, now, cli_view_lookup, &views, &q)) {
        fprintf(stderr, "ERROR %s\n", q.error);
        return 1;
    }
    printf("sql=%s\nparams=", q.sql);
    if (q.nparams == 0) {
        fputs("-", stdout);
    }
    for (i = 0; i < q.nparams; i++) {
        if (q.params[i].is_int) {
            printf("%si:%lld", i ? "|" : "", q.params[i].i);
        } else {
            printf("%ss:%s", i ? "|" : "", q.params[i].s);
        }
    }
    printf("\nquery_terms=%u\n", q.terms);
    return 0;
}

/* `library scan [--db <path>] <dir>` (T71 scan part): LIBRARY_SCAN through the engine, which scans
 * <dir> with the local source through cfg.library (the handle LIBRARY_SEARCH uses). Prints
 * `scanned_files= skipped_files= scan_batches= probed= dirs= archives= archive_members= db_songs=`
 * (db_songs read back from the DB after the scan). */
static int library_scan(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const char* db_path = ":memory:";
    const char* dir = NULL;
    browse_sink sink;
    sel_scan_info info;
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    sel_message st;
    char line[1100];
    char pool[1100];
    int i;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (dir == NULL) {
            dir = argv[i];
        } else {
            dir = NULL;
            break;
        }
    }
    if (dir == NULL) {
        fputs("usage: selecta-cli library scan [--db <path>] <dir>\n", stderr);
        return 2;
    }
    memset(&sink, 0, sizeof sink);
    memset(&cfg, 0, sizeof cfg);
    sel_registry_init(host);
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.machine = sel_machine_probe(host);
    lib = sel_library_open(host, cfg.machine, 0, db_path);
    if (lib == NULL) {
        puts("db_open=0");
        return 1;
    }
    cfg.library = lib;
    e = sel_engine_create(host, &cfg);
    if (e == NULL) {
        fputs("selecta-cli: engine allocation failed\n", stderr);
        sel_library_close(lib);
        return 1;
    }
    snprintf(line, sizeof line, "LIBRARY_SCAN %s", dir);
    sel_engine_send_line(e, line, pool, sizeof pool);
    sel_engine_pump(e, on_event, &sink);
    memset(&st, 0, sizeof st);
    if (!sel_library_call(lib, SEL_LIB_REQ_STATS, &st)) {
        st.args[SEL_LIB_STAT_SONGS].v.i = -1;
    }
    sel_engine_scan_info(e, &info);
    printf("scanned_files=%u skipped_files=%u scan_batches=%u probed=%u dirs=%u archives=%u archive_members=%u "
           "db_songs=%d\n",
           (unsigned)info.scanned_files, (unsigned)info.skipped_files, (unsigned)info.scan_batches,
           (unsigned)info.probed, (unsigned)info.dirs, (unsigned)info.archives, (unsigned)info.archive_members,
           st.args[SEL_LIB_STAT_SONGS].v.i);
    sel_engine_destroy(e);
    sel_library_close(lib);
    return sink.failed ? 1 : 0;
}

/* `library key <source> <path>` (T72 key round trip): prints `key=<id>:<path>`, then the split back
 * (`key_source=` `key_path=`), and for a source with URLs `url=` and `path_in=` plus
 * `roundtrip=1` when url -> path_in -> url returns the first URL and the key splits back to the
 * input. Exit 1 when the round trip fails. */
static int library_key(int argc, char** argv) {
    char key[SEL_SONG_KEY_MAX];
    char src[SEL_SOURCE_ID_MAX];
    char path[SEL_BROWSE_PATH_MAX];
    char url[SEL_SONG_KEY_MAX + 64];
    char back[SEL_BROWSE_PATH_MAX];
    char url2[SEL_SONG_KEY_MAX + 64];
    const sel_source* source;
    int ok;

    if (argc != 2) {
        fputs("usage: selecta-cli library key <source> <path>\n", stderr);
        return 2;
    }
    sel_source_register_builtin();
    source = sel_source_find(argv[0]);
    if (source == NULL) {
        printf("ERROR unknown_source %s\n", argv[0]);
        return 1;
    }
    ok = sel_song_key(argv[0], argv[1], key, sizeof key) &&
         sel_song_key_split(key, src, sizeof src, path, sizeof path) && strcmp(src, argv[0]) == 0 &&
         strcmp(path, argv[1]) == 0;
    printf("key=%s\n", key);
    if (ok && source->url != NULL && source->path_in != NULL) {
        ok = source->url(argv[1], url, sizeof url) && source->path_in(url, back, sizeof back) &&
             strcmp(back, argv[1]) == 0 && source->url(back, url2, sizeof url2) && strcmp(url, url2) == 0;
        printf("url=%s\npath_in=%s\n", url, back);
    }
    printf("roundtrip=%d\n", ok);
    return ok ? 0 : 1;
}

int sel_cli_library(int argc, char** argv) {
    if (argc >= 1 && strcmp(argv[0], "browse") == 0) {
        return library_browse(argc, argv);
    }
    if (argc >= 1 && strcmp(argv[0], "scan") == 0) {
        return library_scan(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "key") == 0) {
        return library_key(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "stats") == 0) {
        return library_stats(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "search") == 0) {
        return library_search(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "fetch") == 0) {
        return sel_cli_library_fetch(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "query") == 0) {
        return library_query(argc - 1, argv + 1);
    }
    fputs("usage: selecta-cli library browse <dir> | search --catalogue <tsv> <q> | scan [--db <path>] <dir> | key <source> <path> | query --explain <q> | stats [--machine <name>] [--db <path>]\n", stderr);
    return 2;
}
