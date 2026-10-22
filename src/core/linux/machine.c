/* Linux machine probe: physical memory from /proc/meminfo, everything else
 * is fixed (an FPU is always present, the GPU tier is assumed until a UI
 * probes the display). */
#include "machine_internal.h"

#include <stdio.h>

static uintptr_t physical_memory(void) {
    char line[128];
    unsigned long kb = 0;
    FILE* f = fopen("/proc/meminfo", "r");
    if (f == NULL) {
        return 0;
    }
    while (fgets(line, (int)sizeof(line), f) != NULL) {
        if (sscanf(line, "MemTotal: %lu kB", &kb) == 1) {
            break;
        }
    }
    fclose(f);
    return (uintptr_t)kb * 1024u;
}

void sel_machine_probe_platform(sel_machine_raw* raw) {
    raw->cpu = SEL_CPU_DESKTOP;
    raw->fpu = 1;
    raw->fast_ram = physical_memory();
    raw->chip_ram = 0;
    raw->tier = SEL_TIER_GPU;
}
