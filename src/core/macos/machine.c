/* macOS machine probe: physical memory from sysctl, everything else is
 * fixed (an FPU is always present, the GPU tier is assumed until a UI
 * probes the display). */
#include "machine_internal.h"

#include <sys/sysctl.h>
#include <sys/types.h>

static uintptr_t physical_memory(void) {
    uint64_t bytes = 0;
    size_t len = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &len, NULL, 0) != 0) {
        return 0;
    }
    return (uintptr_t)bytes;
}

void sel_machine_probe_platform(sel_machine_raw* raw) {
    raw->cpu = SEL_CPU_DESKTOP;
    raw->fpu = 1;
    raw->fast_ram = physical_memory();
    raw->chip_ram = 0;
    raw->tier = SEL_TIER_GPU;
    raw->audio = "coreaudio";
}
