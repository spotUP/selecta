/* Library search on the worker (plan Phase 2, I149 and I150): LIBRARY_SEARCH, saved views and the
 * TSV catalogue import. Query text becomes SQL only through sel_query_compile; bare words are
 * served by the FTS5 index (prefix match per word), everything else by the compiled SQL with
 * `ORDER BY songs.key`. REGEXP is a SQL function backed by src/library/regex.c, so `key:/re/`
 * runs the same on the desktop and the 68k. */
#include "db_internal.h"

#include "../regex.h"

#include <stdlib.h>
#include <string.h>

static void regexp_fn(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
    const char* pat = (const char*)sqlite3_value_text(argv[0]);
    const char* txt = (const char*)sqlite3_value_text(argv[1]);
    int r;

    (void)argc;
    if (pat == NULL || txt == NULL) {
        sqlite3_result_null(ctx);
        return;
    }
    r = sel_regex_match(pat, txt);
    if (r < 0) {
        sqlite3_result_error(ctx, "bad_regex", -1);
        return;
    }
    sqlite3_result_int(ctx, r);
}

int sel_library_register_regexp(sqlite3* db) {
    return sqlite3_create_function(db, "regexp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL, regexp_fn, NULL,
                                   NULL) == SQLITE_OK;
}

/* Saved query lookup for `view:name`: the text lives in a per-compile slot, because the compiler
 * keeps reading the text of every view on its expansion stack. */
static const char* view_lookup(void* ctx, const char* name) {
    sel_library* lib = ctx;
    sqlite3_stmt* st = NULL;
    const char* text = NULL;

    if (lib->view_used >= sizeof lib->view_text / sizeof lib->view_text[0]) {
        return NULL;
    }
    if (sqlite3_prepare_v2(lib->db, "SELECT query FROM views WHERE name = ?1", -1, &st, NULL) != SQLITE_OK) {
        return NULL;
    }
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW) {
        char* slot = lib->view_text[lib->view_used];
        const char* q = (const char*)sqlite3_column_text(st, 0);
        if (q != NULL && strlen(q) < sizeof lib->view_text[0]) {
            strcpy(slot, q);
            lib->view_used++;
            text = slot;
        }
    }
    sqlite3_finalize(st);
    return text;
}

static void set_error(sel_lib_job* job, const char* code) {
    strncpy(job->error, code, sizeof job->error - 1u);
    job->error[sizeof job->error - 1u] = '\0';
}

/* Bare words: letters, digits, `_` and blanks only, and none of the query operators. */
static int is_bare_words(const char* t) {
    int words = 0;
    int in_word = 0;
    const char* w = t;

    for (; *t != '\0'; t++) {
        char c = *t;
        int alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (alnum) {
            if (!in_word) {
                w = t;
                words++;
            }
            in_word = 1;
        } else if (c == ' ') {
            if (in_word && ((t - w == 2 && strncmp(w, "or", 2) == 0) || (t - w == 3 && (strncmp(w, "and", 3) == 0 || strncmp(w, "not", 3) == 0)))) {
                return 0;
            }
            in_word = 0;
        } else {
            return 0;
        }
    }
    if (in_word && ((t - w == 2 && strncmp(w, "or", 2) == 0) || (t - w == 3 && (strncmp(w, "and", 3) == 0 || strncmp(w, "not", 3) == 0)))) {
        return 0;
    }
    return words > 0;
}

static void collect(sel_library* lib, sqlite3_stmt* st, sel_lib_job* job) {
    uint32_t used = 0;
    int rc;

    (void)lib;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const char* k = (const char*)sqlite3_column_text(st, 0);
        size_t n = k != NULL ? strlen(k) + 1u : 0;
        if (n == 0) {
            continue;
        }
        if (used + n > job->out_cap) {
            job->truncated++;
            continue;
        }
        memcpy(job->out + used, k, n);
        used += (uint32_t)n;
        job->count++;
    }
    if (rc != SQLITE_DONE) {
        set_error(job, "bad_value");
    }
}

