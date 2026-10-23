/* `selecta-cli library browse <dir>` (T57): LIBRARY_BROWSE through the engine,
 * the same path a front end takes. Prints `dir_entries=<n>` (the directory's
 * entries; the ".." row the browse adds is not counted) and exits 1 when the
 * engine answers ERROR. */
#include <selecta/engine.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/tags.h>

#include "composer.h"
#include "cache.h"
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
#include "watch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef __amigaos__
#include <sqlite3.h>
#endif
#if !SEL_PATH_STYLE_AMIGA
#include <unistd.h>
#endif

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

/* `library meta [--db <path>] <key>` (I161a): prints `meta.<key>=<value>` from the DB meta table, empty when unset. */
static int library_meta(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const char* db_path = ":memory:";
    const char* key = NULL;
    char value[128];
    sel_meta_op op;
    sel_message reply;
    sel_library* lib;
    int i;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (key == NULL) {
            key = argv[i];
        } else {
            key = NULL;
            break;
        }
    }
    if (key == NULL) {
        fputs("usage: selecta-cli library meta [--db <path>] <key>\n", stderr);
        return 2;
    }
    lib = sel_library_open(host, sel_machine_probe(host), 0, db_path);
    if (lib == NULL) {
        puts("db_open=0");
        return 1;
    }
    value[0] = '\0';
    op.key = key;
    op.value = value;
    op.cap = sizeof value;
    op.set = 0;
    memset(&reply, 0, sizeof reply);
    i = sel_library_request(lib, SEL_LIB_REQ_META, &op, &reply) && reply.args[1].v.i == 0;
    printf("meta.%s=%s\n", key, value);
    sel_library_close(lib);
    return i ? 0 : 1;
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
typedef struct rescan_dirs {
    char path[SEL_TRACK_DIRS][SEL_BROWSE_PATH_MAX];
    const char* list[SEL_TRACK_DIRS];
    uint32_t count;
} rescan_dirs;

static void collect_dir(void* ctx, const char* dir) {
    rescan_dirs* r = ctx;

    if (r->count < SEL_TRACK_DIRS) {
        snprintf(r->path[r->count], sizeof r->path[0], "%s", dir);
        r->list[r->count] = r->path[r->count];
        r->count++;
    }
}

/* The I160 part of `library scan`: after the first scan, `--watch <ms>` (folder watcher) or `--poll <ms>`
 * (directory mtimes, the Amiga path) waits at most <ms> for a change and rescans just the changed
 * directories through sel_source_rescan. Prints `watch_ready=1` once the watcher is armed so a test can
 * touch the tree deterministically. Fills rescanned_dirs, rescan_files and watch_events. */
static void library_watch_rescan(const sel_local_scan_config* scfg, const char* dir, uint32_t ms, int use_poll,
                                 sel_local_scan_stats* rs, uint32_t* events) {
    static rescan_dirs r;
    sel_watch* w = use_poll ? NULL : sel_watch_open(dir);

    memset(rs, 0, sizeof *rs);
    r.count = 0;
    if (w != NULL) {
        puts("watch_ready=1");
        fflush(stdout);
        *events = sel_watch_poll(w, ms, collect_dir, &r);
        sel_watch_close(w);
        if (r.count > 0) {
            sel_source_rescan(scfg, r.list, r.count, rs);
        }
        return;
    }
#if !SEL_PATH_STYLE_AMIGA
    {
        sel_local_tracker* t = sel_local_track(scfg, dir);
        uint32_t waited;

        if (t != NULL) {
            puts("watch_ready=1");
            fflush(stdout);
            for (waited = 0; waited < ms; waited += 50u) {
                usleep(50000);
                if (sel_local_poll_changed(scfg, t, rs) > 0) {
                    break;
                }
            }
            sel_local_untrack(scfg, t);
        }
    }
#endif
}

/* `library rescan [--db <path>] <dir>...` (I160): the manual rescan, flat per directory, same entry the
 * watcher and the Amiga timer use. Prints `rescanned_dirs= rescan_files= db_songs=`. */
