/* I118: SEL_MEM_SAMPLE reaches the chip RAM path. The Amiga host (core/amiga/mem.c) allocates through
 * sel_mem_alloc_classed; here a fake backend records the flags it is handed and the chip counter is the sentinel. */
#include "../../src/host/mem_class.h"

#include <stdio.h>
#include <stdlib.h>

static uint32_t last_flags;
static void* fake_backend(void* ctx, size_t size, uint32_t flags) {
    (void)ctx;
    last_flags = flags;
    return malloc(size);
}

int main(void) {
    int failures = 0;
    void* p;
    sel_mem_counters_reset();
    p = sel_mem_alloc_classed(fake_backend, NULL, 16, SEL_MEM_SAMPLE);
    if (last_flags != SEL_MEMF_CHIP || sel_mem_chip_allocs() != 1) {
        printf("FAIL sample: flags=%u chip=%u\n", (unsigned)last_flags, (unsigned)sel_mem_chip_allocs());
        failures++;
    }
    free(p);
    p = sel_mem_alloc_classed(fake_backend, NULL, 16, SEL_MEM_FAST);
    if (last_flags != SEL_MEMF_PUBLIC || sel_mem_chip_allocs() != 1) {
        printf("FAIL fast: flags=%u chip=%u\n", (unsigned)last_flags, (unsigned)sel_mem_chip_allocs());
        failures++;
    }
    free(p);
    printf("mem_class chip_allocs=%u failures=%d\n", (unsigned)sel_mem_chip_allocs(), failures);
    return failures != 0;
}
