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
int brt_open(const uint8_t* data, size_t size, brt_info* info); /* 1 = ready */
long long brt_total_frames(void);                                /* frames at 44100 Hz */
int brt_start(uint32_t rate);                                    /* (re)start from the top at a host rate */
uint32_t brt_render(int16_t* out, uint32_t frames);              /* stereo s16; fewer than asked = ended */
void brt_close(void);
#ifdef __cplusplus
}
#endif
#endif
