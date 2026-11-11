/* Selecta glue over the SunVox engine: open a song from memory, render interleaved int16 stereo. One instance at a time
 * (the engine keeps global tables and g_sunvox_engine). The caller checks the song header and version before the engine
 * sees the data (plugins/replayers/sunvox/probe.c sv_probe, the one check, shared with the Amiga manifest, I427e4). */
#include "sunvox_engine.h"
#include "filesystem/v3nus_fs.h"
#include "sound/sound.h"
#include <string.h>
#include <stdlib.h>

struct sunvox_glue { sunvox_engine eng; int rate; };
static sunvox_glue* g_active;

void sunvox_shim_pump(sunvox_engine* s) {
    short scratch[2];
    sunvox_render_piece_of_sound(0, scratch, 1, 2, g_snd.freq, 0, s);
}

extern "C" sunvox_glue* sunvox_glue_open(const unsigned char* data, unsigned long size, int rate) {
    sunvox_glue* g;
    if (g_active != NULL) return NULL;
    g = (sunvox_glue*)calloc(1, sizeof *g);
    if (g == NULL) return NULL;
    g->rate = rate;
    g_snd.freq = rate;
    g_snd.channels = 2;
    sunvox_engine_init(0, &g->eng);
    g_sunvox_engine = &g->eng;
    sunvox_shim_set_song(data, size);
    sunvox_load_song("memory.sunvox", &g->eng);
    sunvox_shim_set_song(NULL, 0);
    g_sunvox_engine = &g->eng;
    sunvox_play_from_beginning(&g->eng);
    g->eng.stop_at_the_end_of_song = 1;
    g_active = g;
    return g;
}

/* Frames rendered (fewer than asked at the song end). */
extern "C" unsigned sunvox_glue_render(sunvox_glue* g, short* out, unsigned frames) {
    unsigned done = 0;
    while (done < frames && !g->eng.end_of_song) {
        unsigned n = frames - done > 512u ? 512u : frames - done;
        sunvox_render_piece_of_sound(0, out + (size_t)done * 2u, (int)n, 2, g->rate, 0, &g->eng);
        done += n;
    }
    if (done > frames) done = frames;
    return done;
}

extern "C" unsigned long sunvox_glue_length_frames(sunvox_glue* g) { return g->eng.song_len; }
extern "C" const char* sunvox_glue_title(sunvox_glue* g) { return g->eng.song_name ? g->eng.song_name : ""; }

extern "C" void sunvox_glue_close(sunvox_glue* g) {
    if (g == NULL) return;
    sunvox_stop(&g->eng);
    sunvox_engine_close(&g->eng);
    if (g_active == g) g_active = NULL;
    free(g);
}
