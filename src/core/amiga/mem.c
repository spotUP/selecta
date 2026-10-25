/* Amiga memory: SEL_MEM_SAMPLE comes from chip RAM (Paula DMA cannot read fast RAM), everything else from
 * any public RAM. The class-to-flags mapping is host/mem_class.c, shared with the desktop tests. */
#include "host_internal.h"
#include "mem_class.h"

#include <exec/memory.h>
#include <proto/exec.h>
#include <stdio.h>

static void* alloc_vec(void* ctx, size_t size, uint32_t flags) {
    (void)ctx;
    return AllocVec((ULONG)size, (flags & SEL_MEMF_CHIP) ? MEMF_CHIP : MEMF_PUBLIC);
}

void* sel_host_mem_alloc(const sel_host* h, size_t size, uint32_t kind) {
    (void)h;
    void* p = sel_mem_alloc_classed(alloc_vec, NULL, size, kind);
    if (p == NULL) {
        /* The only trace of a failed allocation: the callers just return NULL (stderr goes to --log). */
        fprintf(stderr, "selecta: alloc of %lu bytes (kind %lu) failed, largest public block %lu, free %lu\n",
                (unsigned long)size, (unsigned long)kind, (unsigned long)AvailMem(MEMF_PUBLIC | MEMF_LARGEST),
                (unsigned long)AvailMem(MEMF_PUBLIC));
    }
    return p;
}

void sel_host_mem_free(const sel_host* h, void* p) {
    (void)h;
    if (p != NULL) {
        FreeVec(p);
    }
}
