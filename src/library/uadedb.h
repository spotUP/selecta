/* audacious-uade-tools derived data files beside songlengths.bin (I774, design 2026-11-17-i774-uade-tools-all.md).
 * All integers big-endian, all files read by seeks. An "entry" is the position of a key in songlengths.bin
 * (sel_songlen_find), shared by every file below.
 *
 *   uadeinfo.bin  "UINF", u32 version (1), u32 entry count, u32 byte offset of the pool.
 *                 entries 13 bytes each, indexed by entry: u16 format id, u8 channels (0 = unknown), u24 authors,
 *                 u24 publishers, u24 album (string refs), u8 year - 1900 (0 = unknown).
 *                 pool: u32 format count, count x u32 string ref per format id, then NUL-terminated UTF-8 strings.
 *                 A string ref is a byte offset from the first string; ref 0 is the empty string. Authors are
 *                 upstream's "~"-joined list.
 *   uademd5.bin   "UMD5", u32 version (1), u32 count, u32 0. Rows of 9 bytes sorted by the first 6 md5 bytes:
 *                 6 md5 bytes, u24 entry. */
#ifndef SELECTA_LIBRARY_UADEDB_H
#define SELECTA_LIBRARY_UADEDB_H

#include <stdint.h>

#define SEL_UADEINFO_NAME "uadeinfo.bin"
#define SEL_UADEMD5_NAME "uademd5.bin"
#define SEL_UADEINFO_ROW 13u
#define SEL_UADEMD5_ROW 9u

typedef struct sel_uadeinfo {
    char format[64];
    char authors[256];
    char publishers[128];
    char album[128];
    uint32_t channels; /* 0 = unknown */
    uint32_t year;     /* 0 = unknown */
} sel_uadeinfo;

/* 1 = *out filled for `entry` of the file `path`; 0 = file missing/bad or entry out of range. Strings are cut to
 * the buffer sizes. */
int sel_uadeinfo_get(const char* path, uint32_t entry, sel_uadeinfo* out);

/* md5 (at least 12 hex digits) -> entry. 1 = found. */
int sel_uademd5_lookup(const char* path, const char* md5hex, uint32_t* entry);

/* Host baker (selecta_songlen_bake): reads the audacious-uade-tools checkout `checkout` and writes
 * songlengths.bin, uadeinfo.bin and uademd5.bin into `outdir`. 0 = ok. */
typedef struct sel_uadedb_stats {
    long songlen_entries, songlen_bytes;
    long info_entries, pool_bytes, info_bytes;
    long info_unmapped; /* xxh32 modinfos/metadata rows whose key is not in songlengths */
    long md5_mapped, md5_unmapped, md5_conflict, md5_bytes;
    long md5_checked, md5_mismatch, md5_whitelisted, md5_twin_unmapped;
} sel_uadedb_stats;

int sel_uadedb_bake(const char* checkout, const char* outdir, sel_uadedb_stats* st);

#endif
