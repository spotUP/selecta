/* I994b: the list kinds `collection` (version = state.collection->serial) and `hits` (version = state.search_serial) of the SONGS_*
 * verbs and PLAY_CONTEXT. Driven through the engine command entry. Sentinels: stats.songs_collection_opens / songs_hits_opens prove
 * songs_open bound the new kinds; context_plays / context_rows prove PLAY_CONTEXT ran. Covers: an album page resolves the right paths
 * in page order (PLAY_CONTEXT from row 2 and SONGS_QUEUE_ADD of a range), a drill group page names no song (every row skipped), a stale
 * serial changes nothing, no page open is bad_list, the hit list resolves the hit keys, the serial moves per search, a stale hit
 * serial changes nothing. */
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

#define NSONGS 6u

static int g_fail;
static uint32_t g_errors;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_ERROR) g_errors++;
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

static int is_ctx(sel_engine* e, uint32_t i) { return (sel_engine_state(e)->queue[i].flags & SEL_QUEUE_FLAG_CONTEXT) != 0u; }

/* One search source with four hits, keys "i994bsrc:h0" .. "i994bsrc:h3". */
static int hit_search(const sel_host* host, const char* query, sel_search_sink* out) {
    char name[16];
    unsigned i;
    (void)host;
    (void)query;
    for (i = 0; i < 4u; i++) {
        snprintf(name, sizeof name, "h%u", i);
        if (!out->row(out->ctx, name, name, "Hit Artist", "mod")) return -1;
    }
    return 1;
}

static const sel_source g_hit_src = {
    "i994bsrc", "I994b hit source", "", "Four search hits.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, hit_search,
};

