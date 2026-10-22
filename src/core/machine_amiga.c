/* Amiga machine probe: CPU and FPU from exec's AttnFlags, RAM from
 * AvailMem totals. Graphics, audio, network and disk probes arrive with
 * the UIs and outputs that need them (M1, M2); the tier stays text. */
#include "machine_internal.h"

#include <exec/execbase.h>
#include <exec/memory.h>
#include <proto/exec.h>

void sel_machine_probe_platform(sel_machine_raw* raw) {
    UWORD flags = SysBase->AttnFlags;

    if (flags & AFF_68060) {
        raw->cpu = SEL_CPU_68060;
    } else if (flags & AFF_68040) {
        raw->cpu = SEL_CPU_68040;
    } else if (flags & AFF_68030) {
        raw->cpu = SEL_CPU_68030;
    } else if (flags & AFF_68020) {
        raw->cpu = SEL_CPU_68020;
    } else if (flags & AFF_68010) {
        raw->cpu = SEL_CPU_68010;
    } else {
        raw->cpu = SEL_CPU_68000;
    }
    raw->fpu = (flags & (AFF_68881 | AFF_68882 | AFF_FPU40)) ? 1u : 0u;
    raw->chip_ram = (uintptr_t)AvailMem(MEMF_CHIP | MEMF_TOTAL);
    raw->fast_ram = (uintptr_t)AvailMem(MEMF_FAST | MEMF_TOTAL);
    raw->tier = SEL_TIER_TEXT;
}
