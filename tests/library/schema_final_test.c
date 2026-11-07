/* I625a8c: a fresh library.db must come from ONE consolidated final schema (SCHEMA_FINAL), not from replaying the 18 migrations: the
 * ALTER and rebuild steps cost seconds on a 68020 (v1 1137 ms, v7 670 ms, v12 709 ms; each ALTER TABLE ADD COLUMN reloads the schema).
 * Invariants checked here, both on in-memory databases:
 *   1. the fresh path (sel_schema_migrate on user_version 0) executes no ALTER statement;
 *   2. it leaves exactly the schema the migration chain leaves (sel_schema_migrate_chain): sqlite_master rows with normalised sql,
 *      the column list, index list and index columns of every table, and the rows every table holds (seeded rows);
 *   3. both are at user_version SEL_LIB_SCHEMA_VERSION and pass PRAGMA integrity_check.
 * Drift guard: a migration appended to g_migrations without the same change in SCHEMA_FINAL fails (2).
 * `schema_final_test --dump` prints the chain database's DDL in creation order (how SCHEMA_FINAL was derived).
 * Sentinel: "schema_final_ok". Design: thoughts/spot/todos.md row I625a8c.
 *
 * `schema_final_test --template <library-template.db>` (I625a8e, ctest library-template-schema): the build's prebuilt template database
 * equals a fresh in-memory SCHEMA_FINAL database (sqlite_master rows and user_version), and sel_library_open on a missing path copies the
 * template that sits next to it (trace "template copied" fires, "final schema created v" does not) and yields the same dump.
 * Sentinel: "template_ok". */
#include "db.h"
#include "schema.h"
#include <selecta/host.h>
#include <selecta/machine.h>
#include <sqlite3.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static int g_alter;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static int on_trace(unsigned type, void* ctx, void* p, void* x) {
    const char* sql = (const char*)x;

    (void)ctx;
    (void)p;
    if (type == SQLITE_TRACE_STMT && sql != NULL) {
        while (isspace((unsigned char)*sql)) sql++;
        if (sql[0] != '-' && strncasecmp(sql, "ALTER", 5) == 0) g_alter++;
    }
    return 0;
}

/* Lower case, no quotes, whitespace collapsed to nothing around punctuation and to one space elsewhere. */
static void normalise(const char* in, char* out, size_t cap) {
    size_t n = 0;
    int space = 0;

    for (; *in != '\0' && n + 2 < cap; in++) {
        char c = *in;

        if (c == '"' || c == '`') continue;
        if (isspace((unsigned char)c)) {
            space = 1;
            continue;
        }
        if (space && n > 0 && strchr("(),;", c) == NULL && strchr("(,", out[n - 1]) == NULL) out[n++] = ' ';
        space = 0;
        out[n++] = (char)tolower((unsigned char)c);
    }
    out[n] = '\0';
}

/* All rows of `sql` flattened to one string (columns '|', rows '\n'), sql columns normalised when `norm`. */
static char* flatten(sqlite3* db, const char* sql, int norm) {
    sqlite3_stmt* st = NULL;
    size_t cap = 4096, len = 0;
    char* out = malloc(cap);

    out[0] = '\0';
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        printf("FAIL prepare: %s -> %s\n", sql, sqlite3_errmsg(db));
        g_fail++;
        return out;
    }
    while (sqlite3_step(st) == SQLITE_ROW) {
        int i;

        for (i = 0; i < sqlite3_column_count(st); i++) {
            const char* t = (const char*)sqlite3_column_text(st, i);
            char* tmp = malloc(strlen(t != NULL ? t : "") + 4);
            size_t need;

            if (t == NULL) t = "<null>";
            if (norm) {
                free(tmp);
                tmp = malloc(strlen(t) + 4);
                normalise(t, tmp, strlen(t) + 4);
            } else {
                strcpy(tmp, t);
            }
            need = len + strlen(tmp) + 3;
            if (need > cap) {
                while (need > cap) cap *= 2;
                out = realloc(out, cap);
            }
            len += (size_t)sprintf(out + len, "%s%c", tmp, i + 1 < sqlite3_column_count(st) ? '|' : '\n');
            free(tmp);
        }
    }
    sqlite3_finalize(st);
    return out;
}

