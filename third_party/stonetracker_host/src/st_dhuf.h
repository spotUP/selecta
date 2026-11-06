/* st_dhuf.h - StoneTracker DeltaHuffman sample-bank depacker (see st_dhuf.c). */
#ifndef ST_DHUF_H
#define ST_DHUF_H
#include <stddef.h>
#include <stdint.h>

/* Unpacked length of a 'psn' stream, or -1 if src is not one. */
int st_dhuf_unpacked_length(const uint8_t *src, size_t srcLen);

/* Depack a 'psn' stream into dst. srcBaseOffset is the stream's offset from
 * an even (allocation-aligned) address - the coded data starts on the next
 * even address of the bank. Returns the unpacked length, or -1. */
int st_dhuf_depack(const uint8_t *src, size_t srcLen, size_t srcBaseOffset,
                   uint8_t *dst, size_t dstLen);
#endif
