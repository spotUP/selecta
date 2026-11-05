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
#endif
