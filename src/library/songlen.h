/* Song length database (I605, feature F1): the audacious-uade-tools songlengths.tsv baked by tools/songlen_bake.c
 * into one sorted binary file, looked up by the UADE content key (xxh32.h) with a binary search over seeks. The file
 * is never loaded whole, so a 470k-row database costs a few hundred bytes of RAM on a 68k Amiga.
 *
 * File layout, all integers big-endian:
 *   header  16 bytes: "SLDB", u32 version (1), u32 entry count, u32 byte offset of the length array
 *   entries 12 bytes each, sorted by key: 6 key bytes (the 12 hex digits), u8 first subsong, u8 subsong count,
 *           u32 index of this entry's first length in the length array
 *   lengths u32 each: milliseconds, 0 = unknown
 * Version 2 (I774k) packs the upstream per-subsong flags into the top 5 bits of each length: bit 31 = duplicate
 * subsong ("!"), bits 30..27 = songend type code (sel_songend_name); milliseconds are the low 27 bits (37 hours).
 * Readers mask the flags for version 2 and read version 1 files (no flag bits) unchanged. */
#ifndef SELECTA_LIBRARY_SONGLEN_H
#define SELECTA_LIBRARY_SONGLEN_H

#include <stddef.h>
#include <stdint.h>

#define SEL_SONGLEN_NAME "songlengths.bin"
#define SEL_SONGLEN_HEADER 16u
#define SEL_SONGLEN_ENTRY 12u
#define SEL_SONGLEN_MAX_SUBSONGS 255u
#define SEL_SONGLEN_MS_MASK 0x07ffffffu
#define SEL_SONGLEN_DUP_BIT 0x80000000u
#define SEL_SONGLEN_END_SHIFT 27u

/* Songend types seen in the upstream database; the code is the index (0 = unknown). */
static const char* const sel_songend_names[] = {"", "p", "p+s", "p+v", "n", "l", "s", "e", "t", "r", "v", "l+v", "l+s"};
#define SEL_SONGEND_COUNT 13u

static inline const char* sel_songend_name(unsigned code) {
    return code < SEL_SONGEND_COUNT ? sel_songend_names[code] : "";
}

/* Code of the `len`-char songend token, 0 when not in the table. */
static inline unsigned sel_songend_code(const char* s, size_t len) {
    unsigned i, j;
    for (i = 1; i < SEL_SONGEND_COUNT; i++) {
        for (j = 0; j < len && sel_songend_names[i][j] == s[j]; j++) {}
        if (j == len && sel_songend_names[i][j] == '\0') return i;
    }
    return 0;
}

/* Parses 12 hex digits into 6 bytes. 1 = ok. Inline: the reader (selecta_library) and the baker share it without
 * the baker linking the library. */
static inline int sel_songlen_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static inline int sel_songlen_key_bytes(const char* hex12, uint8_t out[6]) {
    unsigned i;
    for (i = 0; i < 6u; i++) {
        int hi = sel_songlen_hexval(hex12[i * 2u]), lo = hi < 0 ? -1 : sel_songlen_hexval(hex12[i * 2u + 1u]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (uint8_t)(hi * 16 + lo);
    }
    return 1;
}

/* Looks up `key` (12 lowercase hex digits, sel_uade_key) in the database file `path`, subsong `subsong` (the
 * database's own numbering: first subsong is the entry's `first`). 1 = *ms set (non-zero), 0 = file missing, key or
 * subsong not in it, or length unknown. `reads` (optional) counts the fread calls of the search. */
int sel_songlen_lookup(const char* path, const char* key, uint32_t subsong, uint32_t* ms, uint32_t* reads);

/* Like sel_songlen_lookup, also returning the songend type code and the duplicate flag (both optional; always 0
 * for a version 1 file). `entry` (optional) receives the entry index (the key position in the sorted table, the
 * index of the uadeinfo/uademd5 files). */
int sel_songlen_lookup_ex(const char* path, const char* key, uint32_t subsong, uint32_t* ms, uint32_t* songend,
                          uint32_t* dup, uint32_t* entry);

/* Entry index of `key` in the database file, 1 = found. Works even when the entry's lengths are all unknown. */
int sel_songlen_find(const char* path, const char* key, uint32_t* entry);

/* Builds the file from TSV text (one row per line: key TAB first TAB "ms,flags" entries separated by spaces).
 * Returns the entry count written, or -1 on error. `out_bytes` (optional) receives the file size. */
long sel_songlen_bake(const char* tsv_path, const char* out_path, long* out_bytes);

#endif
