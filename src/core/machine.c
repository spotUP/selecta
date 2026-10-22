/* Machine probe, platform independent half: the speed loop, the counter
 * and the tag list. Platform files fill sel_machine_raw. No float. */
#include "machine_internal.h"

#define SEL_MACHINE_TAGS 12u /* eleven rows plus SEL_TAG_END */
#define SEL_SPEED_CHUNK 20000u
#define SEL_SPEED_MIN_MS 100u

static sel_tag g_machine[SEL_MACHINE_TAGS];
static uint32_t g_probe_calls;

/* Fixed integer loop timed by the host clock: iterations per millisecond.
 * The volatile accumulator keeps the compiler from folding the loop. */
static uint32_t measure_cpu_speed(const sel_host* host) {
    volatile uint32_t acc = 1;
    uint32_t start = host->clock_ms(host);
    uint32_t elapsed;
    uint32_t chunks = 0;
    uint32_t i;

    do {
        for (i = 0; i < SEL_SPEED_CHUNK; i++) {
            acc = acc * 1664525u + 1013904223u;
        }
        chunks++;
        elapsed = host->clock_ms(host) - start;
    } while (elapsed < SEL_SPEED_MIN_MS);
    return (chunks * SEL_SPEED_CHUNK) / elapsed;
}

const sel_tag* sel_machine_probe(const sel_host* host) {
    sel_machine_raw raw = {SEL_CPU_UNKNOWN, 0, 0, 0, SEL_TIER_TEXT};
    uint32_t speed;
    uint32_t n = 0;

    g_probe_calls++;
    sel_machine_probe_platform(&raw);
    speed = measure_cpu_speed(host);

#define PUT(id, val)                       \
    g_machine[n].tag = (id);               \
    g_machine[n].value = (uintptr_t)(val); \
    n++
    PUT(SEL_MACHINE_CPU, raw.cpu);
    PUT(SEL_MACHINE_CPU_SPEED, speed);
    PUT(SEL_MACHINE_FPU, raw.fpu);
    /* Measuring float throughput needs float code, which src/core never
     * contains (RULES.md rule 4). Until a float consumer measures it, a
     * present FPU reports the integer loop speed as its proxy. */
    PUT(SEL_MACHINE_FPU_SPEED, raw.fpu ? speed : 0u);
    PUT(SEL_MACHINE_FAST_RAM, raw.fast_ram);
    PUT(SEL_MACHINE_CHIP_RAM, raw.chip_ram);
    PUT(SEL_MACHINE_GFX, "unknown");
    PUT(SEL_MACHINE_AUDIO, "unknown");
    PUT(SEL_MACHINE_NET, "unknown");
    PUT(SEL_MACHINE_DISK, "unknown");
    PUT(SEL_MACHINE_TIER, raw.tier);
#undef PUT
    g_machine[n].tag = SEL_TAG_END;
    g_machine[n].value = 0;
    return g_machine;
}

uint32_t sel_machine_probe_calls(void) {
    return g_probe_calls;
}

const char* sel_machine_cpu_name(uint32_t cpu) {
    switch (cpu) {
    case SEL_CPU_68000:
        return "68000";
    case SEL_CPU_68010:
        return "68010";
    case SEL_CPU_68020:
        return "68020";
    case SEL_CPU_68030:
        return "68030";
    case SEL_CPU_68040:
        return "68040";
    case SEL_CPU_68060:
        return "68060";
    case SEL_CPU_DESKTOP:
        return "desktop";
    default:
        return "unknown";
    }
}

const char* sel_machine_tier_name(uint32_t tier) {
    switch (tier) {
    case SEL_TIER_TEXT:
        return "text";
    case SEL_TIER_PLANAR:
        return "planar";
    case SEL_TIER_RTG:
        return "rtg";
    case SEL_TIER_GPU:
        return "gpu";
    default:
        return "unknown";
    }
}
