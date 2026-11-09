/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BBMP_BRT_ENGINE_H
#define BBMP_BRT_ENGINE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct brt_info {
  long duration_ms;
  long long frames_44100;
  char title[256];
  char error[256];
} brt_info;
/* One brt_song is one independent song with its own engine, player and resampler: any number can be open and rendering at once
 * (the process-wide constant tables are filled once, under a lock, and only read afterwards). */
typedef struct brt_song brt_song;
brt_song* brt_open(const uint8_t* data, size_t size, brt_info* info); /* NULL = not a song (info->error says why) */
int brt_start(brt_song* s, uint32_t rate);                            /* (re)start from the top at a host rate */
uint32_t brt_render(brt_song* s, int16_t* out, uint32_t frames);      /* stereo s16; fewer than asked = ended */
void brt_close(brt_song* s);
#ifdef __cplusplus
}
#endif
#endif
