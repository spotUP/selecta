/* I195h: force-included into the stb_vorbis implementation and the adapter of stb_vorbis_arena_test. Every malloc
 * and alloca stb_vorbis makes is counted; the decoder must make none (RULES.md 4: no allocation after start, and
 * with an arena none at all). */
#ifndef SEL_STB_VORBIS_COUNT_H
#define SEL_STB_VORBIS_COUNT_H
#include <stdlib.h>
#if defined(__linux__)
#include <alloca.h>
#endif
extern unsigned long sel_test_mallocs, sel_test_allocas;
void* sel_test_malloc(size_t n);
void* sel_test_alloca(size_t n);
#undef alloca
#define malloc(n) sel_test_malloc(n)
#define alloca(n) sel_test_alloca(n)
#endif
