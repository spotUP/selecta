/* SQLite file names on AmigaOS (I197/T96). SQLite's unix VFS turns a name that does not start with '/' into
 * "<getcwd()>/<name>". An AmigaDOS path never starts with '/' ("work:user.db", "Songs/a.db") and libnix
 * getcwd() needs dos.library GetCurrentDirName, so every file database failed to open with "unable to open
 * database file"; only ":memory:" worked. AmigaDOS resolves relative names itself, so the full path of a name
 * is the name. libnix fcntl() byte-range locks fail too (IOERR_LOCK), and one task owns the file, so the base is
 * the unix-none VFS (no file locking). This wraps it and replaces only xFullPathname. */
#include "db_internal.h"

#ifdef __amigaos__
#include <string.h>

static sqlite3_vfs g_amiga_vfs;

static int amiga_full_pathname(sqlite3_vfs* vfs, const char* name, int n, char* out) {
    size_t len = strlen(name);

    (void)vfs;
    if (len + 1 > (size_t)n) {
        return SQLITE_CANTOPEN;
    }
    memcpy(out, name, len + 1);
    return SQLITE_OK;
}

void sel_library_vfs_init(void) {
    sqlite3_vfs* base;

    /* sqlite3_shutdown() (last library closed) drops every registered VFS: register again on the next open. */
    if (sqlite3_vfs_find("amiga") != NULL) {
        return;
    }
    base = sqlite3_vfs_find("unix-none");
    if (base == NULL) {
        return;
    }
    g_amiga_vfs = *base;
    g_amiga_vfs.zName = "amiga";
    g_amiga_vfs.pNext = NULL;
    g_amiga_vfs.xFullPathname = amiga_full_pathname;
    sqlite3_vfs_register(&g_amiga_vfs, 1);
}
#else
void sel_library_vfs_init(void) {}
#endif
