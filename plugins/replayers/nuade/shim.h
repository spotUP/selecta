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

/* io_shim.c (I710au): an NuadeIo that reads files inside the song's own directory (and below it) and refuses any
 * other url. The struct is the NuadeIo's private data, so it must outlive the player (it lives in the adapter state).
 * song_path NULL, or a directory too long for root: falls back to sel_nuade_io_none(). */
typedef struct SelNuadeSiblingIo {
    NuadeIo io;
    const sel_host* host;
    char root[1024]; /* the song's directory with its trailing separator, "" for a bare file name */
} SelNuadeSiblingIo;
const NuadeIo* sel_nuade_io_sibling(SelNuadeSiblingIo* io, const sel_host* host, const char* song_path);

#endif /* SELECTA_NUADE_SHIM_H */
