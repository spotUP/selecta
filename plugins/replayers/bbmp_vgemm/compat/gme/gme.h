/* Stand-in for the five Game Music Emu calls that bbmp's vgemm_engine.cpp makes (row I710ah). Selecta's selecta_gme has no VGM emulator
 * (VGM belongs to libvgm, row I431), so the engine's in-memory VGM is played by libvgm through gme_compat.cpp instead. This directory
 * comes first on the include path of the bbmp_vgemm module, so the unchanged engine finds this header, not third_party/gme/gme.h. */
#ifndef SELECTA_BBMP_VGEMM_GME_COMPAT_H
#define SELECTA_BBMP_VGEMM_GME_COMPAT_H

struct Music_Emu;
typedef const char* gme_err_t;

gme_err_t gme_open_data(const void* data, long size, Music_Emu** out, int rate);
void gme_ignore_silence(Music_Emu* emu, int ignore);
gme_err_t gme_start_track(Music_Emu* emu, int index);
/* count = interleaved stereo samples (frames * 2); the tail past the song's end is silence. */
gme_err_t gme_play(Music_Emu* emu, int count, short* out);
void gme_delete(Music_Emu* emu);

#endif