static void pump_until_serial_moves(sel_engine* e, uint32_t from) {
    int i;
    for (i = 0; i < 5000 && sel_engine_state(e)->search_serial < from + 2u; i++) {
        usleep(1000);
        (void)sel_engine_pump(e, sink, NULL);
    }
    (void)sel_engine_pump(e, sink, NULL);
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_pctxlists_XXXXXX";
    char db[256], line[600], key[NSONGS][64], sqltext[2048], hk[64];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    const sel_collection_page* pg;
    uint32_t i, serial, plays, rows, opens, ser0;
    size_t n = 0;
    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    CHECK(sel_registry_init(host) > 0);
    CHECK(sel_source_register(&g_hit_src));
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.queue_capacity = 32u;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);

    /* An album of six songs (the keys are the paths the queue holds). */
    n = (size_t)snprintf(sqltext, sizeof sqltext, "INSERT INTO songs(key, source, path, name, artist, album, added) VALUES");
    for (i = 0; i < NSONGS; i++) {
        snprintf(key[i], sizeof key[i], "local:alb%u.mod", (unsigned)i);
        n += (size_t)snprintf(sqltext + n, sizeof sqltext - n, "%s('%s','local','alb%u.mod','song%u','Ann','Alb1',%u)", i ? "," : "", key[i],
                              (unsigned)i, (unsigned)i, 100u + i);
    }
    sql(lib, sqltext);

    /* No collection page open yet: bad_list, nothing queued, no sentinel. */
    g_errors = 0;
    run(e, "SONGS_QUEUE_ADD collection 0 \"0-1\"");
    CHECK(g_errors == 1u && qcount(e) == 0u && stats(e).songs_collection_opens == 0u);

    {
        sel_library_item it[8];
        uint32_t total = 0, got;
        got = sel_library_items(lib, SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM), 0, it, 8u, &total);
        CHECK(got == 1u);
        snprintf(line, sizeof line, "COLLECTION_OPEN %u \"%u\"", (unsigned)SEL_LIB_ITEM_ALBUM, got > 0u ? (unsigned)it[0].art_id : 0u);
        run(e, line);
    }
    pg = sel_engine_state(e)->collection;
    CHECK(pg != NULL && pg->songs == NSONGS);
    serial = pg != NULL ? pg->serial : 0u;

    /* PLAY_CONTEXT from row 2 of the page: rows 2..5 in page order, all flagged context, the clicked one plays. */
    opens = stats(e).songs_collection_opens;
    plays = stats(e).context_plays;
    rows = stats(e).context_rows;
    snprintf(line, sizeof line, "PLAY_CONTEXT collection %u 2 \"Alb1\"", (unsigned)serial);
    run(e, line);
    CHECK(stats(e).songs_collection_opens == opens + 1u);
    CHECK(stats(e).context_plays == plays + 1u && stats(e).context_rows == rows + 4u);
    CHECK(qcount(e) == 4u);
    for (i = 0; i < qcount(e) && i < 4u; i++) {
        CHECK(strcmp(qpath(e, i), key[2u + i]) == 0);
        CHECK(is_ctx(e, i));
    }
    CHECK(strcmp(sel_engine_state(e)->ctx_name, "Alb1") == 0);

    /* Another verb reads the same list: SONGS_QUEUE_ADD of the range 0-1 appends two songs. */
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD collection %u \"0-1\"", (unsigned)serial);
    run(e, line);
    CHECK(stats(e).songs_collection_opens == opens + 2u);
    /* I994: hand-added songs land before the context rows: the playing row stays first, the two new songs sit at 1 and 2 (not flagged),
     * the rest of the context keeps its order. */
    CHECK(qcount(e) == 6u && strcmp(qpath(e, 0), key[2]) == 0);
    CHECK(strcmp(qpath(e, 1), key[0]) == 0 && strcmp(qpath(e, 2), key[1]) == 0 && !is_ctx(e, 1) && !is_ctx(e, 2));
    CHECK(strcmp(qpath(e, 3), key[3]) == 0 && strcmp(qpath(e, 4), key[4]) == 0 && strcmp(qpath(e, 5), key[5]) == 0);

    /* A stale serial changes nothing. */
    {
        uint32_t stale = stats(e).songs_stale;
        snprintf(line, sizeof line, "SONGS_QUEUE_ADD collection %u \"0-1\"", (unsigned)serial + 1u);
        run(e, line);
        CHECK(qcount(e) == 6u && stats(e).songs_stale == stale + 1u);
    }

    /* A range past the page is a range error. */
    g_errors = 0;
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD collection %u \"0-9\"", (unsigned)serial);
    run(e, line);
    CHECK(g_errors == 1u && qcount(e) == 6u);

    /* A drill group page (the source's first level: Formats, Platforms, Artists, New) names no song: every row is skipped. */
    run(e, "QUEUE_CLEAR");
    snprintf(line, sizeof line, "COLLECTION_OPEN %u \"local\"", (unsigned)SEL_LIB_ITEM_SOURCE);
    run(e, line);
    pg = sel_engine_state(e)->collection;
    CHECK(pg != NULL && pg->drill == (uint32_t)SEL_DRILL_PAGE_ROOT && pg->songs >= 2u);
    if (pg != NULL && pg->songs >= 2u) {
        snprintf(line, sizeof line, "SONGS_QUEUE_ADD collection %u \"0-%u\"", (unsigned)pg->serial, (unsigned)pg->songs - 1u);
        run(e, line);
        CHECK(qcount(e) == 0u);
    }

    /* The hit list: the serial moves when a search starts and when its hits are collected. */
    ser0 = sel_engine_state(e)->search_serial;
    g_errors = 0;
    run(e, "SONGS_QUEUE_ADD hits 0 \"0-0\"");
    CHECK(qcount(e) == 0u && g_errors == 1u); /* nothing searched: no row 0 */
    run(e, "SOURCE_SEARCH i994bsrc q");
    pump_until_serial_moves(e, ser0);
    CHECK(sel_engine_state(e)->search_serial >= ser0 + 2u);
    serial = sel_engine_state(e)->search_serial;
    opens = stats(e).songs_hits_opens;
    plays = stats(e).context_plays;
    snprintf(line, sizeof line, "PLAY_CONTEXT hits %u 1 \"Search\"", (unsigned)serial);
    run(e, line);
    CHECK(stats(e).songs_hits_opens == opens + 1u);
    CHECK(stats(e).context_plays == plays + 1u);
    CHECK(qcount(e) == 3u);
    for (i = 0; i < qcount(e) && i < 3u; i++) {
        snprintf(hk, sizeof hk, "i994bsrc:h%u", i + 1u);
        CHECK(strcmp(qpath(e, i), hk) == 0 && is_ctx(e, i));
    }
    CHECK(strcmp(sel_engine_state(e)->ctx_name, "Search") == 0);

    /* A stale hit serial changes nothing. */
    {
        uint32_t stale = stats(e).songs_stale;
        snprintf(line, sizeof line, "PLAY_CONTEXT hits %u 0 \"Search\"", (unsigned)serial - 1u);
        run(e, line);
        CHECK(qcount(e) == 3u && stats(e).songs_stale == stale + 1u);
    }

    sel_engine_destroy(e);
    sel_library_close(lib);
    snprintf(sqltext, sizeof sqltext, "rm -rf %s", dir);
    if (system(sqltext) != 0) g_fail++;
    if (g_fail == 0) puts("play_context_lists_ok");
    return g_fail == 0 ? 0 : 1;
}
