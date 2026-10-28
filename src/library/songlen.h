/* Song length database (I605, feature F1): the audacious-uade-tools songlengths.tsv baked by tools/songlen_bake.c
 * into one sorted binary file, looked up by the UADE content key (xxh32.h) with a binary search over seeks. The file
 * is never loaded whole, so a 470k-row database costs a few hundred bytes of RAM on a 68k Amiga.
 *
 * File layout, all integers big-endian:
 *   header  16 bytes: "SLDB", u32 version (1), u32 entry count, u32 byte offset of the length array
 *   entries 12 bytes each, sorted by key: 6 key bytes (the 12 hex digits), u8 first subsong, u8 subsong count,
 *           u32 index of this entry's first length in the length array
 *   lengths u32 each: milliseconds, 0 = unknown */
#ifndef SELECTA_LIBRARY_SONGLEN_H
#define SELECTA_LIBRARY_SONGLEN_H

#include <stddef.h>
#include <stdint.h>

#define SEL_SONGLEN_NAME "songlengths.bin"
#define SEL_SONGLEN_HEADER 16u
#define SEL_SONGLEN_ENTRY 12u
#define SEL_SONGLEN_MAX_SUBSONGS 255u

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

/* Builds the file from TSV text (one row per line: key TAB first TAB "ms,flags" entries separated by spaces).
 * Returns the entry count written, or -1 on error. `out_bytes` (optional) receives the file size. */
long sel_songlen_bake(const char* tsv_path, const char* out_path, long* out_bytes);

#endif
