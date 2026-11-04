/* C face of the Furnace wrapper (row I430). The C++ DivEngine API stays inside shim.cpp. */
#ifndef SELECTA_FURNACE_SHIM_H
#define SELECTA_FURNACE_SHIM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct furnace_shim furnace_shim;
/* Parses a Furnace-readable song image (.fur, .dmf, .ftm, .tfm and the other formats DivEngine::load detects by
 * magic). `name` is only the extension hint. The data is copied. NULL when the engine rejects it. */
furnace_shim* furnace_shim_open(const uint8_t* data, size_t size, const char* name);
/* (Re)starts the song at rate Hz, stopping after the first loop. 1 on success. */
int furnace_shim_start(furnace_shim* s, uint32_t rate, uint32_t subsong);
/* Info string 0 = title, 1 = author, 2 = system name; empty when absent. */
const char* furnace_shim_info(const furnace_shim* s, uint32_t index);
uint32_t furnace_shim_subsongs(const furnace_shim* s);
/* Interleaved stereo; returns frames written, fewer than asked only when the song has ended (first loop). */
uint32_t furnace_shim_render(furnace_shim* s, int16_t* out, uint32_t frames);
void furnace_shim_close(furnace_shim* s);
#ifdef __cplusplus
}
#endif
#endif
