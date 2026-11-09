/* I1082d: the published snapshot (design doc 2026-11-05-I1082-engine-thread section 2.1).
 * The engine thread (this test's second thread, the only one that sends commands and pumps) edits the queue and rebuilds the
 * view and the collection page 10000 times while the UI thread (main) walks queue, view and collection of the snapshot it
 * acquired each turn. Built with SELECTA_TSAN=ON a read of live engine memory is a data race TSan reports; with publishing on
 * the UI reads only its own copy and TSan stays silent. Every snapshot is also checked for self-consistency (counts match the
 * lists, strings are what the engine wrote).
 * Sentinels: stats.pub_publishes > 0 and the UI saw more than one distinct queue_version (the copy ran and moved).
 * SELECTA_TEST_NOPUB=1 reads the live state instead (the red case on a build without the row's code path: TSan reports). */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/library_item.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"
#include "engine_test_db_lock.h"
#include "source.h"

#define NSONGS 40u
#define ITERATIONS 10000u

static int g_fail;
static volatile uint32_t g_done;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            if (g_fail < 20) printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
}

static void run(sel_engine* eng, const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump_reads(eng, sink, NULL, 60000u);
}

static void send_pump(sel_engine* eng, const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    (void)sel_engine_pump(eng, sink, NULL);
}

typedef struct engine_job {
    sel_engine* e;
} engine_job;

static void* engine_thread(void* arg) {
    sel_engine* e = ((engine_job*)arg)->e;
    char line[128];
    uint32_t i;

    for (i = 0; i < ITERATIONS; i++) {
        switch (i % 6u) {
            case 0:
            case 1:
                snprintf(line, sizeof line, "QUEUE_ADD \"q%u_song.mod\"", (unsigned)i);
                send_pump(e, line);
                break;
            case 2:
                send_pump(e, (i / 6u) % 2u ? "VIEW_STYLE \"tree\"" : "VIEW_STYLE \"regular\"");
                break;
            case 3:
                snprintf(line, sizeof line, "VIEW_WINDOW %u", 4u + (i / 6u) % 12u);
                send_pump(e, line);
                break;
            case 4:
                snprintf(line, sizeof line, "COLLECTION_VIEW \"%s\" %u %u", (i / 6u) % 3u ? "song" : "", 1u + (i / 6u) % 3u, (i / 6u) % 2u);
                send_pump(e, line);
                break;
            default:
                if ((i / 6u) % 5u == 4u) {
                    send_pump(e, "QUEUE_CLEAR");
                } else {
                    (void)sel_engine_pump(e, sink, NULL);
                }
                break;
        }
    }
    __atomic_store_n(&g_done, 1u, __ATOMIC_RELEASE);
    return NULL;
}

static void sql(sel_library* lib, const char* text) {
    char* err = NULL;
    int rc;
    db_enter(lib);
    rc = sqlite3_exec(lib->db, text, NULL, NULL, &err);
    db_leave(lib);
    if (rc != SQLITE_OK) {
        printf("FAIL sql: %s (%s)\n", text, err != NULL ? err : "?");
        g_fail++;
    }
}

