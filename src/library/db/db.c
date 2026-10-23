/* Library DB open, memsys5 heap and the request handler (plan Phase 1, I144
 * to I146; D-M3-1, D-M3-2). SQLite's heap is process global, so the first
 * open allocates it once through host->mem_alloc and the last close frees it;
 * further libraries (other accounts) share it. */
#include "db_internal.h"
#include "schema.h"

#include <selecta/machine.h>

#include <string.h>

static void* g_heap;
static uint32_t g_heap_bytes;
static uint32_t g_heap_users;
static uint32_t g_heap_allocs;

uint32_t sel_library_heap_bytes(const sel_tag* machine) {
    unsigned long long ram =
        (unsigned long long)sel_tag_find(machine, SEL_MACHINE_FAST_RAM, 0) +
        (unsigned long long)sel_tag_find(machine, SEL_MACHINE_CHIP_RAM, 0);

    if (ram <= 4ull * 1024 * 1024) {
        return 512u * 1024u;
    }
    if (ram <= 16ull * 1024 * 1024) {
        return 2u * 1024u * 1024u;
    }
    if (ram <= 128ull * 1024 * 1024) {
        return 8u * 1024u * 1024u;
    }
    return 32u * 1024u * 1024u;
}

const char* sel_library_sqlite_version(void) {
    return sqlite3_libversion();
}

static int heap_acquire(const sel_host* host, uint32_t bytes) {
    if (g_heap_users == 0) {
        g_heap = host->mem_alloc(host, bytes, SEL_MEM_FAST);
        if (g_heap == NULL) {
            return 0;
        }
        g_heap_bytes = bytes;
        g_heap_allocs++;
        if (sqlite3_config(SQLITE_CONFIG_HEAP, g_heap, (int)bytes, 32) != SQLITE_OK) {
            host->mem_free(host, g_heap);
            g_heap = NULL;
            return 0;
        }
    }
    g_heap_users++;
    return 1;
}

static void heap_release(const sel_host* host) {
    if (g_heap_users > 0 && --g_heap_users == 0) {
        sqlite3_shutdown();
        host->mem_free(host, g_heap);
        g_heap = NULL;
    }
}

sel_library* sel_library_open(const sel_host* host, const sel_tag* machine, uint32_t account, const char* path) {
    sel_library* lib = host->mem_alloc(host, sizeof *lib, SEL_MEM_FAST);
    char sql[80];

    if (lib == NULL) {
        return NULL;
    }
    memset(lib, 0, sizeof *lib);
    lib->host = host;
    lib->account = account;
    if (!heap_acquire(host, sel_library_heap_bytes(machine))) {
        host->mem_free(host, lib);
        return NULL;
    }
    lib->heap_bytes = g_heap_bytes;
    lib->heap_allocs = g_heap_allocs;
    if (sqlite3_open_v2(path, &lib->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL) !=
            SQLITE_OK ||
        !sel_schema_migrate(lib->db) || !sel_library_register_regexp(lib->db)) {
        goto fail;
    }
    sqlite3_snprintf((int)sizeof sql, sql, "INSERT OR REPLACE INTO meta(key, value) VALUES('account', '%u')", account);
    if (sqlite3_exec(lib->db, sql, NULL, NULL, NULL) != SQLITE_OK || !sel_worker_start(lib)) {
        goto fail;
    }
    return lib;
fail:
    sqlite3_close(lib->db);
    heap_release(host);
    host->mem_free(host, lib);
    return NULL;
}

void sel_library_close(sel_library* lib) {
    const sel_host* host;

    if (lib == NULL) {
        return;
    }
    host = lib->host;
    sel_worker_stop(lib);
    sqlite3_close(lib->db);
    heap_release(host);
    host->mem_free(host, lib);
}

uint32_t sel_library_account(const sel_library* lib) {
    return lib->account;
}

uint32_t sel_library_heap_size(const sel_library* lib) {
    return lib->heap_bytes;
}

uint32_t sel_library_heap_allocs(const sel_library* lib) {
    return lib->heap_allocs;
}

static int query_int(sqlite3* db, const char* sql, int* out) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);

    if (rc == SQLITE_OK) {
        rc = sqlite3_step(st);
        if (rc == SQLITE_ROW) {
            *out = sqlite3_column_int(st, 0);
            rc = SQLITE_OK;
        }
    }
    sqlite3_finalize(st);
    return rc;
}

static void arg_int(sel_message* m, unsigned i, int v) {
    m->args[i].type = SEL_ARG_INT;
    m->args[i].v.i = v;
}

/* Stats plus an FTS5 self-check: one row goes in and is found by a prefix match inside a savepoint
 * that is rolled back, so the catalogue is left as it was. */
static void handle_stats(sel_library* lib, sel_message* reply) {
    int hits = 0;
    int songs = 0;
    int rc = SQLITE_OK;

    if (sqlite3_exec(lib->db, "SAVEPOINT selftest", NULL, NULL, NULL) == SQLITE_OK) {
        rc = sqlite3_exec(lib->db,
                          "INSERT INTO songs(key, source, path, name, artist, format)"
                          " VALUES('selftest:a.mod', 'selftest', 'a.mod', 'Probe', 'Lizardking', 'Protracker')",
                          NULL, NULL, NULL);
        if (rc == SQLITE_OK) {
            rc = query_int(lib->db, "SELECT count(*) FROM song_fts WHERE song_fts MATCH 'liz*'", &hits);
        }
        sqlite3_exec(lib->db, "ROLLBACK TO selftest", NULL, NULL, NULL);
        sqlite3_exec(lib->db, "RELEASE selftest", NULL, NULL, NULL);
    }
    if (rc == SQLITE_OK) {
        rc = query_int(lib->db, "SELECT count(*) FROM songs", &songs);
    }
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_STATS;
    reply->argc = 6;
    arg_int(reply, SEL_LIB_STAT_SCHEMA, sel_schema_version(lib->db));
    arg_int(reply, SEL_LIB_STAT_FTS5, sqlite3_compileoption_used("ENABLE_FTS5"));
    arg_int(reply, SEL_LIB_STAT_FTS_HITS, hits);
    arg_int(reply, SEL_LIB_STAT_SONGS, songs);
    arg_int(reply, SEL_LIB_STAT_SERVED, (int)lib->served);
    arg_int(reply, SEL_LIB_STAT_RC, rc);
}

void sel_library_handle(sel_library* lib, const sel_message* req, sel_message* reply) {
    lib->served++;
    if (req->id == SEL_LIB_REQ_STATS) {
        handle_stats(lib, reply);
    } else if (req->id == SEL_LIB_REQ_SEARCH || req->id == SEL_LIB_REQ_SAVE_VIEW || req->id == SEL_LIB_REQ_IMPORT) {
        if (lib->job != NULL) {
            if (req->id == SEL_LIB_REQ_SEARCH) {
                sel_library_search(lib, lib->job);
            } else if (req->id == SEL_LIB_REQ_SAVE_VIEW) {
                sel_library_save_view(lib, lib->job);
            } else {
                sel_library_import(lib, lib->job);
            }
        }
        reply->kind = SEL_MSG_EVT;
        reply->id = req->id;
        reply->argc = 0;
    }
}
