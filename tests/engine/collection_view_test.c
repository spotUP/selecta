/* I994d1 part 2: the collection page order, driven through the engine (COLLECTION_OPEN -> state.collection -> PLAY_CONTEXT).
 * Sentinel: stats.collection_orders counts the orders coll_build_run builds; it must move once per song list page and stay put for a
 * drill group page. Covers: the page carries order/shown/view fields (Custom Order: identity, shown == songs), PLAY_CONTEXT start is a
 * shown position and the context runs to the end of the shown rows, a re-opened page has a new serial and the old serial is stale
 * (nothing queued, songs_stale moves), a drill group page has no order. Sort and filter cases need the COLLECTION_VIEW verb (I994d2);
 * the pure ordering is tested in collection_order_test.c. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/library_item.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"
#include "engine_test_db_lock.h"
#include "source.h"

#define NSONGS 5u

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
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

static sel_engine_stats stats(sel_engine* e) {
    sel_engine_stats s;
    sel_engine_stats_get(e, &s);
    return s;
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

static uint32_t qcount(sel_engine* e) { return sel_engine_state(e)->queue_count; }

static const char* qpath(sel_engine* e, uint32_t i) {
    const sel_state* st = sel_engine_state(e);
    return st->queue_text + st->queue[i].path_offset;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_collview_XXXXXX";
    char db[256], line[600], key[NSONGS][64], sqltext[2048];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    const sel_collection_page* pg;
    sel_library_item it[8];
    uint32_t i, serial, old_serial, orders, stale, total = 0, got;
    size_t n = 0;
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
        snprintf(key[i], sizeof key[i], "local:cv%u.mod", (unsigned)i);
        n += (size_t)snprintf(sqltext + n, sizeof sqltext - n, "%s('%s','local','cv%u.mod','song%u','Ann','CvAlb',%u,%u)", i ? "," : "", key[i],
                              (unsigned)i, (unsigned)i, 500u - i, 1000u * (i + 1u));
    }
    sql(lib, sqltext);

    CHECK(stats(e).collection_orders == 0u);
    got = sel_library_items(lib, SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM), 0, it, 8u, &total);
    CHECK(got == 1u);
    if (got != 1u) return 1;
    snprintf(line, sizeof line, "COLLECTION_OPEN %u \"%u\"", (unsigned)SEL_LIB_ITEM_ALBUM, (unsigned)it[0].art_id);
    run(e, line);
    pg = sel_engine_state(e)->collection;
    CHECK(pg != NULL && pg->songs == NSONGS);
    if (pg == NULL) return 1;
    /* The order block exists, is the identity under Custom Order, and the build ran once. */
    CHECK(stats(e).collection_orders == 1u);
    CHECK(pg->order != NULL && pg->shown == NSONGS && pg->sort == (uint32_t)SEL_COLL_SORT_CUSTOM && pg->descending == 0u && pg->filter[0] == '\0');
    for (i = 0; pg->order != NULL && i < NSONGS; i++) CHECK(pg->order[i] == i);
    serial = pg->serial;

    /* PLAY_CONTEXT start 1 is shown position 1: rows 1..4 queue in shown order. */
    snprintf(line, sizeof line, "PLAY_CONTEXT collection %u 1 \"CvAlb\"", (unsigned)serial);
    run(e, line);
    CHECK(stats(e).context_plays == 1u && stats(e).context_rows == NSONGS - 1u && qcount(e) == NSONGS - 1u);
    for (i = 0; i < qcount(e) && i < NSONGS - 1u; i++) CHECK(strcmp(qpath(e, i), key[1u + i]) == 0);

    /* Reopening the page builds a new order under a new serial; the old serial is stale and queues nothing. */
    old_serial = serial;
    run(e, "QUEUE_CLEAR");
    orders = stats(e).collection_orders;
    snprintf(line, sizeof line, "COLLECTION_OPEN %u \"%u\"", (unsigned)SEL_LIB_ITEM_ALBUM, (unsigned)it[0].art_id);
    run(e, line);
    pg = sel_engine_state(e)->collection;
    CHECK(pg != NULL && pg->serial > old_serial && stats(e).collection_orders == orders + 1u);
    stale = stats(e).songs_stale;
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD collection %u \"0-1\"", (unsigned)old_serial);
    run(e, line);
    CHECK(qcount(e) == 0u && stats(e).songs_stale == stale + 1u);

    /* A drill group page has no order and builds none. */
    orders = stats(e).collection_orders;
    snprintf(line, sizeof line, "COLLECTION_OPEN %u \"local\"", (unsigned)SEL_LIB_ITEM_SOURCE);
    run(e, line);
    pg = sel_engine_state(e)->collection;
    CHECK(pg != NULL && pg->drill == (uint32_t)SEL_DRILL_PAGE_ROOT && pg->order == NULL && pg->shown == pg->songs);
    CHECK(stats(e).collection_orders == orders);

    sel_engine_destroy(e);
    sel_library_close(lib);
    snprintf(sqltext, sizeof sqltext, "rm -rf %s", dir);
    if (system(sqltext) != 0) g_fail++;
    if (g_fail == 0) puts("collection_view_ok");
    return g_fail == 0 ? 0 : 1;
}