static int library_rescan(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const char* db_path = ":memory:";
    const char* dirs[SEL_TRACK_DIRS];
    uint32_t n = 0;
    sel_local_scan_config scfg;
    sel_local_scan_stats rs;
    sel_library* lib;
    sel_message st;
    int i, ok;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (n < SEL_TRACK_DIRS) {
            dirs[n++] = argv[i];
        }
    }
    if (n == 0) {
        fputs("usage: selecta-cli library rescan [--db <path>] <dir>...\n", stderr);
        return 2;
    }
    sel_registry_init(host);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db_path);
    if (lib == NULL) {
        puts("db_open=0");
        return 1;
    }
    memset(&scfg, 0, sizeof scfg);
    scfg.host = host;
    scfg.lib = lib;
    scfg.registry_count = sel_registry_count;
    scfg.registry_get = sel_registry_get;
    scfg.now = (int64_t)time(NULL);
    ok = sel_source_rescan(&scfg, dirs, n, &rs);
    memset(&st, 0, sizeof st);
    if (!sel_library_call(lib, SEL_LIB_REQ_STATS, &st)) {
        st.args[SEL_LIB_STAT_SONGS].v.i = -1;
    }
    printf("rescanned_dirs=%u rescan_files=%u db_songs=%d\n", (unsigned)rs.rescanned_dirs,
           (unsigned)rs.scanned_files, st.args[SEL_LIB_STAT_SONGS].v.i);
    sel_library_close(lib);
    return ok ? 0 : 1;
}

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
    sel_local_scan_config scfg;
    sel_local_scan_stats rs;
    uint32_t watch_ms = 0, watch_events = 0;
    int use_poll = 0;
    int i;

    memset(&rs, 0, sizeof rs);
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if ((strcmp(argv[i], "--watch") == 0 || strcmp(argv[i], "--poll") == 0) && i + 1 < argc) {
            use_poll = argv[i][2] == 'p';
            watch_ms = (uint32_t)strtoul(argv[++i], NULL, 10);
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
    sel_engine_scan_info(e, &info);
    if (watch_ms > 0) {
        memset(&scfg, 0, sizeof scfg);
        scfg.host = host;
        scfg.lib = lib;
        scfg.registry_count = sel_registry_count;
        scfg.registry_get = sel_registry_get;
        scfg.now = (int64_t)time(NULL);
        library_watch_rescan(&scfg, dir, watch_ms, use_poll, &rs, &watch_events);
    }
    memset(&st, 0, sizeof st);
    if (!sel_library_call(lib, SEL_LIB_REQ_STATS, &st)) {
        st.args[SEL_LIB_STAT_SONGS].v.i = -1;
    }
    printf("rescanned_dirs=%u rescan_files=%u watch_events=%u ", (unsigned)rs.rescanned_dirs,
           (unsigned)rs.scanned_files, (unsigned)watch_events);
    printf("scanned_files=%u skipped_files=%u scan_batches=%u probed=%u dirs=%u archives=%u archive_members=%u "
           "archive_lzx=%u depack_pp=%u db_songs=%d\n",
           (unsigned)info.scanned_files, (unsigned)info.skipped_files, (unsigned)info.scan_batches,
           (unsigned)info.probed, (unsigned)info.dirs, (unsigned)info.archives, (unsigned)info.archive_members,
           (unsigned)info.archive_lzx, (unsigned)info.depack_pp, st.args[SEL_LIB_STAT_SONGS].v.i);
    sel_engine_destroy(e);
    sel_library_close(lib);
    return sink.failed ? 1 : 0;
}

#ifndef __amigaos__
/* `--dump`: one `row=<path>|<format>|<artist>|<md5>` line per modland files row, read straight from the
 * database file after the engine is closed (T72 compares them with the golden). */
