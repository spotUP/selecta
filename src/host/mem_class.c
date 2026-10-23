#include "mem_class.h"

static uint32_t g_chip_allocs;

uint32_t sel_mem_flags(uint32_t kind) {
    return kind == SEL_MEM_SAMPLE ? SEL_MEMF_CHIP : SEL_MEMF_PUBLIC;
}

void* sel_mem_alloc_classed(sel_mem_backend_fn backend, void* ctx, size_t size, uint32_t kind) {
    uint32_t flags = sel_mem_flags(kind);
    if (flags & SEL_MEMF_CHIP) {
        g_chip_allocs++;
    }
    return backend(ctx, size, flags);
}

uint32_t sel_mem_chip_allocs(void) {
    return g_chip_allocs;
}

void sel_mem_counters_reset(void) {
    g_chip_allocs = 0;
}
