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
/* Cross-thread publication (I1082b): a store/increment that releases, a load that acquires. Ring indices and
 * the cancel generation use these; arm64 reorders plain volatile accesses. The Amiga has one task: plain. */
#define SEL_ATOMIC_LOAD_ACQ(p) (*(p))
#define SEL_ATOMIC_STORE_REL(p, v) (*(p) = (v))
#define SEL_ATOMIC_INC_REL(p) (*(p) += 1u)
#define SEL_ATOMIC_XCHG(p, v) sel_atomic_xchg_plain((p), (v))
static inline uint32_t sel_atomic_xchg_plain(uint32_t* p, uint32_t v) {
    uint32_t o = *p;
    *p = v;
    return o;
}
#else
#define SEL_COUNT_ADD(p, n) __atomic_fetch_add((p), (n), __ATOMIC_RELAXED)
#define SEL_COUNT_GET(p) __atomic_load_n((p), __ATOMIC_RELAXED)
#define SEL_ATOMIC_LOAD_ACQ(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define SEL_ATOMIC_STORE_REL(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)
#define SEL_ATOMIC_INC_REL(p) ((void)__atomic_fetch_add((p), 1u, __ATOMIC_RELEASE))
#define SEL_ATOMIC_XCHG(p, v) __atomic_exchange_n((p), (v), __ATOMIC_ACQ_REL)
#endif

#endif