static void library_dump_modland(const char* db_path) {
    sqlite3* db = NULL;
    sqlite3_stmt* st = NULL;

    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "SELECT path, COALESCE(format,''), COALESCE(artist,''), COALESCE(md5,'') FROM files"
                               " WHERE source='modland' ORDER BY path", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            printf("row=%s|%s|%s|%s\n", (const char*)sqlite3_column_text(st, 0),
                   (const char*)sqlite3_column_text(st, 1), (const char*)sqlite3_column_text(st, 2),
                   (const char*)sqlite3_column_text(st, 3));
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
}

/* `--dump` for hvsc: `hvsc_row=<path>|<format>|<md5>|<duration_ms>|<subtunes>|<lengths_ms>|<url>` per song and
 * `hvsc_sticker=<path>|<name>|<value>` per STIL note (T73). */
static void library_dump_hvsc(const char* db_path) {
    sqlite3* db = NULL;
    sqlite3_stmt* st = NULL;
    const sel_source* src;
    char url[SEL_BROWSE_PATH_MAX];

    sel_source_register_builtin();
    src = sel_source_find("hvsc");
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return;
    }
    if (sqlite3_prepare_v2(db,
                           "SELECT s.path, COALESCE(s.format,''), COALESCE(s.md5,''), COALESCE(s.duration_ms,0),"
                           " COALESCE((SELECT value FROM stickers k WHERE k.key=s.key AND k.name='subtunes'),''),"
                           " COALESCE((SELECT value FROM stickers k WHERE k.key=s.key AND k.name='lengths_ms'),'')"
                           " FROM songs s WHERE s.source='hvsc' ORDER BY s.path", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char* path = (const char*)sqlite3_column_text(st, 0);

            if (src == NULL || src->url == NULL || !src->url(path, url, sizeof url)) {
                url[0] = '\0';
            }
            printf("hvsc_row=%s|%s|%s|%d|%s|%s|%s\n", path, (const char*)sqlite3_column_text(st, 1),
                   (const char*)sqlite3_column_text(st, 2), sqlite3_column_int(st, 3),
                   (const char*)sqlite3_column_text(st, 4), (const char*)sqlite3_column_text(st, 5), url);
        }
    }
    sqlite3_finalize(st);
    st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT s.path, k.name, k.value FROM stickers k JOIN songs s ON s.key=k.key"
                               " WHERE s.source='hvsc' AND k.name LIKE 'stil.%' ORDER BY s.path, k.name",
                           -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            printf("hvsc_sticker=%s|%s|%s\n", (const char*)sqlite3_column_text(st, 0),
                   (const char*)sqlite3_column_text(st, 1), (const char*)sqlite3_column_text(st, 2));
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
}
/* `--dump` for hippo: `hippo_row=<path>|<format>|<name>|<artist>|<genre>|<url>|<hippo_id>|<prg_entries>|<prg_dividers>|
 * <prg_stealth>|<prg_subsongs>` per playlist (T74). */
