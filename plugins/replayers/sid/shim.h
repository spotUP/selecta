/* C face of the libsidplayfp wrapper (row I434). The C++ API stays inside shim.cpp. */
#ifndef SELECTA_SID_SHIM_H
#define SELECTA_SID_SHIM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sid_shim sid_shim;

/* Chip and ROM configuration applied at every start (MPS-1). model: 0 auto (tune), 1 MOS 6581, 2
 * MOS 8580; clock: 0 auto (tune), 1 PAL, 2 NTSC; filter: 1 on, 0 off; digiboost: 0 auto (on for an
 * 8580), 1 on, 2 off. Each ROM pointer is NULL (libsidplayfp stub) or a full image: kernal and
 * basic 8192 bytes, chargen 4096. */
typedef struct sid_shim_cfg {
    int model, clock, filter, digiboost;
    const uint8_t *kernal, *basic, *chargen;
} sid_shim_cfg;

/* Parses a PSID/RSID image (copied). NULL when libsidplayfp rejects it. */
sid_shim* sid_shim_open(const uint8_t* data, size_t size);
/* (Re)starts emulation at rate Hz; song is 1-based, 0 means the file default. 1 on success. */
int sid_shim_start(sid_shim* s, uint32_t song, uint32_t rate, const sid_shim_cfg* cfg);
uint32_t sid_shim_songs(const sid_shim* s);
uint32_t sid_shim_start_song(const sid_shim* s);
/* Info string 0 = title, 1 = author, 2 = release; empty when absent. */
const char* sid_shim_info(const sid_shim* s, uint32_t index);
const char* sid_shim_format(const sid_shim* s);
/* Interleaved stereo; returns frames written, fewer than asked only when emulation stops. */
uint32_t sid_shim_render(sid_shim* s, int16_t* out, uint32_t frames);
/* Number of successful starts that applied a configuration (the sid.cfg_applied sentinel). */
uint32_t sid_shim_cfg_applied(const sid_shim* s);
/* The embedded OpenROMs images (LGPL-3+); sizes 8192, 8192, 4096. */
const uint8_t* sid_shim_builtin_kernal(void);
const uint8_t* sid_shim_builtin_basic(void);
const uint8_t* sid_shim_builtin_chargen(void);
/* SID chips in the tune (1 to 3). */
uint32_t sid_shim_chips(const sid_shim* s);
/* Mutes (1) or unmutes (0) voice 0-2 of a chip, or voice 3 (its digi samples). The setting survives
 * a restart. */
void sid_shim_mute(sid_shim* s, uint32_t chip, uint32_t voice, uint32_t muted);
void sid_shim_close(sid_shim* s);

#ifdef __cplusplus
}
#endif
#endif
