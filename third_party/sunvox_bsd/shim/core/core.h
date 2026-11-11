/* Selecta shim for the SunDog "core/core.h" the SunVox engine includes: fixed-width integer names, no OS switches. */
#ifndef __CORE__
#define __CORE__
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#define NONPALM
typedef unsigned char uchar;
typedef uint16_t uint16;
typedef int16_t int16;
typedef intptr_t sv_ret_t; /* synth handler result: also carries name pointers */
#if defined(__AMIGA__) || defined(AMIGA) || defined(__amigaos__)
/* AmigaOS system headers already define ulong as unsigned long, which is 32 bits on m68k */
#else
typedef uint32_t ulong; /* 32 bits on every target: the song format stores 32-bit fields read straight into these */
#endif
typedef int32_t slong;
typedef char UTF8_CHAR;
typedef unsigned short UTF16_CHAR;
typedef uint32_t UTF32_CHAR;

/* I427e7: byte-order readers for song-file data. The .sunvox format is little-endian (block sizes, integers,
   pattern notes, sample data) and block ids are four ASCII bytes compared against 'SVOX'-style multi-char
   constants, i.e. read big-endian. These assemble values from bytes, so they are correct on every host (a no-op
   result on little-endian) with no byte-order #ifdef. The *_to_native helpers convert an array in place. */
static inline uint16_t sv_rd_le16( const void *p ) { const uchar *b = (const uchar*)p; return (uint16_t)( b[ 0 ] | ( b[ 1 ] << 8 ) ); }
static inline uint32_t sv_rd_le32( const void *p ) { const uchar *b = (const uchar*)p; return (uint32_t)b[ 0 ] | ( (uint32_t)b[ 1 ] << 8 ) | ( (uint32_t)b[ 2 ] << 16 ) | ( (uint32_t)b[ 3 ] << 24 ); }
static inline uint32_t sv_rd_be32( const void *p ) { const uchar *b = (const uchar*)p; return ( (uint32_t)b[ 0 ] << 24 ) | ( (uint32_t)b[ 1 ] << 16 ) | ( (uint32_t)b[ 2 ] << 8 ) | (uint32_t)b[ 3 ]; }
static inline void sv_le16_to_native( void *p, size_t count ) { uint16_t *v = (uint16_t*)p; for( size_t i = 0; i < count; i++ ) v[ i ] = sv_rd_le16( &v[ i ] ); }
static inline void sv_le32_to_native( void *p, size_t count ) { uint32_t *v = (uint32_t*)p; for( size_t i = 0; i < count; i++ ) v[ i ] = sv_rd_le32( &v[ i ] ); }
#endif
