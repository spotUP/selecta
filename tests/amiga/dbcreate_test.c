/* Amiga test program (I225): a new library database survives create, write, shrink and reopen on libnix.
 * Two libnix habits broke this on hardware while vamos hid them:
 * - lseek() past the end of a file grows the file with dos Write() from an uninitialised buffer. SQLite's
 *   lseek + read fallback seeks to byte 24 of a new, empty database to read the change counter, so the file
 *   became 24 junk bytes and every later open failed with SQLITE_NOTADB. vamos does not grow the file, so this
 *   program is linked with --wrap=lseek and counts every seek past the end instead.
 * - ftruncate() returns the new size (dos SetFileSize) on success, not 0, and SQLite reports any nonzero result
 *   as SQLITE_IOERR_TRUNCATE. vamos does not implement SetFileSize and returns 0, so the wrapper below returns
 *   the size the way libnix does on a real Amiga.
 * Usage: selecta-dbcreate-test <db path>. Prints the counts and db_create=ok, or exits non-zero. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "amiga_start.h"
#include "db.h"
#include "sqlite3.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

off_t __real_lseek(int fd, off_t offset, int whence);
off_t __wrap_lseek(int fd, off_t offset, int whence);
int __real_ftruncate(int fd, off_t length);
int __wrap_ftruncate(int fd, off_t length);

static unsigned g_past_end_seeks;
static unsigned g_truncates;

off_t __wrap_lseek(int fd, off_t offset, int whence) {
    if (whence == SEEK_SET) {
        off_t here = __real_lseek(fd, 0, SEEK_CUR);
        off_t end = __real_lseek(fd, 0, SEEK_END);

        __real_lseek(fd, here, SEEK_SET);
        if (end >= 0 && offset > end) {
            g_past_end_seeks++;
        }
    }
    return __real_lseek(fd, offset, whence);
}

int __wrap_ftruncate(int fd, off_t length) {
    int rc = __real_ftruncate(fd, length);

    g_truncates++;
    return rc == 0 ? (int)length : rc;
}

static int exec_sql(sqlite3* db, const char* sql) {
    char* err = NULL;

    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stdout, "sql_failed=%s (%s)\n", sql, err != NULL ? err : "?");
        sqlite3_free(err);
        return 0;
    }
    return 1;
}

static int has_header(const char* path) {
    char head[16];
    FILE* f = fopen(path, "rb");
    size_t n;

    if (f == NULL) {
        return 0;
    }
    n = fread(head, 1, sizeof head, f);
    fclose(f);
    return n == sizeof head && memcmp(head, "SQLite format 3", 16) == 0;
}

static int app_main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    sel_library* lib;
    sqlite3* db = NULL;
    int ok;

    if (argc != 2) {
        fputs("usage: selecta-dbcreate-test <db path>\n", stderr);
        return 2;
    }
    lib = sel_library_open(host, sel_machine_probe(host), 0, argv[1]);
    if (lib == NULL) {
        fputs("db_create=open_failed\n", stdout);
        return 1;
    }
    /* A second connection through the default ("amiga") VFS grows the file, then shrinks it: VACUUM truncates. */
    ok = sqlite3_open_v2(argv[1], &db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK &&
         exec_sql(db, "CREATE TABLE grow(x); BEGIN;"
                      "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 400)"
                      " INSERT INTO grow SELECT zeroblob(500) FROM n; COMMIT;") &&
         exec_sql(db, "DROP TABLE grow;") && exec_sql(db, "VACUUM;");
    sqlite3_close(db);
    sel_library_close(lib);
    if (!ok) {
        fputs("db_create=write_failed\n", stdout);
        return 1;
    }
    lib = sel_library_open(host, sel_machine_probe(host), 0, argv[1]);
    if (lib == NULL) {
        fputs("db_create=reopen_failed\n", stdout);
        return 1;
    }
    sel_library_close(lib);
    fprintf(stdout, "past_end_seeks=%u truncates=%u header=%d\n", g_past_end_seeks, g_truncates, has_header(argv[1]));
    if (g_past_end_seeks != 0 || g_truncates == 0 || !has_header(argv[1])) {
        fputs("db_create=bad_file\n", stdout);
        return 1;
    }
    fputs("db_create=ok\n", stdout);
    return 0;
}

int main(int argc, char** argv) {
    return sel_amiga_run_with_stack(app_main, argc, argv, SEL_AMIGA_STACK_BYTES);
}
