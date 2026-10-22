/* Desktop memory: both kinds come from malloc. The Amiga host (M2) maps
 * SEL_MEM_SAMPLE to MEMF_CHIP and SEL_MEM_FAST to MEMF_FAST. */
#include "host_internal.h"

#include <stdlib.h>

void* sel_host_mem_alloc(const sel_host* h, size_t size, uint32_t kind) {
    (void)h;
    (void)kind;
    return malloc(size);
}

void sel_host_mem_free(const sel_host* h, void* p) {
    (void)h;
    free(p);
}
