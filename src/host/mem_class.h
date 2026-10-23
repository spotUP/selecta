/* Memory class mapping (plan M2 phase 3, I118): which exec allocation flags a SEL_MEM_* kind needs.
 * Pure integer code compiled on every platform, so the desktop tests prove the mapping the Amiga host
 * (core/amiga/mem.c) hands to AllocVec. The flag values are exec/memory.h's. */
#ifndef SELECTA_MEM_CLASS_H
#define SELECTA_MEM_CLASS_H

#include <selecta/host.h>

#include <stddef.h>
#include <stdint.h>

enum { SEL_MEMF_PUBLIC = 1u, SEL_MEMF_CHIP = 2u, SEL_MEMF_FAST = 4u };

/* SEL_MEM_SAMPLE (Paula DMA reads it) -> chip RAM; SEL_MEM_FAST -> any public RAM. */
uint32_t sel_mem_flags(uint32_t kind);

/* The allocator a platform supplies (AllocVec on the Amiga). */
typedef void* (*sel_mem_backend_fn)(void* ctx, size_t size, uint32_t flags);

/* Allocates through `backend` with the flags of `kind` and counts per class. */
void* sel_mem_alloc_classed(sel_mem_backend_fn backend, void* ctx, size_t size, uint32_t kind);

/* Allocations that asked for chip RAM since start (reachability sentinel). */
uint32_t sel_mem_chip_allocs(void);
void sel_mem_counters_reset(void);

#endif
