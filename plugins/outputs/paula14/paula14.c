/* paula14 output (plan M2 phase 4, I123): 14-bit PCM over four Paula channels. Same skeleton as the shadow;
 * the bus is the custom chip (../paula/paula_hw.c). Amiga only. */
#include "../paula/paula_hw.h"
#include "common/p14_out.h"

static void* hw_open(const sel_host* host, sel_paula_bus* bus) {
    (void)host;
    sel_paula_hw_bus(bus, (volatile uint8_t*)SEL_PAULA_CUSTOM_BASE);
    return bus->ctx;
}

static void hw_close(const sel_host* host, void* ctx) {
    (void)host;
    (void)ctx;
}

static const sel_p14_out_kind g_kind = {"paula14", "Paula 14 Bit", 1, hw_open, hw_close};
static sel_tag g_hooks[12];
static sel_tag g_desc[5];

static void* paula14_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    return sel_p14_out_open(&g_kind, host, rate, channels);
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    return sel_p14_out_describe(&g_kind, paula14_open, g_hooks, g_desc);
}
