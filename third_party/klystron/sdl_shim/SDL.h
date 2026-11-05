/* Minimal SDL stand-in for the klystron sound engine (Selecta). Only the types and calls klystron's snd/ code touches;
 * the engine runs single threaded with NOSDL_MIXER and without SDL_RWops, so none of the audio or mutex calls ever run. */
#ifndef SEL_KLYS_SDL_SHIM_H
#define SEL_KLYS_SDL_SHIM_H
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
/* strict-C libms (Amiga) omit M_PI; the engine uses it in cyd.c, cydchr.c, cydrvb.c and music.c */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef int64_t Sint64;
typedef void SDL_mutex;
static inline SDL_mutex* SDL_CreateMutex(void) { return NULL; }
static inline void SDL_DestroyMutex(SDL_mutex* m) { (void)m; }
static inline int SDL_LockMutex(SDL_mutex* m) { (void)m; return 0; }
static inline int SDL_UnlockMutex(SDL_mutex* m) { (void)m; return 0; }
static inline void SDL_Delay(Uint32 ms) { (void)ms; }
static inline Uint32 SDL_GetTicks(void) { return 0; }
static inline const char* SDL_GetError(void) { return ""; }
#define SDL_MUSTLOCK(s) 0
static inline int SDL_LockSurface(void* s) { (void)s; return 0; }
static inline void SDL_UnlockSurface(void* s) { (void)s; }
#define AUDIO_S16SYS 0x8010
typedef struct {
    int freq;
    Uint16 format;
    Uint8 channels;
    Uint16 samples;
    Uint32 size;
    void (*callback)(void* userdata, Uint8* stream, int len);
    void* userdata;
} SDL_AudioSpec;
static inline int SDL_OpenAudio(SDL_AudioSpec* d, SDL_AudioSpec* o) { (void)d; (void)o; return -1; }
static inline void SDL_CloseAudio(void) {}
static inline void SDL_PauseAudio(int p) { (void)p; }
/* Upstream macros.h reads song fields through SDL_RWread(ctx, ...); the engine's own RWops (USESDL_RWOPS undefined) carries read(). */
#define SDL_RWread(ctx, ptr, size, n) ((ctx)->read((ctx), (ptr), (size), (n)))
#endif
