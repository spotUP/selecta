/* C face of the libvgm wrapper (row I431). The C++ player API stays inside shim.cpp. */
#ifndef SELECTA_LIBVGM_SHIM_H
#define SELECTA_LIBVGM_SHIM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct vgm_shim vgm_shim;
/* Parses a VGM, VGZ, S98, DRO or GYM image (gzip is detected by content). The data is copied. NULL when no libvgm
 * engine accepts it. */
vgm_shim* vgm_shim_open(const uint8_t* data, size_t size);
/* (Re)starts the song at rate Hz. 1 on success. */
int vgm_shim_start(vgm_shim* s, uint32_t rate);
/* Info string 0 = title, 1 = author, 2 = game (release), 3 = system; empty when absent. */
const char* vgm_shim_info(const vgm_shim* s, uint32_t index);
/* Engine name such as "VGM" or "S98". */
const char* vgm_shim_format(const vgm_shim* s);
/* Whole-song length in frames at the rate of the last start (the song plus one loop pass, no fade). */
uint32_t vgm_shim_total_frames(const vgm_shim* s);
/* Interleaved stereo; returns frames written, fewer than asked only when the song has ended. */
uint32_t vgm_shim_render(vgm_shim* s, int16_t* out, uint32_t frames);
void vgm_shim_close(vgm_shim* s);
#ifdef __cplusplus
}
#endif
#endif