static void library_dump_hippo(const char* db_path) {
    sqlite3* db = NULL;
    sqlite3_stmt* st = NULL;
    const sel_source* src;
    char url[SEL_BROWSE_PATH_MAX];

    sel_source_register_builtin();
    src = sel_source_find("hippo");
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return;
    }
    if (sqlite3_prepare_v2(db,
                           "SELECT s.path, COALESCE(s.format,''), COALESCE(s.name,''), COALESCE(s.artist,''),"
                           " COALESCE((SELECT value FROM stickers k WHERE k.key=s.key AND k.name='genre'),''),"
                           " COALESCE((SELECT value FROM stickers k WHERE k.key=s.key AND k.name='hippo_id'),''),"
                           " COALESCE((SELECT value FROM stickers k WHERE k.key=s.key AND k.name='prg_entries'),''),"
                           " COALESCE((SELECT value FROM stickers k WHERE k.key=s.key AND k.name='prg_dividers'),''),"
                           " COALESCE((SELECT value FROM stickers k WHERE k.key=s.key AND k.name='prg_stealth'),''),"
                           " COALESCE((SELECT value FROM stickers k WHERE k.key=s.key AND k.name='prg_subsongs'),'')"
                           " FROM songs s WHERE s.source='hippo' ORDER BY s.path", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char* path = (const char*)sqlite3_column_text(st, 0);

            if (src == NULL || src->url == NULL || !src->url(path, url, sizeof url)) {
                url[0] = '\0';
            }
            printf("hippo_row=%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n", path, (const char*)sqlite3_column_text(st, 1),
                   (const char*)sqlite3_column_text(st, 2), (const char*)sqlite3_column_text(st, 3),
                   (const char*)sqlite3_column_text(st, 4), url, (const char*)sqlite3_column_text(st, 5),
                   (const char*)sqlite3_column_text(st, 6), (const char*)sqlite3_column_text(st, 7),
                   (const char*)sqlite3_column_text(st, 8), (const char*)sqlite3_column_text(st, 9));
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
}
#endif

/* `library sync [--db <p>] [--now <epoch>] [--dump] modland <dir>` (I161, T72): LIBRARY_SYNC through the
 * engine. <dir> holds allmods_md5.txt and changelog_YYYY-MM-DD.txt files. Prints
 * `listing_rows= new_this_week= ...` and `db_songs=`. */
static int library_sync(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const char* db_path = ":memory:";
    const char* src = NULL;
    const char* dir = NULL;
    const char* now = NULL;
    int dump = 0;
    browse_sink sink;
    sel_sync_info info;
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
        } else if (strcmp(argv[i], "--now") == 0 && i + 1 < argc) {
            now = argv[++i];
        } else if (strcmp(argv[i], "--dump") == 0) {
            dump = 1;
        } else if (src == NULL) {
            src = argv[i];
        } else if (dir == NULL) {
            dir = argv[i];
        } else {
            src = NULL;
            break;
        }
    }
    if (src == NULL || dir == NULL) {
        fputs("usage: selecta-cli library sync [--db <path>] [--now <epoch>] [--dump] modland|hvsc|hippo <dir> | modland <http(s) url>\n", stderr);
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
    if (now != NULL) {
        snprintf(line, sizeof line, "LIBRARY_SYNC %s %s %s", src, dir, now);
    } else {
        snprintf(line, sizeof line, "LIBRARY_SYNC %s %s", src, dir);
    }
    sel_engine_send_line(e, line, pool, sizeof pool);
    sel_engine_pump(e, on_event, &sink);
    sel_engine_sync_info(e, &info);
    memset(&st, 0, sizeof st);
    if (!sel_library_call(lib, SEL_LIB_REQ_STATS, &st)) {
        st.args[SEL_LIB_STAT_SONGS].v.i = -1;
    }
    if (strcmp(src, "hippo") == 0) {
        printf("hippo_lists=%u skipped_rows=%u prg_files=%u prg_entries=%u prg_dividers=%u prg_stealth=%u "
               "prg_subsongs=%u db_songs=%d\n",
               (unsigned)info.hippo_lists, (unsigned)info.skipped_rows, (unsigned)info.prg_files,
               (unsigned)info.prg_entries, (unsigned)info.prg_dividers, (unsigned)info.prg_stealth,
               (unsigned)info.prg_subsongs, st.args[SEL_LIB_STAT_SONGS].v.i);
    } else if (strcmp(src, "hvsc") == 0) {
        printf("listing_rows=%u skipped_rows=%u songlengths=%u subtunes=%u stil_entries=%u db_songs=%d\n",
               (unsigned)info.listing_rows, (unsigned)info.skipped_rows, (unsigned)info.songlengths,
               (unsigned)info.subtunes, (unsigned)info.stil_entries, st.args[SEL_LIB_STAT_SONGS].v.i);
    } else
    printf("listing_rows=%u new_this_week=%u skipped_rows=%u changelogs=%u cl_added=%u cl_removed=%u cl_moved=%u "
           "cl_updated=%u db_songs=%d\n",
           (unsigned)info.listing_rows, (unsigned)info.new_this_week, (unsigned)info.skipped_rows,
           (unsigned)info.changelogs, (unsigned)info.cl_added, (unsigned)info.cl_removed, (unsigned)info.cl_moved,
           (unsigned)info.cl_updated, st.args[SEL_LIB_STAT_SONGS].v.i);
    if (strcmp(src, "modland") == 0 && info.net_requests > 0) {
        printf("net_requests=%u net_zip_bytes=%u net_listing_bytes=%u net_changelogs_fetched=%u "
               "net_changelogs_missing=%u\n",
               (unsigned)info.net_requests, (unsigned)info.net_zip_bytes, (unsigned)info.net_listing_bytes,
               (unsigned)info.net_changelogs_fetched, (unsigned)info.net_changelogs_missing);
    }
    sel_engine_destroy(e);
    sel_library_close(lib);
#ifndef __amigaos__
    if (dump && strcmp(db_path, ":memory:") != 0) {
        if (strcmp(src, "hippo") == 0) {
            library_dump_hippo(db_path);
        } else if (strcmp(src, "hvsc") == 0) {
            library_dump_hvsc(db_path);
        } else {
            library_dump_modland(db_path);
        }
    }
#else
    (void)dump;
#endif
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

/* `library trim [--budget <n>] [--machine <fake>] [--db <path>] [--mark kept|liked:<key>]...` (T76, I167):
 * opens the library, marks the given songs and trims the cache under $SELECTA_CACHE_DIR through
 * sel_library_cache_trim, the call every LIBRARY_FETCH makes after storing a file. Without
 * --budget the machine's default applies. */
static int library_trim(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const sel_tag* machine = NULL;
    const char* db_path = ":memory:";
    uint32_t budget = 0;
    const char* rules = NULL;
    sel_library* lib;
    sel_cache_trim_result tr;
    int i;

    for (i = 0; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--machine") == 0) {
            machine = sel_machine_fake(argv[i + 1]);
            if (machine == NULL) {
                fprintf(stderr, "library trim: unknown machine %s\n", argv[i + 1]);
                return 2;
            }
        } else if (strcmp(argv[i], "--db") == 0) {
            db_path = argv[i + 1];
        } else if (strcmp(argv[i], "--rules") == 0) {
            rules = argv[i + 1];
        } else if (strcmp(argv[i], "--budget") == 0) {
            budget = (uint32_t)strtoul(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--mark") != 0) {
            fputs("usage: selecta-cli library trim [--budget <n>] [--machine <name>] [--db <path>] [--rules <companion rules>] [--mark kept:<key>|liked:<key>]\n", stderr);
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
    for (i = 0; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--mark") == 0) {
            char table[8];
            const char* colon = strchr(argv[i + 1], ':');
            if (colon == NULL || (size_t)(colon - argv[i + 1]) >= sizeof table) {
                continue;
            }
            memcpy(table, argv[i + 1], (size_t)(colon - argv[i + 1]));
            table[colon - argv[i + 1]] = '\0';
            if (!sel_library_mark(lib, table, colon + 1)) {
                fprintf(stderr, "library trim: cannot mark %s\n", argv[i + 1]);
            }
        }
    }
    sel_library_set_rules(lib, rules);
    printf("budget=%lu\n", (unsigned long)(budget != 0 ? budget : sel_cache_budget(machine)));
    printf("trim_runs=%u\n", sel_library_cache_trim(lib, budget, &tr));
    printf("bytes_before=%lu\nbytes_after=%lu\ntrimmed=%u kept_exempt=%u companion_exempt=%u\n",
           (unsigned long)tr.bytes_before, (unsigned long)tr.bytes_after, tr.trimmed, tr.kept_exempt,
           tr.companion_exempt);
    sel_library_close(lib);
    return 0;
}

int sel_cli_wrong(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const char* db_path = ":memory:";
    const char* export_path = NULL;
    const char* song = NULL;
    const char* note = NULL;
    uint32_t rows = 0;
    uint32_t exported = 0;
    sel_library* lib;
    int i, ok = 1;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (strcmp(argv[i], "--export") == 0 && i + 1 < argc) {
            export_path = argv[++i];
        } else if (song == NULL && argv[i][0] != '-') {
            song = argv[i];
        } else if (note == NULL && argv[i][0] != '-') {
            note = argv[i];
        } else {
            song = NULL;
            export_path = NULL;
            break;
        }
    }
    if (song == NULL && export_path == NULL) {
        fputs("usage: selecta-cli wrong [--db <path>] [--export <sounds-wrong.txt>] [<song> [<note>]]\n", stderr);
        return 2;
    }
    sel_registry_init(host);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db_path);
    if (lib == NULL) {
        puts("db_open=0");
        return 1;
    }
    if (song != NULL) {
        ok = sel_library_flag(lib, song, "wrong", note != NULL ? note : "", &rows);
    }
    if (ok && export_path != NULL) {
        ok = sel_library_flags_export(lib, export_path, &exported);
    }
    printf("flags=%u wrong_exported=%u\n", (unsigned)rows, (unsigned)exported);
    sel_library_close(lib);
    return ok ? 0 : 1;
}

