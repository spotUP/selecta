/* Directory iteration, one implementation per platform under
 * src/core/<os>/dir.c (RULES.md rule 3: OS headers live only there). The
 * library code above it is portable. */
#ifndef SELECTA_LIBRARY_DIR_H
#define SELECTA_LIBRARY_DIR_H

#include <stdint.h>

/* Called once per entry other than "." and "..". Return 0 to stop early. */
typedef int (*sel_dir_fn)(void* ctx, const char* name, int is_dir);

/* Lists `path`. Returns 0 when the directory cannot be opened, else 1. */
int sel_dir_each(const char* path, sel_dir_fn fn, void* ctx);

/* Modification time of directory `path` in an OS-specific unit (nanoseconds on POSIX, 1/50 s on the
 * Amiga); only ever compared for equality. -1 when the directory cannot be read. */
int64_t sel_dir_mtime(const char* path);

/* Size in bytes and modification time (same unit as sel_dir_mtime, ordered) of regular file `path`;
 * 0 when it cannot be read. Used by the cache trim (I167). */
int sel_file_stat(const char* path, uint32_t* size, int64_t* mtime);
/* Sets the modification time of `path` to now: the cache's last-access stamp. 0 on failure. */
int sel_file_touch(const char* path);

#endif /* SELECTA_LIBRARY_DIR_H */
