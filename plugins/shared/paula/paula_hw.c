#include "paula_hw.h"

static uint32_t g_writes;

void sel_paula_hw_write(void* ctx, uint32_t reg, uint32_t value) {
    *(volatile uint16_t*)((volatile uint8_t*)ctx + reg) = (uint16_t)value;
    g_writes++;
}

/* AUDxLC is a 32-bit pointer split over LCH and LCL; two 16-bit writes through the single writer. */
static void hw_set_lc(void* ctx, uint32_t channel, const void* bytes, uint32_t offset, uint32_t serial) {
    uint32_t addr = (uint32_t)(uintptr_t)bytes + offset;
    uint32_t reg = SEL_PAULA_AUD_LCH + channel * SEL_PAULA_CHANNEL_STRIDE;
    (void)serial;
    sel_paula_hw_write(ctx, reg, addr >> 16);
    sel_paula_hw_write(ctx, reg + 2u, addr & 0xFFFFu);
}

void sel_paula_hw_bus(sel_paula_bus* bus, volatile uint8_t* base) {
    bus->write = sel_paula_hw_write;
    bus->set_lc = hw_set_lc;
    bus->ctx = (void*)base;
}

uint32_t sel_paula_hw_writes(void) {
    return g_writes;
}
