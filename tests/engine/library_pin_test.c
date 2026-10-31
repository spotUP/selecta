/* I406b: sel_library_pin hands online hits to the library job thread through a ring of SEL_PIN_MAX slots. With the thread blocked
 * on the library call lock (a sync owns the library, I782), 1 + 33 pins leave 33 rows written and exactly 1 dropped (ring full);
 * a pinned row has added = 0 (never New) and the format is the library's name for the site's string. Pins queued right
 * before close are in the database afterwards (close drains the ring). */
#include <pthread.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/registry.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static volatile int g_held, g_release;

static void* holder(void* arg) {
    sel_library* lib = arg;

    pthread_mutex_lock(&lib->call_lock);
    g_held = 1;
    while (!g_release) usleep(1000);
    pthread_mutex_unlock(&lib->call_lock);
    return NULL;
}

static void pin_n(sel_library* lib, int from, int to) {
    char path[64], title[64];
    int i;

    for (i = from; i < to; i++) {
        snprintf(path, sizeof path, "sixtus/Song_%03d.zip", i);
        snprintf(title, sizeof title, "Song %03d", i);
        sel_library_pin(lib, "modulespl", path, title, "Sixtus", "mod");
    }
}

static int count(const char* db_path, const char* sql) {
    sqlite3* db;
    sqlite3_stmt* st;
    int n = -1;

    if (sqlite3_open(db_path, &db) != SQLITE_OK) return -1;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return n;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_pin_XXXXXX";
    char db[256];
    sel_library* lib;
    pthread_t th;
    int i;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);

    /* The job thread owns one pin and waits on the held call lock; 33 more fill the ring (32) and drop one. */
    pthread_create(&th, NULL, holder, lib);
    while (!g_held) usleep(1000);
    pin_n(lib, 0, 1);
    for (i = 0; i < 5000 && !lib->job_pin_busy; i++) usleep(1000);
    CHECK(lib->job_pin_busy);
    pin_n(lib, 1, 34);
    CHECK(sel_library_pin_dropped(lib) == 1u);
    g_release = 1;
    pthread_join(th, NULL);
    for (i = 0; i < 5000 && sel_library_pin_rows(lib) < 33u; i++) usleep(1000);
    CHECK(sel_library_pin_rows(lib) == 33u);
    /* Pins still queued at close are written by the close (drain). */
    pin_n(lib, 100, 103);
    sel_library_close(lib);
    CHECK(count(db, "SELECT COUNT(*) FROM songs WHERE source='modulespl'") == 36);
    CHECK(count(db, "SELECT COUNT(*) FROM songs WHERE source='modulespl' AND added=0") == 36);
    CHECK(count(db, "SELECT COUNT(*) FROM songs WHERE source='modulespl' AND format='Protracker'") == 36);
    /* A pin is the user's pick, not news from the source: the batch records no New-level entry (new_day = 0). */
    CHECK(count(db, "SELECT COUNT(*) FROM source_new") == 0);

    if (g_fail == 0) printf("library_pin_ok\n");
    return g_fail != 0;
}
