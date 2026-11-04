/* SPDX-License-Identifier: GPL-3.0-or-later */
/* C face of the ZXTune wrapper (row I421). The C++ API stays inside shim.cpp. */
#ifndef SELECTA_ZXTUNE_SHIM_H
#define SELECTA_ZXTUNE_SHIM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct zx_shim zx_shim;

/* Runs ZXTune's detection over a copy of the image. NULL when no tracker module is found. */
zx_shim* zx_shim_open(const uint8_t* data, size_t size);
/* Number of modules (subsongs) found. */
uint32_t zx_shim_tracks(const zx_shim* s);
/* (Re)creates the renderer for one module at rate Hz. 1 on success. */
int zx_shim_start(zx_shim* s, uint32_t track, uint32_t rate);
/* Info string: 0 title, 1 author, 2 program, 3 type (format name), 4 comment; empty when absent. */
const char* zx_shim_info(const zx_shim* s, uint32_t index);
/* Module duration in ms as ZXTune counts it (0 when unknown). */
uint32_t zx_shim_duration_ms(const zx_shim* s);
/* Interleaved stereo int16; returns frames written, fewer than asked only at the end of the song. */
uint32_t zx_shim_render(zx_shim* s, int16_t* out, uint32_t frames);
int zx_shim_seek(zx_shim* s, uint32_t ms);
void zx_shim_close(zx_shim* s);

#ifdef __cplusplus
}
#endif
#endif
