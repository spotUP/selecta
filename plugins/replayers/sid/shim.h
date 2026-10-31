/* C face of the libsidplayfp wrapper (row I434). The C++ API stays inside shim.cpp. */
#ifndef SELECTA_SID_SHIM_H
#define SELECTA_SID_SHIM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sid_shim sid_shim;

/* Parses a PSID/RSID image (copied). NULL when libsidplayfp rejects it. */
sid_shim* sid_shim_open(const uint8_t* data, size_t size);
/* (Re)starts emulation at rate Hz; song is 1-based, 0 means the file default. 1 on success. */
int sid_shim_start(sid_shim* s, uint32_t song, uint32_t rate);
uint32_t sid_shim_songs(const sid_shim* s);
uint32_t sid_shim_start_song(const sid_shim* s);
/* Info string 0 = title, 1 = author, 2 = release; empty when absent. */
const char* sid_shim_info(const sid_shim* s, uint32_t index);
const char* sid_shim_format(const sid_shim* s);
/* Interleaved stereo; returns frames written, fewer than asked only when emulation stops. */
uint32_t sid_shim_render(sid_shim* s, int16_t* out, uint32_t frames);
void sid_shim_close(sid_shim* s);

#ifdef __cplusplus
}
#endif
#endif
