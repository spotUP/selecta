/* C face of the AdPlug wrapper (row I696). The C++ player API stays inside shim.cpp. */
#ifndef SELECTA_ADPLUG_SHIM_H
#define SELECTA_ADPLUG_SHIM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct adplug_shim adplug_shim;
/* Parses an OPL/AdLib song image; `path` supplies the extension AdPlug dispatches on and the directory companion files
 * (patch.003, standard.bnk, insts.dat) are read from. The data is copied. NULL when no AdPlug player accepts it. */
adplug_shim* adplug_shim_open(const uint8_t* data, size_t size, const char* path);
/* (Re)starts the song at rate Hz. 1 on success. */
int adplug_shim_start(adplug_shim* s, uint32_t rate, uint32_t subsong);
/* Info string 0 = title, 1 = author, 2 = description, 3 = format name; empty when absent. */
const char* adplug_shim_info(const adplug_shim* s, uint32_t index);
uint32_t adplug_shim_subsongs(const adplug_shim* s);
/* Interleaved stereo; returns frames written, fewer than asked only when the song has ended (first loop). */
uint32_t adplug_shim_render(adplug_shim* s, int16_t* out, uint32_t frames);
void adplug_shim_close(adplug_shim* s);
#ifdef __cplusplus
}
#endif
#endif
