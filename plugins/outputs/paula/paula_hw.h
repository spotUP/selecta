/* The register bus of the real chip: 16-bit writes at base + register offset. `base` is $DFF000 on the
 * Amiga; tests pass a RAM array. Integer only (audio task). */
#ifndef SELECTA_PAULA_HW_H
#define SELECTA_PAULA_HW_H

#include "paula_voices.h"

#define SEL_PAULA_CUSTOM_BASE 0xDFF000u

/* Fills `bus` and sets bus->ctx to `base` (never NULL for a valid base). */
void sel_paula_hw_bus(sel_paula_bus* bus, volatile uint8_t* base);
/* The one function that writes a Paula register; counts writes (sentinel for the reachability test). */
void sel_paula_hw_write(void* ctx, uint32_t reg, uint32_t value);
uint32_t sel_paula_hw_writes(void);

#endif
