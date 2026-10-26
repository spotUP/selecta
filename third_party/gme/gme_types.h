/* Selecta build configuration (replaces the file CMake generates from gme_types.h.in): every console emulator of
 * libgme 0.6.5 except VGM/VGZ. VGM is left to the libvgm replayer (row I431); GYM uses the GENS YM2612. */
#ifndef GME_TYPES_H
#define GME_TYPES_H
#define USE_GME_AY
#define USE_GME_GBS
#define USE_GME_GYM
#define USE_GME_HES
#define USE_GME_KSS
#define USE_GME_NSF
#define USE_GME_NSFE
#define USE_GME_SAP
#define USE_GME_SPC
#endif /* GME_TYPES_H */
