/* Machine probe, platform independent half: the speed loop, the counter
 * and the tag list. Platform files fill sel_machine_raw. No float. */
#include "machine_internal.h"

#include <string.h>

#define SEL_MACHINE_TAGS 12u /* eleven rows plus SEL_TAG_END */
#define SEL_SPEED_CHUNK 20000u
#define SEL_SPEED_MIN_MS 100u

static sel_tag g_machine[SEL_MACHINE_TAGS];
static uint32_t g_probe_calls;
static int g_tier_override = -1;

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
    sel_machine_raw raw = {SEL_CPU_UNKNOWN, 0, 0, 0, SEL_TIER_TEXT, NULL, NULL};
    uint32_t speed;
    uint32_t n = 0;

    g_probe_calls++;
    sel_machine_probe_platform(&raw);
    if (g_tier_override >= 0) {
        raw.tier = (sel_tier)g_tier_override;
    }
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
    PUT(SEL_MACHINE_GFX, raw.gfx ? raw.gfx : "unknown");
    PUT(SEL_MACHINE_AUDIO, raw.audio ? raw.audio : "unknown");
    PUT(SEL_MACHINE_NET, "unknown");
    PUT(SEL_MACHINE_DISK, "unknown");
    PUT(SEL_MACHINE_TIER, raw.tier);
#undef PUT
    g_machine[n].tag = SEL_TAG_END;
    g_machine[n].value = 0;
    return g_machine;
}

static const struct {
    const char* name;
    sel_cpu cpu;
    uint32_t fpu;
    uintptr_t chip;
    uintptr_t fast;
} g_fakes[] = {
    {"a500", SEL_CPU_68000, 0, 512u * 1024u, 0},
    {"a1200", SEL_CPU_68020, 0, 2048u * 1024u, 0},
    {"a1200-fpu", SEL_CPU_68030, 1, 2048u * 1024u, 4096u * 1024u},
    {"a060", SEL_CPU_68060, 1, 2048u * 1024u, 65536u * 1024u},
};

const sel_tag* sel_machine_fake(const char* name) {
    static sel_tag fake[8];
    uint32_t i;
    for (i = 0; i < sizeof g_fakes / sizeof g_fakes[0]; i++) {
        if (strcmp(g_fakes[i].name, name) != 0) {
            continue;
        }
        fake[0].tag = SEL_MACHINE_CPU;
        fake[0].value = g_fakes[i].cpu;
        fake[1].tag = SEL_MACHINE_FPU;
        fake[1].value = g_fakes[i].fpu;
        fake[2].tag = SEL_MACHINE_CHIP_RAM;
        fake[2].value = g_fakes[i].chip;
        fake[3].tag = SEL_MACHINE_FAST_RAM;
        fake[3].value = g_fakes[i].fast;
        fake[4].tag = SEL_MACHINE_TIER;
        fake[4].value = SEL_TIER_TEXT;
        fake[5].tag = SEL_TAG_END;
        fake[5].value = 0;
        return fake;
    }
    return NULL;
}

void sel_machine_set_tier_override(int tier) {
    g_tier_override = tier;
}

int sel_machine_tier_by_name(const char* name) {
    int t;
    for (t = SEL_TIER_TEXT; t <= SEL_TIER_GPU; t++) {
        if (strcmp(sel_machine_tier_name((uint32_t)t), name) == 0) {
            return t;
        }
    }
    return -1;
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
