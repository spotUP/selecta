/* Event counters that a second thread may bump while another reads them (sentinels and statistics).
 * The 68k gcc has no libatomic, so __atomic_fetch_add does not link there. Builds with no second
 * thread (the Amiga: SEL_LIBRARY_NO_THREAD on the library side, no SEL_LI_THREAD in the engine, both
 * the __amigaos__ / AMIGA build) count with plain loads and stores; every other build is atomic.
 * Counters are uint32_t. */
#ifndef SELECTA_CORE_COUNT_H
#define SELECTA_CORE_COUNT_H

#if defined(SEL_LIBRARY_NO_THREAD) || defined(__amigaos__) || defined(AMIGA)
#define SEL_COUNT_ADD(p, n) (*(p) += (n))
#define SEL_COUNT_GET(p) (*(p))
#else
#define SEL_COUNT_ADD(p, n) __atomic_fetch_add((p), (n), __ATOMIC_RELAXED)
#define SEL_COUNT_GET(p) __atomic_load_n((p), __ATOMIC_RELAXED)
#endif

#endif