void sel_library_search(sel_library* lib, sel_lib_job* job) {
    sqlite3_stmt* st = NULL;
    char sql[SEL_QUERY_SQL_MAX + 40];
    char match[SEL_QUERY_TEXT_MAX * 2];
    unsigned i;

    job->count = job->truncated = job->used_fts = 0;
    job->error[0] = '\0';
    if (is_bare_words(job->text)) {
        const char* p = job->text;
        size_t o = 0;
        while (*p != '\0' && o + 6 < sizeof match) {
            if (*p == ' ') {
                match[o++] = *p++;
                continue;
            }
            match[o++] = '"';
            while (*p != '\0' && *p != ' ' && o + 4 < sizeof match) {
                match[o++] = *p++;
            }
            match[o++] = '"';
            match[o++] = '*';
        }
        match[o] = '\0';
        if (sqlite3_prepare_v2(lib->db,
                               "SELECT songs.key FROM song_fts JOIN songs ON songs.id = song_fts.rowid"
                               " WHERE song_fts MATCH ?1 ORDER BY songs.key",
                               -1, &st, NULL) != SQLITE_OK) {
            set_error(job, "bad_value");
            return;
        }
        sqlite3_bind_text(st, 1, match, -1, SQLITE_STATIC);
        job->used_fts = 1;
    } else {
        lib->view_used = 0;
        if (!sel_query_compile(job->text, job->now, view_lookup, lib, &lib->query)) {
            set_error(job, lib->query.error);
            return;
        }
        sqlite3_snprintf((int)sizeof sql, sql, "%s ORDER BY songs.key", lib->query.sql);
        if (sqlite3_prepare_v2(lib->db, sql, -1, &st, NULL) != SQLITE_OK) {
            set_error(job, "bad_value");
            return;
        }
        for (i = 0; i < lib->query.nparams; i++) {
            const sel_query_param* p = &lib->query.params[i];
            if (p->is_int) {
                sqlite3_bind_int64(st, (int)i + 1, p->i);
            } else {
                sqlite3_bind_text(st, (int)i + 1, p->s, -1, SQLITE_STATIC);
            }
        }
    }
    collect(lib, st, job);
    sqlite3_finalize(st);
}

void sel_library_save_view(sel_library* lib, sel_lib_job* job) {
    sqlite3_stmt* st = NULL;

    job->error[0] = '\0';
    lib->view_used = 0;
    if (!sel_query_compile(job->text2, job->now, view_lookup, lib, &lib->query)) {
        set_error(job, lib->query.error);
        return;
    }
    if (sqlite3_prepare_v2(lib->db, "INSERT OR REPLACE INTO views(name, query) VALUES(?1, ?2)", -1, &st, NULL) !=
        SQLITE_OK) {
        set_error(job, "bad_value");
        return;
    }
    sqlite3_bind_text(st, 1, job->text, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, job->text2, -1, SQLITE_STATIC);
    if (sqlite3_step(st) != SQLITE_DONE) {
        set_error(job, "bad_value");
    }
    sqlite3_finalize(st);
    job->count = 1;
}

/* TSV columns: key, source, path, name, artist, format, year. `#` lines and blank lines are skipped. */
void sel_library_import(sel_library* lib, sel_lib_job* job) {
    sqlite3_stmt* st = NULL;
    const char* p = job->text;

    job->count = 0;
    job->error[0] = '\0';
    if (sqlite3_exec(lib->db, "BEGIN", NULL, NULL, NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(lib->db,
                           "INSERT INTO songs(key, source, path, name, artist, format, year)"
                           " VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7) ON CONFLICT(key) DO UPDATE SET"
                           " source = excluded.source, path = excluded.path, name = excluded.name,"
                           " artist = excluded.artist, format = excluded.format, year = excluded.year",
                           -1, &st, NULL) != SQLITE_OK) {
        sqlite3_exec(lib->db, "ROLLBACK", NULL, NULL, NULL);
        set_error(job, "bad_value");
        return;
    }
    while (*p != '\0') {
        char line[512];
        size_t n = strcspn(p, "\n");
        char* f[7];
        unsigned nf = 0;
        char* c;
        unsigned i;

        if (n >= sizeof line) {
            p += n + (p[n] == '\n');
            continue;
        }
        memcpy(line, p, n);
        line[n] = '\0';
        p += n + (p[n] == '\n');
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        for (c = line; nf < 7; c++) {
            f[nf++] = c;
            c = strchr(c, '\t');
            if (c == NULL) {
                break;
            }
            *c = '\0';
        }
        if (nf < 7) {
            continue;
        }
        sqlite3_reset(st);
        for (i = 0; i < 6; i++) {
            sqlite3_bind_text(st, (int)i + 1, f[i], -1, SQLITE_STATIC);
        }
        sqlite3_bind_int(st, 7, atoi(f[6]));
        if (sqlite3_step(st) == SQLITE_DONE) {
            job->count++;
        }
    }
    sqlite3_finalize(st);
    sqlite3_exec(lib->db, "COMMIT", NULL, NULL, NULL);
}
