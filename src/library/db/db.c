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
        /* SQLite is compiled with MEMSTATUS=0; switch the counters on so sel_library_heap_peak() has data. */
        sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 1);
        if (sqlite3_config(SQLITE_CONFIG_HEAP, g_heap, (int)bytes, 32) != SQLITE_OK) {
            host->mem_free(host, g_heap);
            g_heap = NULL;
            return 0;
        }
    }
    sel_library_vfs_init();
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
    lib->cache_budget = sel_cache_budget(machine);
    if (!heap_acquire(host, sel_library_heap_bytes(machine))) {
        host->mem_free(host, lib);
        return NULL;
    }
    lib->heap_bytes = g_heap_bytes;
    lib->heap_allocs = g_heap_allocs;
    if (sqlite3_open_v2(path, &lib->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_URI, NULL) !=
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
    if (lib->cache != NULL) {
        host->mem_free(host, lib->cache);
    }
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
    sel_library_prefetch_stop(lib);
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
    if (rc == SQLITE_OK && lib->has_index) {
        int idx_songs = 0;
        rc = query_int(lib->db, "SELECT count(*) FROM idx.songs", &idx_songs);
        songs += idx_songs;
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
    } else if (req->id == SEL_LIB_REQ_SCAN_BATCH && req->argc == 1) {
        sel_library_scan_batch(lib, (const sel_scan_batch*)req->args[0].v.s, reply);
    } else if (req->id == SEL_LIB_REQ_REMOVE && req->argc == 1) {
        sel_library_remove(lib, (const sel_remove_batch*)req->args[0].v.s, reply);
    } else if (req->id == SEL_LIB_REQ_MOVE && req->argc == 1) {
        sel_library_move(lib, (const sel_move*)req->args[0].v.s, reply);
    } else if (req->id == SEL_LIB_REQ_META && req->argc == 1) {
        sel_library_meta(lib, (const sel_meta_op*)req->args[0].v.s, reply);
    } else if (req->id == SEL_LIB_REQ_ANNOTATE && req->argc == 1) {
        sel_library_annotate(lib, (const sel_annotate_batch*)req->args[0].v.s, reply);
    } else if (req->id == SEL_LIB_REQ_USER && req->argc == 1) {
        sel_library_user(lib, (const sel_user_op*)req->args[0].v.s, reply);
    } else if (req->id == SEL_LIB_REQ_GENRE && req->argc == 1) {
        sel_library_genre_op(lib, (const sel_genre_op*)req->args[0].v.s, reply);
    }
}

static int user_call(sel_library* lib, const sel_user_op* op) {
    sel_message reply;
    memset(&reply, 0, sizeof reply);
    if (lib == NULL || !sel_library_request(lib, SEL_LIB_REQ_USER, op, &reply) || reply.argc < 1) return 0;
    return reply.args[0].v.i;
}

/* Like user_call, and the reply's third value (rows) when the caller wants it. */
static int user_call_rows(sel_library* lib, const sel_user_op* op, uint32_t* rows) {
    sel_message reply;
    memset(&reply, 0, sizeof reply);
    if (lib == NULL || !sel_library_request(lib, SEL_LIB_REQ_USER, op, &reply) || reply.argc < 3) return 0;
    if (rows != NULL) *rows = reply.args[2].v.i > 0 ? (uint32_t)reply.args[2].v.i : 0;
    return reply.args[0].v.i;
}

int sel_library_flag(sel_library* lib, const char* key, const char* kind, const char* note, uint32_t* rows) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_FLAG;
    op.key = key;
    op.name = kind;
    op.value = note;
    return user_call_rows(lib, &op, rows) > 0;
}

int sel_library_flag_skipped(sel_library* lib, const char* key) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_FLAG_SKIPPED;
    op.key = key;
    return user_call(lib, &op) == 2;
}

int sel_library_flags_export(sel_library* lib, const char* path, uint32_t* rows) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_FLAG_EXPORT;
    op.name = path;
    return user_call_rows(lib, &op, rows) > 0;
}

