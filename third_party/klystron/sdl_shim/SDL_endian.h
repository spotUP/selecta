/* SDL_endian stand-in: klystron song files are little endian; swap on a big endian host (Amiga 68k). */
#ifndef SEL_KLYS_SDL_ENDIAN_SHIM_H
#define SEL_KLYS_SDL_ENDIAN_SHIM_H
#include <stdint.h>
#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SDL_BYTEORDER SDL_BIG_ENDIAN
#define SDL_SwapLE16(x) ((uint16_t)((((uint16_t)(x)) >> 8) | (((uint16_t)(x)) << 8)))
#define SDL_SwapLE32(x) ((uint32_t)((((uint32_t)(x)) >> 24) | ((((uint32_t)(x)) >> 8) & 0xFF00u) | ((((uint32_t)(x)) << 8) & 0xFF0000u) | (((uint32_t)(x)) << 24)))
#else
#define SDL_BYTEORDER SDL_LIL_ENDIAN
#define SDL_SwapLE16(x) (x)
#define SDL_SwapLE32(x) (x)
#endif
#endif
