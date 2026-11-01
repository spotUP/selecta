/* I961c: the per-source song counts and sync times, kept by the library's writer (the I958 rating_pending_n pattern).
 *
 * The SOURCES command asks for both once per source row. As worker round trips they were 32 watched waits per burst on the
 * command thread. Now the worker keeps a snapshot: it recomputes the part a write dirtied at the end of sel_library_handle (before
 * the reply goes out, so a caller reading after its own write sees it), and the readers copy the snapshot under a sequence
 * counter. A reader never issues a request and never waits for the worker. */
#include "db_internal.h"

#include "selecta/source.h"

#include <stdlib.h>
#include <string.h>

/* sqlite calls this for every row change of a rowid table of the connection, the attached index included, and never for a WITHOUT ROWID
 * table: source_songs and meta are both WITHOUT ROWID, so their own changes are invisible here. Every source_songs change comes from a
 * trigger on songs, so a change of songs (main or idx) is the signal to recount. */
static void mirror_hook(void* arg, int op, const char* db, const char* table, sqlite3_int64 rowid) {
    sel_library* lib = (sel_library*)arg;

    (void)op, (void)db, (void)rowid;
    if (strcmp(table, "songs") == 0) lib->mir_dirty_songs = 1u;
}

static void mirror_add_songs(sel_src_mirror* m, const char* id, int n) {
    uint32_t i;

    if (id == NULL || n <= 0) return;
    for (i = 0; i < m->nsongs; i++) {
        if (strcmp(m->songs[i].id, id) == 0) {
            m->songs[i].n += n;
            return;
        }
    }
    if (m->nsongs >= SEL_SOURCE_MAX) return;
    snprintf(m->songs[m->nsongs].id, sizeof m->songs[0].id, "%s", id);
    m->songs[m->nsongs].n = n;
    m->nsongs++;
}

static void mirror_load_songs(sel_library* lib, sel_src_mirror* m) {
    sqlite3_stmt* st = NULL;

    m->nsongs = 0;
    if (sqlite3_prepare_v2(lib->db, "SELECT source, n FROM source_songs", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            mirror_add_songs(m, (const char*)sqlite3_column_text(st, 0), sqlite3_column_int(st, 1));
        }
        sqlite3_finalize(st);
    }
    /* An attached index adds the songs it holds that the user DB does not (the same key counts once, I265). */
    if (lib->has_index && sqlite3_prepare_v2(lib->db,
                                             "SELECT i.source, COUNT(*) FROM idx.songs AS i WHERE NOT EXISTS (SELECT 1 FROM songs AS s"
                                             " WHERE s.key = i.key) GROUP BY i.source",
                                             -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            mirror_add_songs(m, (const char*)sqlite3_column_text(st, 0), sqlite3_column_int(st, 1));
        }
        sqlite3_finalize(st);
    }
}

static void mirror_load_sync(sel_library* lib, sel_src_mirror* m) {
    sqlite3_stmt* st = NULL;

    m->nsync = 0;
    if (sqlite3_prepare_v2(lib->db, "SELECT substr(key, 6), value FROM meta WHERE key >= 'sync.' AND key < 'sync/'", -1, &st, NULL) != SQLITE_OK) return;
    while (m->nsync < SEL_SOURCE_MAX && sqlite3_step(st) == SQLITE_ROW) {
        const char* id = (const char*)sqlite3_column_text(st, 0);
        const char* v = (const char*)sqlite3_column_text(st, 1);

        if (id == NULL) continue;
        snprintf(m->sync[m->nsync].id, sizeof m->sync[0].id, "%s", id);
        m->sync[m->nsync].when = v != NULL ? (int64_t)strtoll(v, NULL, 10) : 0;
        m->nsync++;
    }
    sqlite3_finalize(st);
}

void sel_library_source_mirror_open(sel_library* lib) {
    lib->mir_dirty_songs = 1u;
    sqlite3_update_hook(lib->db, mirror_hook, lib);
    sel_library_source_mirror_refresh(lib);
}

/* Writer side: recount the songs when a write dirtied them, re-read the sync times always. The index scan (has_index) is the costly part; while more requests wait it is left
 * dirty, so a burst of scan batches pays for it once at the end of the burst. */
void sel_library_source_mirror_refresh(sel_library* lib) {
    sel_src_mirror* m = &lib->src_mir;
    int songs = lib->mir_dirty_songs || lib->mir_index != lib->has_index;

    if (songs && lib->has_index && sel_spsc_count(&lib->requests) > 0) songs = 0;
    __atomic_add_fetch(&m->seq, 1u, __ATOMIC_ACQ_REL); /* odd: a reader copying now retries */
    if (songs) {
        mirror_load_songs(lib, m);
        lib->mir_dirty_songs = 0u;
        lib->mir_index = lib->has_index;
    }
    mirror_load_sync(lib, m); /* a handful of rows: re-read every time, meta is WITHOUT ROWID and has no change signal */
    __atomic_add_fetch(&m->seq, 1u, __ATOMIC_ACQ_REL);
}

static void mirror_copy(sel_library* lib, sel_src_mirror* out) {
    unsigned s0, s1;

    do {
        s0 = __atomic_load_n(&lib->src_mir.seq, __ATOMIC_ACQUIRE);
        memcpy(out, &lib->src_mir, sizeof *out);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        s1 = __atomic_load_n(&lib->src_mir.seq, __ATOMIC_ACQUIRE);
    } while ((s0 & 1u) != 0u || s0 != s1);
}

int sel_library_source_songs(sel_library* lib, const char* source) {
    sel_src_mirror m;
    uint32_t i;

    if (lib == NULL || source == NULL) return -1;
    mirror_copy(lib, &m);
    for (i = 0; i < m.nsongs; i++) {
        if (strcmp(m.songs[i].id, source) == 0) return m.songs[i].n;
    }
    return 0;
}

int64_t sel_library_source_synced(sel_library* lib, const char* source) {
    sel_src_mirror m;
    uint32_t i;

    if (lib == NULL || source == NULL) return 0;
    mirror_copy(lib, &m);
    for (i = 0; i < m.nsync; i++) {
        if (strcmp(m.sync[i].id, source) == 0) return m.sync[i].when;
    }
    return 0;
}
