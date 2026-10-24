#define _POSIX_C_SOURCE 200809L /* ftruncate, pread, pwrite in libnix unistd.h */
/* SQLite file names on AmigaOS (I197/T96). SQLite's unix VFS turns a name that does not start with '/' into
 * "<getcwd()>/<name>". An AmigaDOS path never starts with '/' ("work:user.db", "Songs/a.db") and libnix
 * getcwd() needs dos.library GetCurrentDirName, so every file database failed to open with "unable to open
 * database file"; only ":memory:" worked. AmigaDOS resolves relative names itself, so the full path of a name
 * is the name. libnix fcntl() byte-range locks fail too (IOERR_LOCK), and one task owns the file, so the base is
 * the unix-none VFS (no file locking). This wraps it and replaces xFullPathname and xRandomness.
 * The unix xRandomness opens "/dev/urandom", which libnix maps to the volume "dev:": every library open showed
 * the requester "Please insert volume dev:" (I223). The seed mixes the time of day and two addresses instead, which
 * is what the unix VFS itself falls back to without /dev/urandom.
 * libnix ftruncate() returns the new size on success, not 0, and SQLite takes any nonzero result as
 * SQLITE_IOERR_TRUNCATE (I225): the syscall table gets a wrapper that returns 0 or -1. */
#include "db_internal.h"

#ifdef __amigaos__
#include "../xxh32.h"
#include <string.h>
#include <time.h>
#include <unistd.h>

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

static int amiga_ftruncate(int fd, off_t length) {
    return ftruncate(fd, length) < 0 ? -1 : 0;
}

static int amiga_randomness(sqlite3_vfs* vfs, int n, char* out) {
    uint32_t seed[3];
    int i;

    seed[0] = (uint32_t)time(NULL);
    seed[1] = (uint32_t)(uintptr_t)vfs;
    seed[2] = (uint32_t)(uintptr_t)&seed;
    for (i = 0; i < n; i += 4) {
        uint32_t h = sel_xxh32(seed, sizeof seed, (uint32_t)i);
        int k;

        for (k = 0; k < 4 && i + k < n; k++) {
            out[i + k] = (char)(h >> (8 * k));
        }
    }
    return n;
}

void sel_library_vfs_init(void) {
    sqlite3_vfs* base;

    /* sqlite3_shutdown() (last library closed) keeps registered VFSes, but the next sqlite3_initialize() registers
     * the unix VFSes again with "unix" as the default, and the unix xFullPathname needs getcwd(): a library opened
     * again in the same process failed with SQLITE_CANTOPEN. Make "amiga" the default on every open. */
    if (sqlite3_vfs_find("amiga") == NULL) {
        base = sqlite3_vfs_find("unix-none");
        if (base == NULL) {
            return;
        }
        g_amiga_vfs = *base;
        g_amiga_vfs.zName = "amiga";
        g_amiga_vfs.pNext = NULL;
        g_amiga_vfs.xFullPathname = amiga_full_pathname;
        g_amiga_vfs.xRandomness = amiga_randomness;
    }
    if (g_amiga_vfs.xSetSystemCall != NULL) {
        g_amiga_vfs.xSetSystemCall(&g_amiga_vfs, "ftruncate", (sqlite3_syscall_ptr)amiga_ftruncate);
    }
    sqlite3_vfs_register(&g_amiga_vfs, 1);
}
#else
void sel_library_vfs_init(void) {}
#endif
