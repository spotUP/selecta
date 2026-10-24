/* paula output (plan M2 phase 3, I118): the Paula voice sink over the real custom chip. Shares every line of
 * output and sink code with paula-shadow (paula_out.c, paula_voices.c); only the bus differs. The bus is
 * paula_hw.c, integer only, because the sink writes from the audio task. */
#include "paula/paula_hw.h"
#include "paula_out.h"

static void* hw_open(const sel_host* host, sel_paula_bus* bus) {
    (void)host;
    sel_paula_hw_bus(bus, (volatile uint8_t*)SEL_PAULA_CUSTOM_BASE);
    return bus->ctx;
}

static void hw_close(const sel_host* host, void* ctx) {
    (void)host;
    (void)ctx;
}

static const sel_paula_out_kind g_kind = {"paula", "Paula", 1, hw_open, hw_close, NULL};
static sel_tag g_hooks[16];
static sel_tag g_desc[5];

static void* paula_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    return sel_paula_out_open(&g_kind, host, rate, channels);
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    return sel_paula_out_describe(&g_kind, paula_open, g_hooks, g_desc);
}