int sel_library_note(sel_library* lib, const char* key, const char* note) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_NOTE;
    op.key = key;
    op.value = note != NULL ? note : "";
    return user_call(lib, &op) > 0;
}

int sel_library_sticker_set(sel_library* lib, const char* key, const char* name, const char* value) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_STICKER_SET;
    op.key = key;
    op.name = name;
    op.value = value != NULL ? value : "";
    return user_call(lib, &op) > 0;
}

int sel_library_sticker_get(sel_library* lib, const char* key, const char* name, char* out, uint32_t cap) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_STICKER_GET;
    op.key = key;
    op.name = name;
    op.out = out;
    op.out_cap = cap;
    return user_call(lib, &op) == 2;
}

int sel_library_sticker_del(sel_library* lib, const char* key, const char* name) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_STICKER_DEL;
    op.key = key;
    op.name = name;
    return user_call(lib, &op) == 2;
}

int sel_library_sticker_list(sel_library* lib, const char* key, char* out, uint32_t cap, uint32_t* rows) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_STICKER_LIST;
    op.key = key;
    op.out = out;
    op.out_cap = cap;
    return user_call_rows(lib, &op, rows) > 0;
}

int sel_library_song_panel(sel_library* lib, const char* key, char* out, uint32_t cap, uint32_t* rows) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_PANEL;
    op.key = key;
    op.out = out;
    op.out_cap = cap;
    return user_call_rows(lib, &op, rows) > 0;
}

int sel_library_stats(sel_library* lib, char* out, uint32_t cap, uint32_t* rows) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_STATS;
    op.out = out;
    op.out_cap = cap;
    return user_call_rows(lib, &op, rows) > 0;
}

int sel_library_cue_set(sel_library* lib, const char* key, uint32_t idx, uint32_t pos_ms) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_CUE_SET;
    op.key = key;
    op.at = idx;
    op.duration_ms = (int32_t)pos_ms;
    return user_call(lib, &op) > 0;
}

int sel_library_cue_get(sel_library* lib, const char* key, uint32_t idx, uint32_t* pos_ms) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_CUE_GET;
    op.key = key;
    op.at = idx;
    return user_call_rows(lib, &op, pos_ms) == 2;
}

int sel_library_loudness_set_ex(sel_library* lib, const char* key, const char* path, int32_t gain_cdb, int provisional) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_LOUDNESS_SET;
    op.key = key;
    op.value = path;
    op.at = gain_cdb;
    op.to = provisional ? 1 : 0;
    return user_call(lib, &op) > 0;
}

int sel_library_loudness_set(sel_library* lib, const char* key, const char* path, int32_t gain_cdb) {
    return sel_library_loudness_set_ex(lib, key, path, gain_cdb, 0);
}

int sel_library_loudness_get(sel_library* lib, const char* key, int32_t* gain_cdb) {
    return sel_library_loudness_get_ex(lib, key, gain_cdb, NULL);
}

int sel_library_loudness_get_ex(sel_library* lib, const char* key, int32_t* gain_cdb, int* provisional) {
    sel_user_op op;
    sel_message reply;
    memset(&op, 0, sizeof op);
    memset(&reply, 0, sizeof reply);
    op.op = SEL_USER_LOUDNESS_GET;
    op.key = key;
    if (lib == NULL || !sel_library_request(lib, SEL_LIB_REQ_USER, &op, &reply) || reply.argc < 3) return 0;
    if (reply.args[0].v.i != 2 && reply.args[0].v.i != 3) return 0;
    if (gain_cdb != NULL) *gain_cdb = (int32_t)reply.args[2].v.i;
    if (provisional != NULL) *provisional = reply.args[0].v.i == 3;
    return 1;
}

int sel_library_rating_set(sel_library* lib, const char* key, int stars, int origin) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_RATING_SET;
    op.key = key;
    op.at = stars;
    op.to = origin;
    return user_call(lib, &op) > 0;
}

