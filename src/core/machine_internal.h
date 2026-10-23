#ifndef SELECTA_MACHINE_INTERNAL_H
#define SELECTA_MACHINE_INTERNAL_H

#include <selecta/machine.h>

/* What the platform file reports; machine.c turns it into the tag list. */
typedef struct sel_machine_raw {
    sel_cpu cpu;
    uint32_t fpu;
    uintptr_t fast_ram;
    uintptr_t chip_ram;
    sel_tier tier;
    const char* gfx;   /* "OCS" "ECS" "AGA" ..., NULL = not probed */
    const char* audio; /* "paula", "paula,ahi", ..., NULL = not probed */
} sel_machine_raw;

/* Implemented once per platform in src/core/<os>/machine.c. */
void sel_machine_probe_platform(sel_machine_raw* raw);

#endif
