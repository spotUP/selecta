/* Store for required system files (ROMs, disk images) a replayer declares with RP_SYSFILE (row I710ga2, plan
 * thoughts/shared/plans/2026-11-06-I710ga-system-files.md section 3). A file is identified by its sha256, never by
 * its name or URL. Stored as <config dir>/<name>; written as <name>.part, then renamed. */
#ifndef SELECTA_LIBRARY_SYSFILE_H
#define SELECTA_LIBRARY_SYSFILE_H

#include <selecta/registry.h>

#include <stddef.h>

/* 1 when <config_dir>/<d->name> exists, has d->size bytes (when set) and its sha256 is in d->sha256; the path is
 * written to `path` (may be NULL). The verdict is cached per process by (name, size, mtime): a repeat call on an
 * unchanged file does not hash again. */
int sel_sysfile_resolve(const char* config_dir, const sel_sysfile_decl* d, char* path, size_t cap);

/* Installs `src` (the file itself, or a zip with PK\\3\\4 magic holding it) as <config_dir>/<d->name>. A zip is scanned
 * member by member and the first member whose sha256 is accepted is written. 1 on success. 0 on failure with a one-line
 * reason in err; nothing is left behind (a stale <name>.part is removed). */
int sel_sysfile_install(const char* config_dir, const sel_sysfile_decl* d, const char* src, char* err, size_t cap);

/* Number of sha256 computations the store has done in this process (tests prove the cache with it). */
unsigned sel_sysfile_hash_count(void);

#endif
