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
 *                 6 md5 bytes, u24 entry.
 *   uadecredits.bin "UCRD", u32 version (1), u32 entry count, u32 byte offset of the pool. Then (count + 1) u32 row
 *                 indexes (entry e owns rows [idx[e], idx[e+1])), then rows of 11 bytes sorted by entry: u8 source
 *                 (index into sel_uadecredit_source_name), u24 authors, u24 publishers, u24 album, u8 year - 1900.
 *                 Own pool (NUL-terminated UTF-8, ref 0 = empty, refs are offsets from the pool start). Rows equal
 *                 to the distilled uadeinfo metadata are not stored.
 *   uadeprov.bin  "UPRV", u32 version (1), u32 entry count, u32 byte offset of the source table. Then (count + 1) u32
 *                 id indexes, then u16 source ids sorted per entry. Source table: u32 count, then per source u8 kind
 *                 (SEL_UPRV_*), file base name NUL, display name NUL. A source id is its table position. */
#ifndef SELECTA_LIBRARY_UADEDB_H
#define SELECTA_LIBRARY_UADEDB_H

#include <stddef.h>
#include <stdint.h>

#define SEL_UADEINFO_NAME "uadeinfo.bin"
#define SEL_UADEMD5_NAME "uademd5.bin"
#define SEL_UADEINFO_ROW 13u
#define SEL_UADEMD5_ROW 9u

#define SEL_UADECREDITS_NAME "uadecredits.bin"
#define SEL_UADEPROV_NAME "uadeprov.bin"
#define SEL_UADECREDITS_ROW 11u
#define SEL_UADECREDIT_SOURCES 9u
enum { SEL_UPRV_SITE = 0, SEL_UPRV_COLLECTION, SEL_UPRV_GROUP, SEL_UPRV_BBS, SEL_UPRV_CD, SEL_UPRV_KINDS };

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

typedef struct sel_uadecredit {
    uint32_t source; /* index for sel_uadecredit_source_name */
    char authors[256];
    char publishers[128];
    char album[128];
    uint32_t year; /* 0 = unknown */
} sel_uadecredit;

/* "AMP", "Modland", ... for a credit source index; "" when out of range. */
const char* sel_uadecredit_source_name(uint32_t source);

/* Lists the per-source credit rows of `entry` into out[0..max); returns the row count stored (0 = none or bad file). */
int sel_uadecredits_get(const char* path, uint32_t entry, sel_uadecredit* out, int max);

/* Source ids of `entry` into ids[0..max); returns the count stored. */
int sel_uadeprov_get(const char* path, uint32_t entry, uint16_t* ids, int max);

/* Source table lookup: kind (SEL_UPRV_*) and the strings, cut to the buffer sizes. 1 = found. */
int sel_uadeprov_source(const char* path, uint32_t id, int* kind, char* file, size_t filecap, char* display, size_t dispcap);

/* I774n R8: fills the song info input (include/selecta/uimodel/siinfo.h) for the song with UADE key `key` from the bake files
 * in directory `dir` (songlengths.bin, uadeinfo.bin, uadecredits.bin, uadeprov.bin). 1 = the key is in the bake (a missing
 * file leaves its part empty), 0 = not in the bake. *out is zeroed first. */
struct sel_uim_si_input;
int sel_uadesong_load(const char* dir, const char* key, struct sel_uim_si_input* out);

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
    long md5_collision; /* mismatches on a key two or more md5s share (excused; the xxh32 row stays) */
    long sl_checked, sl_mismatch, sl_whitelisted, sl_collision, sl_twin_unmapped; /* songlengths md5 twin check */
    long credit_kept[SEL_UADECREDIT_SOURCES], credit_dropped[SEL_UADECREDIT_SOURCES], credit_unmapped[SEL_UADECREDIT_SOURCES];
    long credit_rows, credit_pool_bytes, credit_bytes;
    long prov_sources[SEL_UPRV_KINDS], prov_named[SEL_UPRV_KINDS]; /* sources per kind; named = display from sources.md */
    long prov_pairs, prov_unmapped, prov_bytes;
} sel_uadedb_stats;

int sel_uadedb_bake(const char* checkout, const char* outdir, sel_uadedb_stats* st);

#endif
