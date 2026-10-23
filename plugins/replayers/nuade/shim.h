/* Internal to the nuade adapter plugin: the state shared by adapter.c,
 * voices_shim.c and io_shim.c. */
#ifndef SELECTA_NUADE_SHIM_H
#define SELECTA_NUADE_SHIM_H

#include <nuade/nuade.h>
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#define SEL_NUADE_MAX_VOICES 32u

/* The type nuade's voice.h leaves opaque. Carries the Selecta voices
 * backend and, per voice, the NuadeSample a player last handed in, so
 * nuade_voice_get_state can return a pointer that stays valid. */
struct NuadeVoices {
    const sel_voices* api;
    uint32_t gain; /* output gain percent, 0 to 100 (SEL_RP_CONFIG nuade.gain) */
    NuadeSample current[SEL_NUADE_MAX_VOICES];
};

/* io_shim.c: an NuadeIo whose files are all absent. */
const NuadeIo* sel_nuade_io_none(void);

#endif /* SELECTA_NUADE_SHIM_H */
