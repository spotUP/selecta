/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BBMP_DW_ENGINE_H
#define BBMP_DW_ENGINE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct dw_info {
  long duration_ms;       /* the whole play: passes of the sequence plus the tail */
  long long once_frames;  /* one pass of the sequence at the rate given to dw_open */
  int passes;
  char format[16];        /* project format, "6.0" */
  char tempo[64];
  char sequence[96];
  char comment[512];
  char error[256];
} dw_info;
int dw_open(const uint8_t* data, size_t size, uint32_t rate, dw_info* info); /* 1 = ready; (re)builds the studio */
int dw_start(void);                                                           /* from the top, after dw_open at the host rate */
uint32_t dw_render(int16_t* out, uint32_t frames);                            /* stereo s16; fewer than asked = ended */
void dw_close(void);
#ifdef __cplusplus
}
#endif
#endif