/* Walks one state the way the UI does: queue, view window, collection page. Returns a checksum so the reads cannot be optimised out. */
static uint64_t walk(const sel_state* st) {
    uint64_t sum = 0;
    uint32_t i, n;
    const char* p;

    CHECK(st->queue_count == 0u || st->queue != NULL);
    for (i = 0; i < st->queue_count && st->queue != NULL; i++) {
        const char* path = st->queue_text + st->queue[i].path_offset;
        CHECK(path[0] == 'q' && strstr(path, "_song.mod") != NULL);
        sum += (uint64_t)strlen(path) + st->queue[i].id;
    }
    if (st->view != NULL) {
        CHECK(st->view->row_n <= SEL_VIEW_WINDOW_MAX);
        CHECK(st->view->row_n == 0u || st->view->rows != NULL);
        CHECK(st->view->group != NULL);
        for (i = 0; i < st->view->row_n && st->view->rows != NULL; i++) {
            const sel_view_row* r = &st->view->rows[i];
            if (r->text != NULL) sum += strlen(r->text);
            sum += r->id;
        }
    }
    if (st->collection != NULL) {
        const sel_collection_page* pg = st->collection;
        CHECK(pg->songs == NSONGS && pg->shown <= pg->songs);
        CHECK(pg->rows != NULL);
        n = 0;
        for (p = pg->rows; p != NULL && *p != '\0'; n++) {
            p += strlen(p) + 1u; /* key */
            p += strlen(p) + 1u; /* plays */
        }
        CHECK(n == pg->songs);
        for (i = 0; i < pg->shown && pg->order != NULL; i++) {
            CHECK(pg->order[i] < pg->songs);
            sum += pg->order[i];
        }
    }
    return sum;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_pubsnap_XXXXXX";
    char db[256], line[600], sqltext[4096];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    sel_library_item it[8];
    uint32_t pubs = 0, bytes_max = 0, us_max = 0;
    engine_job job;
    pthread_t th;
    uint32_t i, got, total = 0, turns = 0, last_ver = 0xFFFFFFFFu, versions = 0;
    uint64_t sum = 0;
    size_t n;
    int nopub = getenv("SELECTA_TEST_NOPUB") != NULL;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.queue_capacity = 32u;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);

    n = (size_t)snprintf(sqltext, sizeof sqltext, "INSERT INTO songs(key, source, path, name, artist, album, added, duration_ms) VALUES");
    for (i = 0; i < NSONGS; i++) {
        n += (size_t)snprintf(sqltext + n, sizeof sqltext - n, "%s('local:ps%u.mod','local','ps%u.mod','song%u','Ann','PsAlb',%u,%u)", i ? "," : "",
                              (unsigned)i, (unsigned)i, (unsigned)i, 900u - i, 1000u * (i + 1u));
    }
    sql(lib, sqltext);
    got = sel_library_items(lib, SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM), 0, it, 8u, &total);
    CHECK(got == 1u);
    if (got != 1u) return 1;
    snprintf(line, sizeof line, "COLLECTION_OPEN %u \"%u\"", (unsigned)SEL_LIB_ITEM_ALBUM, (unsigned)it[0].art_id);
    run(e, line);
    run(e, "VIEW_SOURCE \"queue\"");
    if (!nopub) {
        CHECK(sel_engine_publish_enable(e, 1u) == 1);
    }

    job.e = e;
    CHECK(pthread_create(&th, NULL, engine_thread, &job) == 0);
    while (__atomic_load_n(&g_done, __ATOMIC_ACQUIRE) == 0u) {
        if (!nopub) (void)sel_engine_ui_acquire(e);
        const sel_state* st = sel_engine_state(e);
        sum += walk(st);
        if (st->queue_version != last_ver) {
            last_ver = st->queue_version;
            versions++;
        }
        turns++;
    }
    pthread_join(th, NULL);
    if (!nopub) (void)sel_engine_ui_acquire(e);
    sum += walk(sel_engine_state(e));

    sel_engine_publish_stats(e, &pubs, &bytes_max, &us_max);
    printf("pub_snapshot turns=%u versions=%u publishes=%u pub_bytes_max=%u pub_us_max=%u sum=%llu\n", (unsigned)turns, (unsigned)versions,
           (unsigned)pubs, (unsigned)bytes_max, (unsigned)us_max, (unsigned long long)sum);
    if (!nopub) {
        CHECK(pubs >= ITERATIONS); /* the copy ran at the end of every pump */
        CHECK(versions > 1u);                 /* and the UI saw the queue move */
        CHECK(bytes_max > 0u);
    }
    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail == 0) {
        printf("pub_snapshot_ok\n");
        return 0;
    }
    printf("pub_snapshot FAILED: %d\n", g_fail);
    return 1;
}