static void same(sqlite3* a, sqlite3* b, const char* what, const char* sql, int norm) {
    char* x = flatten(a, sql, norm);
    char* y = flatten(b, sql, norm);

    if (strcmp(x, y) != 0) {
        const char* p = x;
        const char* q = y;

        while (*p != '\0' && *p == *q) p++, q++;
        printf("FAIL %s differs near:\n  fresh: %.160s\n  chain: %.160s\n", what, p, q);
        g_fail++;
    }
    free(x);
    free(y);
}

static void compare(sqlite3* a, sqlite3* b) {
    sqlite3_stmt* st = NULL;

    same(a, b, "sqlite_master", "SELECT type, name, tbl_name, sql FROM sqlite_master ORDER BY type, name", 1);
    same(a, b, "table columns",
         "SELECT m.name, c.name, c.type, c.\"notnull\", c.dflt_value, c.pk FROM sqlite_master m, pragma_table_info(m.name) c "
         "WHERE m.type IN ('table','view') ORDER BY m.name, c.cid",
         0);
    same(a, b, "index list", "SELECT m.name, i.name, i.\"unique\", i.origin, i.partial FROM sqlite_master m, pragma_index_list(m.name) i "
                              "WHERE m.type = 'table' ORDER BY m.name, i.name",
         0);
    same(a, b, "index columns", "SELECT m.name, c.seqno, c.name FROM sqlite_master m, pragma_index_info(m.name) c "
                                "WHERE m.type = 'index' ORDER BY m.name, c.seqno",
         0);
    /* Rows the migrations seed (composer_unknown holds one counter row). */
    if (sqlite3_prepare_v2(a, "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name", -1, &st,
                           NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            char sql[200];

            snprintf(sql, sizeof sql, "SELECT * FROM \"%s\" ORDER BY 1", (const char*)sqlite3_column_text(st, 0));
            same(a, b, (const char*)sqlite3_column_text(st, 0), sql, 0);
        }
    }
    sqlite3_finalize(st);
}

static void integrity(sqlite3* db, const char* what) {
    char* r = flatten(db, "PRAGMA integrity_check", 0);

    if (strcmp(r, "ok\n") != 0) {
        printf("FAIL integrity_check %s: %s", what, r);
        g_fail++;
    }
    free(r);
}

/* Everything that defines the schema, as one string. */
static char* dump(sqlite3* db) {
    char* a = flatten(db, "SELECT type, name, tbl_name, sql FROM sqlite_master ORDER BY type, name", 0);
    char* b = flatten(db, "PRAGMA user_version", 0);
    char* out = malloc(strlen(a) + strlen(b) + 16);

    sprintf(out, "%suser_version=%s", a, b);
    free(a);
    free(b);
    return out;
}

static char* slurp(const char* path, size_t* n) {
    FILE* f = fopen(path, "rb");
    char* buf = malloc(1 << 20);

    *n = f != NULL ? fread(buf, 1, (1 << 20) - 1, f) : 0;
    if (f != NULL) fclose(f);
    buf[*n] = '\0';
    return buf;
}

static int count_of(const char* hay, const char* needle) {
    int n = 0;

    for (; (hay = strstr(hay, needle)) != NULL; hay++) n++;
    return n;
}