int sel_cli_userdata(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const char* db_path = NULL;
    const char* catalogue = NULL;
    uint32_t history = 0, flags = 0, notes = 0, pending = 0, rows = 0;
    sel_library* lib;
    int i, ok = 1;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (strcmp(argv[i], "--catalogue") == 0 && i + 1 < argc) {
            catalogue = argv[++i];
        } else if (strcmp(argv[i], "--played") == 0 && i + 1 < argc) {
            i += 1;
        } else if ((strcmp(argv[i], "--note") == 0 || strcmp(argv[i], "--wrong") == 0) && i + 2 < argc) {
            i += 2;
        } else if (strcmp(argv[i], "--tag") == 0 && i + 3 < argc) {
            i += 3;
        } else {
            db_path = NULL;
            break;
        }
    }
    if (db_path == NULL) {
        fputs("usage: selecta-cli userdata --db <path> [--catalogue <tsv>] [--note <song> <text>] "
              "[--tag <song> <field> <value>] [--wrong <song> <note>]\n", stderr);
        return 2;
    }
    sel_registry_init(host);
    lib = catalogue != NULL ? sel_cli_open_catalogue_at(catalogue, db_path)
                            : sel_library_open(host, sel_machine_probe(host), 0, db_path);
    if (lib == NULL) {
        puts("db_open=0");
        return 1;
    }
    for (i = 0; i < argc && ok; i++) {
        if (strcmp(argv[i], "--played") == 0) {
            ok = sel_library_history_add(lib, argv[i + 1], (int64_t)time(NULL), 0);
            i += 1;
        } else if (strcmp(argv[i], "--note") == 0) {
            ok = sel_library_note(lib, argv[i + 1], argv[i + 2]);
            i += 2;
        } else if (strcmp(argv[i], "--wrong") == 0) {
            ok = sel_library_flag(lib, argv[i + 1], "wrong", argv[i + 2], &rows);
            i += 2;
        } else if (strcmp(argv[i], "--tag") == 0) {
            ok = sel_library_tag_edit(lib, argv[i + 1], argv[i + 2], argv[i + 3]);
            i += 3;
        } else if (strcmp(argv[i], "--db") == 0 || strcmp(argv[i], "--catalogue") == 0) {
            i++;
        }
    }
    if (ok) ok = sel_library_user_counts(lib, &history, &flags, &notes, &pending);
    printf("history_rows=%u flags=%u notes=%u pending_push=%u\n", (unsigned)history, (unsigned)flags,
           (unsigned)notes, (unsigned)pending);
    sel_library_close(lib);
    return ok ? 0 : 1;
}

