/* C face of the bsnes core for the SNSF replayer (I705a). One cartridge at a time (bsnes keeps global state). */
#ifndef SNSF_BSNES_BRIDGE_H
#define SNSF_BSNES_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Boots the ROM headless. rom: whole image (sparse holes zero). sram may be NULL. rate: output rate in Hz.
   Returns 0 on success, negative on failure. */
int snsf_bridge_open(const uint8_t *rom, size_t rom_size, const uint8_t *sram, size_t sram_size, double rate);
/* Fills out with stereo int16 frames; always returns frames. */
size_t snsf_bridge_render(int16_t *out, size_t frames);
void snsf_bridge_close(void);
#ifdef __cplusplus
}
#endif
#endif
