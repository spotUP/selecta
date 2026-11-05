#ifndef __SOUND__
#define __SOUND__
#include "../core/core.h"
#include "../time/timemanager.h"
struct sound_struct { int freq; int channels; };
extern sound_struct g_snd;
void sound_stream_play(void);
void sound_stream_stop(void);
#endif