/* `library genre --db <path>` (I181, T85): sel_library_genre_sync over the local songs of the DB. Prints
 * `songs= genre_lookups= genre_cache_hits= genre_set= unknown= unreadable= http_requests=`. Search the result
 * with `library search --db <path> genre:<text>`. */
static int library_genre(int argc, char** argv) {
    const char* db_path = NULL;
    const sel_host* host = sel_host_default();
    sel_genre_stats gs;
    sel_library* lib;
    int ok;
    int i;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        }
    }
    if (db_path == NULL) {
        fputs("usage: selecta-cli library genre --db <path>\n", stderr);
        return 2;
    }
    lib = sel_library_open(host, sel_machine_probe(host), 0, db_path);
    if (lib == NULL) {
        return 1;
    }
    ok = sel_library_genre_sync(lib, &gs);
    printf("songs=%u genre_lookups=%u genre_cache_hits=%u genre_set=%u unknown=%u unreadable=%u http_requests=%u "
           "songdb_lookups=%u songdb_cache_hits=%u composers_set=%u\n",
           (unsigned)gs.songs, (unsigned)gs.genre_lookups, (unsigned)gs.genre_cache_hits, (unsigned)gs.genre_set,
           (unsigned)gs.unknown, (unsigned)gs.unreadable, (unsigned)gs.http_requests, (unsigned)gs.songdb_lookups,
           (unsigned)gs.songdb_cache_hits, (unsigned)gs.composers_set);
    sel_library_close(lib);
    return ok ? 0 : 1;
}

