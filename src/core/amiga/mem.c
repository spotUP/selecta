/* Amiga memory: SEL_MEM_SAMPLE comes from chip RAM (Paula DMA cannot read fast RAM), everything else from
 * any public RAM. The class-to-flags mapping is host/mem_class.c, shared with the desktop tests. */
#include "host_internal.h"
#include "mem_class.h"

#include <exec/memory.h>
#include <proto/exec.h>

static void* alloc_vec(void* ctx, size_t size, uint32_t flags) {
    (void)ctx;
    return AllocVec((ULONG)size, (flags & SEL_MEMF_CHIP) ? MEMF_CHIP : MEMF_PUBLIC);
}

void* sel_host_mem_alloc(const sel_host* h, size_t size, uint32_t kind) {
    (void)h;
    return sel_mem_alloc_classed(alloc_vec, NULL, size, kind);
}

void sel_host_mem_free(const sel_host* h, void* p) {
    (void)h;
    if (p != NULL) {
        FreeVec(p);
    }
}
