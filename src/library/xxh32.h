/* xxHash32 and the UADE songdb key (design doc 9.2 genre key; DEViLBOX moduleGenre.ts computeUadeKey).
 * The key names a module without its path: 8 hex digits of xxh32 (seed 0) over the first 262144 bytes, then
 * 4 hex digits of the full byte length modulo 65536. The same 12 characters index the DEViLBOX genre and
 * songdb endpoints and the genre_cache table. */
#ifndef SELECTA_LIBRARY_XXH32_H
#define SELECTA_LIBRARY_XXH32_H

#include <stddef.h>
#include <stdint.h>

#define SEL_UADE_KEY_HASH_BYTES 262144u
#define SEL_UADE_KEY_CHARS 12u

uint32_t sel_xxh32(const void* data, size_t len, uint32_t seed);

/* `head` holds the first min(total_len, 262144) bytes; `total_len` is the full (decompressed) length.
 * Writes 12 lowercase hex digits and a NUL into `out`. */
void sel_uade_key(const void* head, size_t head_len, uint64_t total_len, char out[SEL_UADE_KEY_CHARS + 1u]);

/* Reads the head of the file at `path` (up to `cap` bytes) into buf; *got = bytes read. Returns the full file
 * length, or -1 when the file cannot be read. */
long sel_file_head(const char* path, void* buf, size_t cap, size_t* got);

/* The UADE key of the file at `path` (head read, then sel_uade_key). 1 = done, 0 = unreadable. */
int sel_uade_key_file(const char* path, char out[SEL_UADE_KEY_CHARS + 1u]);

#endif