static void composer_row(void* ctx, const char* key, const char* name, const char* composer) {
    (void)ctx;
    (void)composer;
    printf("row=%s|%s\n", key, name);
}

/* `library composer --db <path> <name>` (I182, T85): the composer page. Any spelling of the composer
 * lists the same songs. Prints `composer=<display>`, one `row=<key>|<name>` per song and `composer_rows=N`. */
static int library_composer(int argc, char** argv) {
    const char* db_path = NULL;
    const char* name = NULL;
    const sel_host* host = sel_host_default();
    char display[SEL_COMPOSER_MAX];
    sel_library* lib;
    int rows;
    int i;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else {
            name = argv[i];
        }
    }
    if (db_path == NULL || name == NULL || !sel_composer_display(name, display, sizeof display)) {
        fputs("usage: selecta-cli library composer --db <path> <name>\n", stderr);
        return 2;
    }
    lib = sel_library_open(host, sel_machine_probe(host), 0, db_path);
    if (lib == NULL) {
        return 1;
    }
    printf("composer=%s\n", display);
    rows = sel_library_composer_page(lib, name, composer_row, NULL);
    printf("composer_rows=%d\n", rows);
    sel_library_close(lib);
    return rows >= 0 ? 0 : 1;
}

int sel_cli_library(int argc, char** argv) {
    if (argc >= 1 && strcmp(argv[0], "composer") == 0) {
        return library_composer(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "genre") == 0) {
        return library_genre(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "browse") == 0) {
        return library_browse(argc, argv);
    }
    if (argc >= 1 && strcmp(argv[0], "rescan") == 0) {
        return library_rescan(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "scan") == 0) {
        return library_scan(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "sync") == 0) {
        return library_sync(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "key") == 0) {
        return library_key(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "meta") == 0) {
        return library_meta(argc - 1, argv + 1);
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
    if (argc >= 1 && strcmp(argv[0], "trim") == 0) {
        return library_trim(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "export-folder") == 0) {
        return sel_cli_library_export_folder(argc - 1, argv + 1);
    }
    if (argc >= 1 && strcmp(argv[0], "query") == 0) {
        return library_query(argc - 1, argv + 1);
    }
    fputs("usage: selecta-cli library browse <dir> | search --catalogue <tsv> <q> | scan [--db <path>] [--watch <ms>|--poll <ms>] <dir> | rescan [--db <path>] <dir>... | sync [--db <path>] [--now <epoch>] [--dump] modland <dir or http(s) url> | meta [--db <path>] <key> | key <source> <path> | genre --db <path> | composer --db <path> <name> | trim [--budget <n>] [--machine <name>] | query --explain <q> | export-folder --db <path> --dir <dir> [--folder <id>] [--format m3u] | stats [--machine <name>] [--db <path>]\n", stderr);
    return 2;
}
