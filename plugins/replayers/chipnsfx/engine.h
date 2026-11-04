/* Thin C interface over the CHIPNSFX tracker's own song loader and sound code (third_party/chipnsfx/CHIPNSFX.C,
 * GPL-3.0-or-later, unchanged). The tracker keeps its state in globals, so one song is loaded at a time. */
#ifndef SEL_CHIPNSFX_ENGINE_H
#define SEL_CHIPNSFX_ENGINE_H

#include <stddef.h>

#define CHP_FRAME_SAMPLES 441 /* samples of one engine frame (1/100 s) at CHP_ENGINE_RATE */
#define CHP_ENGINE_RATE 44100
#define CHP_MAX_FRAMES (100 * 60 * 30) /* 30 minutes */

typedef struct chp_info {
    char title[256];
    char comment[256];
    int hertz, divisor;
    long frames; /* length of the song played once, in engine frames */
} chp_info;

/* Loads a song (text) and counts its length; 0 with `why` set when it is not a playable CHIPNSFX song. */
int chp_load(const unsigned char* data, size_t size, chp_info* info, const char** why);
/* Rewinds to the start and plays to the end once (no loop). */
void chp_restart(void);
/* One frame of 8-bit stereo (CHP_FRAME_SAMPLES * 2 bytes, 0 is silence); 0 when the song is over. */
int chp_frame(unsigned char* out);

#endif