int sel_library_rating_get(sel_library* lib, const char* key, int* stars) {
    sel_user_op op;
    uint32_t rows = 0;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_RATING_GET;
    op.key = key;
    if (user_call_rows(lib, &op, &rows) != 2) return 0;
    if (stars != NULL) *stars = (int)rows;
    return 1;
}

int sel_library_rating_pending(sel_library* lib, char* out, uint32_t cap, uint32_t* rows) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_RATING_PENDING;
    op.out = out;
    op.out_cap = cap;
    return user_call_rows(lib, &op, rows) > 0;
}

int sel_library_rating_done(sel_library* lib, const char* key) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_RATING_DONE;
    op.key = key;
    return user_call(lib, &op) > 0;
}

int sel_library_tag_edit(sel_library* lib, const char* key, const char* field, const char* value) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_TAG_EDIT;
    op.key = key;
    op.name = field;
    op.value = value != NULL ? value : "";
    return user_call(lib, &op) > 0;
}

/* I178 folder tree: runs one op; returns SEL_FOLDER_OK or the refusal code from reply [2]. */
static int folder_call(sel_library* lib, const sel_user_op* op, uint32_t* rows) {
    sel_message reply;
    memset(&reply, 0, sizeof reply);
    if (rows != NULL) *rows = 0;
    if (lib == NULL || !sel_library_request(lib, SEL_LIB_REQ_USER, op, &reply) || reply.argc < 3) {
        return SEL_FOLDER_ERR_FAILED;
    }
    if (reply.args[0].v.i == 0) {
        return reply.args[1].v.i == SQLITE_CONSTRAINT ? reply.args[2].v.i : SEL_FOLDER_ERR_FAILED;
    }
    if (rows != NULL) *rows = reply.args[2].v.i > 0 ? (uint32_t)reply.args[2].v.i : 0;
    return SEL_FOLDER_OK;
}

int sel_library_folder_add(sel_library* lib, const char* name, uint32_t parent, uint32_t* out_id) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_FOLDER_ADD;
    op.name = name;
    op.at = parent;
    return folder_call(lib, &op, out_id);
}

int sel_library_folder_move(sel_library* lib, uint32_t id, uint32_t parent) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_FOLDER_MOVE;
    op.at = id;
    op.to = parent;
    return folder_call(lib, &op, NULL);
}

int sel_library_folder_del(sel_library* lib, uint32_t id) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_FOLDER_DEL;
    op.at = id;
    return folder_call(lib, &op, NULL);
}

int sel_library_playlist_move(sel_library* lib, uint32_t id, uint32_t folder) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_PLAYLIST_MOVE;
    op.at = id;
    op.to = folder;
    return folder_call(lib, &op, NULL);
}

int sel_library_playlist_add(sel_library* lib, const char* name, uint32_t folder, const char* keys_nl, uint32_t* out_id) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_PLAYLIST_ADD;
    op.name = name;
    op.at = folder;
    op.value = keys_nl;
    return folder_call(lib, &op, out_id);
}

int sel_library_folder_counts(sel_library* lib, uint32_t* folders, uint32_t* in_folders, uint32_t* playlists, uint32_t* entries) {
    sel_user_op op;
    sel_message reply;
    uint32_t* dst[4];
    int i;
    memset(&op, 0, sizeof op);
    memset(&reply, 0, sizeof reply);
    op.op = SEL_USER_FOLDER_COUNTS;
    dst[0] = folders; dst[1] = in_folders; dst[2] = playlists; dst[3] = entries;
    if (lib == NULL || !sel_library_request(lib, SEL_LIB_REQ_USER, &op, &reply) || reply.argc < 6 ||
        reply.args[0].v.i == 0) {
        return SEL_FOLDER_ERR_FAILED;
    }
    for (i = 0; i < 4; i++) {
        if (dst[i] != NULL) *dst[i] = reply.args[2 + i].v.i > 0 ? (uint32_t)reply.args[2 + i].v.i : 0;
    }
    return SEL_FOLDER_OK;
}

