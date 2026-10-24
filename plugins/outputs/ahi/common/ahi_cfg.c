#include "ahi_cfg.h"

#include <selecta/tags.h>

static const struct {
    uint32_t cpu, buffer;
} g_buffer_table[] = {
    {SEL_CPU_68000, 8192u}, {SEL_CPU_68010, 8192u}, {SEL_CPU_68020, 4096u}, {SEL_CPU_68030, 2048u},
    {SEL_CPU_68040, 1024u}, {SEL_CPU_68060, 1024u}, {SEL_CPU_DESKTOP, 1024u},
};

uint32_t sel_ahi_buffer_frames(uint32_t cpu) {
    uint32_t i;
    for (i = 0; i < sizeof g_buffer_table / sizeof g_buffer_table[0]; i++) {
        if (g_buffer_table[i].cpu == cpu) {
            return g_buffer_table[i].buffer;
        }
    }
    return 4096u; /* unknown CPU: the 68020 baseline */
}

uint32_t sel_ahi_default_rate(uint32_t cpu) {
    return cpu == SEL_CPU_68040 || cpu == SEL_CPU_68060 || cpu == SEL_CPU_DESKTOP ? 44100u : 28150u;
}

uint32_t sel_ahi_pick_buffer(uint32_t cpu, const char* override) {
    uint32_t n = 0;
    while (override != NULL && *override >= '0' && *override <= '9' && n < 100000u) {
        n = n * 10u + (uint32_t)(*override++ - '0');
    }
    return n >= SEL_AHI_BUF_MIN && n <= SEL_AHI_BUF_MAX ? n : sel_ahi_buffer_frames(cpu);
}

uint32_t sel_ahi_host_cpu(const sel_host* host) {
    return (uint32_t)sel_tag_find(sel_machine_probe(host), SEL_MACHINE_CPU, SEL_CPU_UNKNOWN);
}
