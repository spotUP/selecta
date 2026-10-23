/* Archive adapters for the `local` source (plan Phase 4, I159, D-M3-7): zip through vendored miniz
 * and lha/lzh through vendored lhasa, both reading from a memory buffer. An archive is a folder of
 * its members: the scan visits every member as if it were a file at `<archive path>/<member>`. */
#ifndef SELECTA_LIBRARY_SOURCES_ARCHIVE_H
#define SELECTA_LIBRARY_SOURCES_ARCHIVE_H

#include <selecta/host.h>
#include <stddef.h>
#include <stdint.h>

enum { SEL_ARCHIVE_NONE = 0, SEL_ARCHIVE_ZIP = 1, SEL_ARCHIVE_LHA = 2 };

/* Kind of archive by content (zip local header, "-lh?-" / "-lz?-" method id), else SEL_ARCHIVE_NONE. */
int sel_archive_kind(const uint8_t* data, size_t size);

/* Called once per file member with its name (slash separated) and extracted bytes, valid during the
 * call only. Return 0 to stop. */
typedef int (*sel_archive_member_fn)(void* ctx, const char* name, const uint8_t* data, size_t size);

/* Extracts every file member no larger than max_member (larger ones are skipped) and hands it to
 * `fn`. Returns the number of members delivered, or -1 when the archive is damaged. */
int sel_archive_each(const sel_host* host, int kind, const uint8_t* data, size_t size, size_t max_member,
                     sel_archive_member_fn fn, void* ctx);

#endif /* SELECTA_LIBRARY_SOURCES_ARCHIVE_H */
