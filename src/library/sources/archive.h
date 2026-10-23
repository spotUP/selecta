/* Archive adapters for the `local` source (plan Phase 4, I159, I159a, D-M3-7): zip through vendored
 * miniz, lha/lzh through vendored lhasa, LZX (Amiga) through archive_lzx.c, all reading from a memory
 * buffer. An archive is a folder of its members: the scan visits every member as if it were a file at
 * `<archive path>/<member>`. A PowerPacker (PP20) file is not a folder but one packed file: it is
 * depacked by archive_pp20.c and scanned in place of the file itself. */
#ifndef SELECTA_LIBRARY_SOURCES_ARCHIVE_H
#define SELECTA_LIBRARY_SOURCES_ARCHIVE_H

#include <selecta/host.h>
#include <stddef.h>
#include <stdint.h>

enum {
    SEL_ARCHIVE_NONE = 0,
    SEL_ARCHIVE_ZIP = 1,
    SEL_ARCHIVE_LHA = 2,
    SEL_ARCHIVE_LZX = 3,
    SEL_ARCHIVE_PP20 = 4 /* packed single file, not a folder */
};

/* Kind of archive by content (zip local header, "-lh?-" / "-lz?-" method id, "LZX", "PP20"), else
 * SEL_ARCHIVE_NONE. */
int sel_archive_kind(const uint8_t* data, size_t size);

/* Whether `kind` is a folder of members (everything except SEL_ARCHIVE_PP20). */
int sel_archive_is_folder(int kind);

/* Called once per file member with its name (slash separated) and extracted bytes, valid during the
 * call only. Return 0 to stop. In list mode `data` is NULL and `size` is the unpacked size. */
typedef int (*sel_archive_member_fn)(void* ctx, const char* name, const uint8_t* data, size_t size);

/* Extracts every file member no larger than max_member (larger ones are skipped) and hands it to
 * `fn`. Returns the number of members delivered, or -1 when the archive is damaged. */
int sel_archive_each(const sel_host* host, int kind, const uint8_t* data, size_t size, size_t max_member,
                     sel_archive_member_fn fn, void* ctx);

/* Same walk without extraction (LIBRARY_BROWSE): `fn` gets data == NULL and the unpacked size. */
int sel_archive_list(const sel_host* host, int kind, const uint8_t* data, size_t size,
                     sel_archive_member_fn fn, void* ctx);

/* LZX walk (archive_lzx.c); list_only as above. */
int sel_lzx_walk(const sel_host* host, const uint8_t* data, size_t size, size_t max_member, int list_only,
                 sel_archive_member_fn fn, void* ctx);

/* PowerPacker PP20 decrunch (archive_pp20.c). Returns a host-allocated buffer (free with
 * host->mem_free) and sets *out_size, or NULL when damaged or larger than max_out. */
uint8_t* sel_pp20_depack(const sel_host* host, const uint8_t* data, size_t size, size_t max_out,
                         size_t* out_size);

#endif /* SELECTA_LIBRARY_SOURCES_ARCHIVE_H */
