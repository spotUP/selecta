/* paula14-shadow (plan M2 phase 4, I122/I123): the paula14 output over a counting bus instead of the custom
 * chip. T52 compares its split-byte hash between the desktop and the 68k build. */
#include "common/p14_out.h"

static uint32_t g_writes;

static void bus_write(void* ctx, uint32_t reg, uint32_t value) {
    (void)ctx;
    (void)reg;
    (void)value;
    g_writes++;
}

static void bus_set_lc(void* ctx, uint32_t channel, const void* bytes, uint32_t offset, uint32_t serial) {
    (void)ctx;
    (void)channel;
    (void)bytes;
    (void)offset;
    (void)serial;
    g_writes++;
}

static void* shadow_bus_open(const sel_host* host, sel_paula_bus* bus) {
    (void)host;
    bus->write = bus_write;
    bus->set_lc = bus_set_lc;
    bus->ctx = &g_writes;
    return &g_writes;
}

static void shadow_bus_close(const sel_host* host, void* ctx) {
    (void)host;
    (void)ctx;
}

static const sel_p14_out_kind g_kind = {"paula14-shadow", "Paula 14 Bit shadow", 0, shadow_bus_open, shadow_bus_close};
static sel_tag g_hooks[12];
static sel_tag g_desc[5];

static void* shadow_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    return sel_p14_out_open(&g_kind, host, rate, channels);
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    return sel_p14_out_describe(&g_kind, shadow_open, g_hooks, g_desc);
}
