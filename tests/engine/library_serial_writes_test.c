/* I1077e: the engine publishes state.library_serial from the store's commit count (I1077b/c). Through the real entry point
 * (sel_engine_send_line + sel_engine_pump) with no scan or sync running:
 *   - LIKE, PLAYLIST_ADD_SONG and RATE each move library_serial by exactly 1 (one committed write = one publish; the sentinel
 *     stats.lib_writes_done proves the queued write ran,),
 *   - idle (no write) pumps move neither library_serial nor sources_serial. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/plugin.h>
#include <selecta/registry.h>
#include <selecta/tags.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"
#include "engine_test_db_lock.h"

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static volatile int g_plid;

/* The id PLAYLIST_CREATE answers (LIBRARY_RESULT key "playlist", name, value = id), as a UI gets it. */
static void sink(void* ctx, const sel_message* evt) {
    const char* key;
    const char* name;
    const char* value;

    (void)ctx;
    if (evt->id != SEL_EVT_LIBRARY_RESULT || evt->argc == 0 || evt->args[0].type != SEL_ARG_TAGS) return;
    key = (const char*)sel_tag_find(evt->args[0].v.t, SEL_SRC_KEY, 0);
    name = (const char*)sel_tag_find(evt->args[0].v.t, SEL_SRC_NAME, 0);
    value = (const char*)sel_tag_find(evt->args[0].v.t, SEL_SRC_PATH, 0);
    if (key != NULL && name != NULL && value != NULL && strcmp(key, "playlist") == 0 && strcmp(name, "mix") == 0) g_plid = atoi(value);
}

static void pump_n(sel_engine* e, int n) {
    int i;

    for (i = 0; i < n; i++) {
        sel_engine_pump(e, sink, NULL);
        usleep(2000);
    }
}

/* Sends one write, waits until the write queue ran it, then pumps so the publish has had every chance. Returns the library_serial move. */
static uint32_t write_and_settle(sel_engine* e, const char* line) {
    char pool[512];
    sel_engine_stats s0, s1;
    uint32_t before = sel_engine_state(e)->library_serial;
    int i;

    sel_engine_stats_get(e, &s0);
    CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
    for (i = 0; i < 1500; i++) {
        sel_engine_pump(e, sink, NULL);
        sel_engine_stats_get(e, &s1);
        if (s1.lib_writes_done > s0.lib_writes_done) break;
        usleep(2000);
    }
    CHECK(s1.lib_writes_done == s0.lib_writes_done + 1u); /* the queued write ran */
    pump_n(e, 25);
    return sel_engine_state(e)->library_serial - before;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_libserial_XXXXXX";
    char db[256], pool[512], line[128];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    uint32_t lib0, src0;
    int i;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    db_enter(lib);
    CHECK(sqlite3_exec(lib->db,
                       "INSERT INTO songs(key, source, path, name, artist, album, format, added) VALUES"
                       "('local:a.mod','local','a.mod','a','Ann','Alb1','mod',100);",
                       NULL, NULL, NULL) == SQLITE_OK);
    db_leave(lib);
    /* The SOURCES rows are published once a client asked for them: ask, so the idle check covers sources_serial. */
    CHECK(sel_engine_send_line(e, "SOURCES", pool, sizeof pool) == 0);
    CHECK(sel_engine_send_line(e, "PLAYLIST_CREATE mix", pool, sizeof pool) == 0);
    for (i = 0; i < 1500 && g_plid == 0; i++) {
        sel_engine_pump(e, sink, NULL);
        usleep(2000);
    }
    CHECK(g_plid != 0);
    pump_n(e, 50); /* the create's own publish and the posted reads settle */

    /* a. each user write moves library_serial by exactly 1 */
    CHECK(write_and_settle(e, "LIKE local:a.mod") == 1u);
    snprintf(line, sizeof line, "PLAYLIST_ADD_SONG %d local:a.mod", g_plid);
    CHECK(write_and_settle(e, line) == 1u);
    CHECK(write_and_settle(e, "RATE 4 local:a.mod") == 1u);

    /* b. idle: no write, many pumps, neither serial moves */
    lib0 = sel_engine_state(e)->library_serial;
    src0 = sel_engine_state(e)->sources_serial;
    pump_n(e, 200);
    CHECK(sel_engine_state(e)->library_serial == lib0);
    CHECK(sel_engine_state(e)->sources_serial == src0);

    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail == 0) printf("library_serial_writes_ok\n");
    return g_fail != 0;
}
