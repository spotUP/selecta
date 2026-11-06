/*
 * st_dhuf.c - StoneTracker sample-bank "DeltaHuffman" depacker
 *
 * Port of Emmanuel Marty's DHUF method from stonepacker.library 39.2
 * (Aminet mus/edit/stonefree1.lha, Libs/StonePacker.library, code hunk
 * offsets $3CC entry and $A1C decoder). The 68k routine is followed step for
 * step; the comments name the instruction each step stands for.
 *
 * Stream ('psn' + method byte), as written by the packer at $578:
 *   +0  'p' 's' 'n'
 *   +3  symbol count - 1
 *   +4  unpacked length (big-endian long)
 *   +8  per symbol: value byte, code length byte, then (length >> 3) + 1 code
 *       bytes (4 when length >= 24), big-endian; the code is consumed from
 *       its least significant bit.
 *   then, from the next even address, the coded data as big-endian words,
 *   bit 15 first. Bit 1 walks to a node's first child, bit 0 to its second.
 *   Each decoded symbol is a delta added to a running byte.
 */
#include "st_dhuf.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  int32_t one;   /* child for bit 1 ($0(a3)); -1 none, -2 leaf marker */
  int32_t zero;  /* child for bit 0 ($4(a3)) */
  uint8_t sym;   /* $7(a3) on a leaf */
} DhufNode;

int st_dhuf_unpacked_length(const uint8_t *src, size_t srcLen) {
  if (srcLen < 8 || src[0] != 'p' || src[1] != 's' || src[2] != 'n') return -1;
  return (int)(((uint32_t)src[4] << 24) | ((uint32_t)src[5] << 16) | ((uint32_t)src[6] << 8) | src[7]);
}

int st_dhuf_depack(const uint8_t *src, size_t srcLen, size_t srcBaseOffset,
                   uint8_t *dst, size_t dstLen) {
  const int outLen = st_dhuf_unpacked_length(src, srcLen);
  if (outLen < 0 || (size_t)outLen > dstLen) return -1;

  const uint8_t *p = src + 3;
  const uint8_t *end = src + srcLen;
  const int symbols = (int)(*p++) + 1;   /* dbf d3 runs count-1 .. 0 */
  p += 4;                                 /* unpacked length, read above */

  /* At most 256 leaves, so at most 511 nodes in a prefix tree; the packer
   * allocates 5632 bytes of 8-byte nodes (704). */
  DhufNode *nodes = (DhufNode *)calloc(704, sizeof(DhufNode));
  if (!nodes) return -1;
  int count = 1;                          /* node 0 = root ($0(a2)) */
  nodes[0].one = nodes[0].zero = -1;

  uint32_t d4 = 0;                        /* moveq #0,d4 - kept across symbols */
  for (int s = 0; s < symbols; s++) {
    if (p + 2 > end) { free(nodes); return -1; }
    const uint8_t value = *p++;           /* move.b (a0)+,d5 */
    const int bits = *p++;                /* move.b (a0)+,d6 */
    const int extra = bits >> 3;          /* lsr.b #3,d7 */
    const int bytes = extra == 0 ? 1 : extra == 1 ? 2 : extra == 2 ? 3 : 4;
    if (p + bytes > end) { free(nodes); return -1; }
    for (int b = 0; b < bytes; b++) d4 = (d4 << 8) | *p++;

    int node = 0;                         /* move.l a2,a3 */
    for (int b = 0; b < bits; b++) {      /* subq.w #1,d6 / dbf d6 */
      const int bit = (int)(d4 & 1);      /* lsr.l #1,d4 -> carry */
      d4 >>= 1;
      int32_t *child = bit ? &nodes[node].one : &nodes[node].zero;
      if (*child < 0) {
        if (count >= 704) { free(nodes); return -1; }
        nodes[count].one = nodes[count].zero = -1;
        *child = count++;
      }
      node = *child;
    }
    nodes[node].one = -2;                 /* clr.l (a3): leaf */
    nodes[node].sym = value;              /* move.b d5,7(a3) */
  }

  /* Align to an even address of the bank (move.l a0,d3; addq #1; and #-2). */
  size_t off = (size_t)(p - src) + srcBaseOffset;
  if (off & 1) p++;

  uint8_t delta = 0;                      /* moveq #0,d0 */
  int node = 0;
  int written = 0;
  while (written < outLen) {
    if (p + 2 > end) { free(nodes); return -1; }
    uint16_t word = (uint16_t)((p[0] << 8) | p[1]);   /* move.w (a0)+,d2 */
    p += 2;
    for (int b = 0; b < 16 && written < outLen; b++) {
      const int bit = (word & 0x8000) != 0;           /* add.w d2,d2 -> carry */
      word = (uint16_t)(word << 1);
      node = bit ? nodes[node].one : nodes[node].zero;
      if (node < 0) { free(nodes); return -1; }
      if (nodes[node].one == -2) {                    /* tst.l (a3) == 0 */
        delta = (uint8_t)(delta + nodes[node].sym);   /* add.b 7(a3),d0 */
        dst[written++] = delta;                       /* move.b d0,(a1)+ */
        node = 0;                                     /* lea (a2),a3 */
      }
    }
  }
  free(nodes);
  return outLen;
}