static int template_checks(const sel_host* host, const char* tpl) {
    sqlite3* fresh = NULL;
    sqlite3* t = NULL;
    sqlite3* o = NULL;
    char dir[512], db[600], copy[600], logp[600];
    char *want, *got, *got2, *log;
    size_t n, in_n;
    sel_library* lib;
    FILE *in, *out;
    char buf[4096];

    CHECK(sqlite3_open(":memory:", &fresh) == SQLITE_OK);
    CHECK(sel_schema_migrate(fresh, host) == 1);
    want = dump(fresh);
    CHECK(sqlite3_open_v2(tpl, &t, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    got = dump(t);
    if (strcmp(want, got) != 0) {
        printf("FAIL template dump differs from SCHEMA_FINAL\n  want: %.200s\n  got:  %.200s\n", want, got);
        g_fail++;
    }
    sqlite3_close(t);
    /* sel_library_open on a missing path with the template beside it. */
    snprintf(dir, sizeof dir, "%s.d", tpl);
    snprintf(copy, sizeof copy, "%s/" SEL_LIBTEMPLATE_NAME, dir);
    snprintf(db, sizeof db, "%s/library.db", dir);
    snprintf(logp, sizeof logp, "%s.log", tpl);
    {
        char cmd[1300];
        snprintf(cmd, sizeof cmd, "rm -rf '%s' && mkdir '%s'", dir, dir);
        CHECK(system(cmd) == 0);
    }
    in = fopen(tpl, "rb");
    out = fopen(copy, "wb");
    CHECK(in != NULL && out != NULL);
    while (in != NULL && out != NULL && (n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
    if (in != NULL) fclose(in);
    if (out != NULL) fclose(out);
    CHECK(freopen(logp, "w", stderr) != NULL);
    sel_library_trace_enable(1);
    lib = sel_library_open(host, sel_machine_fake("a060"), 1, db);
    sel_library_trace_enable(0);
    fflush(stderr);
    CHECK(lib != NULL);
    if (lib != NULL) sel_library_close(lib);
    log = slurp(logp, &in_n);
    CHECK(count_of(log, "template copied") == 1);
    CHECK(count_of(log, "final schema created v") == 0);
    free(log);
    CHECK(sqlite3_open_v2(db, &o, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    got2 = dump(o);
    /* The library adds meta rows, not schema: the dump of sqlite_master and user_version must stay equal. */
    if (strcmp(want, got2) != 0) {
        printf("FAIL library opened from the template has another schema\n  want: %.200s\n  got:  %.200s\n", want, got2);
        g_fail++;
    }
    sqlite3_close(o);
    sqlite3_close(fresh);
    free(want);
    free(got);
    free(got2);
    if (g_fail == 0) puts("template_ok");
    return g_fail != 0;
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    sqlite3* fresh = NULL;
    sqlite3* chain = NULL;

    if (argc > 2 && strcmp(argv[1], "--template") == 0) return template_checks(host, argv[2]);
    CHECK(sqlite3_open(":memory:", &fresh) == SQLITE_OK);
    CHECK(sqlite3_open(":memory:", &chain) == SQLITE_OK);
    CHECK(sel_schema_migrate_chain(chain, host) == 1);
    if (argc > 1 && strcmp(argv[1], "--dump") == 0) {
        char* d = flatten(chain, "SELECT sql || ';' FROM sqlite_master WHERE sql IS NOT NULL ORDER BY rowid", 0);

        fputs(d, stdout);
        free(d);
        return 0;
    }
    sqlite3_trace_v2(fresh, SQLITE_TRACE_STMT, on_trace, NULL);
    CHECK(sel_schema_migrate(fresh, host) == 1);
    sqlite3_trace_v2(fresh, 0, NULL, NULL);
    printf("fresh path ALTER statements: %d\n", g_alter);
    CHECK(g_alter == 0);
    CHECK(sel_schema_version(fresh) == SEL_LIB_SCHEMA_VERSION);
    CHECK(sel_schema_version(chain) == SEL_LIB_SCHEMA_VERSION);
    integrity(fresh, "fresh");
    integrity(chain, "chain");
    compare(fresh, chain);
    sqlite3_close(fresh);
    sqlite3_close(chain);
    if (g_fail == 0) puts("schema_final_ok");
    return g_fail != 0;
}
