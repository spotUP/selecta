/* Machine probe (design doc section 12). The machine is a tag list of
 * SEL_MACHINE_* values filled once at start; UIs, visualizers and the
 * registry read it and scale themselves. Build time decides only the ISA;
 * everything here is probed at run time. */
#ifndef SELECTA_MACHINE_H
#define SELECTA_MACHINE_H

#include <selecta/host.h>

SEL_EXTERN_C_BEGIN

/* SEL_MACHINE_CPU values. */
typedef enum sel_cpu {
    SEL_CPU_UNKNOWN = 0,
    SEL_CPU_68000,
    SEL_CPU_68010,
    SEL_CPU_68020,
    SEL_CPU_68030,
    SEL_CPU_68040,
    SEL_CPU_68060,
    SEL_CPU_DESKTOP
} sel_cpu;

/* SEL_MACHINE_TIER values: what the UI may draw. */
typedef enum sel_tier {
    SEL_TIER_TEXT = 0, /* character cells only */
    SEL_TIER_PLANAR,   /* ECS/AGA bitplanes */
    SEL_TIER_RTG,      /* chunky RTG screen */
    SEL_TIER_GPU       /* desktop GPU */
} sel_tier;

/* Probes the machine and returns the tag list, valid for the life of the
 * process (static storage; the next call refills it). Tags: SEL_MACHINE_CPU
 * (sel_cpu), CPU_SPEED and FPU_SPEED (loop iterations per millisecond, FPU_SPEED 0
 * without an FPU), FPU (0 or 1), FAST_RAM and CHIP_RAM (bytes, total),
 * GFX / AUDIO / NET / DISK (strings, "unknown" until their probes exist),
 * TIER (sel_tier). Uses host->clock_ms for the speed loop. No floating point. */
const sel_tag* sel_machine_probe(const sel_host* host);

/* Number of times sel_machine_probe has run in this process (test sentinel). */
uint32_t sel_machine_probe_calls(void);

/* Names for the enum values: "68020", "desktop", "text", "gpu", ... */
const char* sel_machine_cpu_name(uint32_t cpu);
const char* sel_machine_tier_name(uint32_t tier);

SEL_EXTERN_C_END

#endif /* SELECTA_MACHINE_H */