static int folder_text_call(sel_library* lib, uint32_t op_id, uint32_t at, char* out, uint32_t cap, uint32_t* count) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = op_id;
    op.at = at;
    op.out = out;
    op.out_cap = cap;
    return folder_call(lib, &op, count);
}

int sel_library_folder_playlists(sel_library* lib, uint32_t folder, char* out, uint32_t cap, uint32_t* count) {
    return folder_text_call(lib, SEL_USER_FOLDER_PLAYLISTS, folder, out, cap, count);
}

int sel_library_playlist_keys(sel_library* lib, uint32_t playlist, char* out, uint32_t cap, uint32_t* count) {
    return folder_text_call(lib, SEL_USER_PLAYLIST_KEYS, playlist, out, cap, count);
}

int sel_library_folder_tree(sel_library* lib, char* out, uint32_t cap, uint32_t* count) {
    return folder_text_call(lib, SEL_USER_FOLDER_TREE, 0, out, cap, count);
}

int sel_library_home(sel_library* lib, uint32_t limit, char* out, uint32_t cap, uint32_t* lines) {
    sel_user_op op;
    sel_message reply;
    memset(&op, 0, sizeof op);
    memset(&reply, 0, sizeof reply);
    op.op = SEL_USER_HOME;
    op.at = limit;
    op.out = out;
    op.out_cap = cap;
    if (lib == NULL || !sel_library_request(lib, SEL_LIB_REQ_USER, &op, &reply) || reply.argc < 3 || reply.args[0].v.i < 1) {
        return 0;
    }
    if (lines != NULL) *lines = (uint32_t)reply.args[2].v.i;
    return 1;
}

int sel_library_user_counts(sel_library* lib, uint32_t* history, uint32_t* flags, uint32_t* notes, uint32_t* pending_push) {
    sel_user_op op;
    sel_message reply;
    memset(&op, 0, sizeof op);
    memset(&reply, 0, sizeof reply);
    op.op = SEL_USER_COUNTS;
    if (lib == NULL || !sel_library_request(lib, SEL_LIB_REQ_USER, &op, &reply) || reply.argc < 6 ||
        reply.args[0].v.i < 1) {
        return 0;
    }
    if (history != NULL) *history = (uint32_t)reply.args[2].v.i;
    if (flags != NULL) *flags = (uint32_t)reply.args[3].v.i;
    if (notes != NULL) *notes = (uint32_t)reply.args[4].v.i;
    if (pending_push != NULL) *pending_push = (uint32_t)reply.args[5].v.i;
    return 1;
}

int sel_library_history_add(sel_library* lib, const char* key, int64_t at, int skipped) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_HISTORY;
    op.key = key;
    op.at = at;
    op.skipped = skipped;
    return user_call(lib, &op) > 0;
}

int sel_library_like(sel_library* lib, const char* key, int64_t at) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_LIKE;
    op.key = key;
    op.at = at;
    return user_call(lib, &op) > 0;
}

int sel_library_keep(sel_library* lib, const char* key, int64_t at) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_KEEP;
    op.key = key;
    op.at = at;
    return user_call(lib, &op) > 0;
}

int sel_library_song_config_set(sel_library* lib, const char* key, const char* list) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_STICKER_SET;
    op.key = key;
    op.name = "rp.config";
    op.value = list != NULL ? list : "";
    return user_call(lib, &op) > 0;
}

int sel_library_song_config_get(sel_library* lib, const char* key, char* out, uint32_t cap) {
    sel_user_op op;
    memset(&op, 0, sizeof op);
    op.op = SEL_USER_STICKER_GET;
    op.key = key;
    op.name = "rp.config";
    op.out = out;
    op.out_cap = cap;
    return user_call(lib, &op) > 0;
}
