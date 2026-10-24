/* Shared skeleton of the two AHI outputs (plan M2 phase 4, I124/I125): `ahi` sends buffers to ahi.device, `ahi-shadow`
 * counts the same hand-offs on the desktop. Both are PCM outputs: the engine pulls a mixed stream (RENDER kind, or
 * VOICE kind through the mixer) and the output fills two buffers in turn. Buffer submitted = one swap (`ahi_bufs`).
 * A submit that finds the previous buffer already finished means the device ran dry (`ahi_underruns`). The buffer
 * length comes from ahi_cfg.c (per CPU class, `output.buffer` overrides). */
#ifndef SELECTA_AHI_OUT_H
#define SELECTA_AHI_OUT_H

#include <selecta/output.h>

#include "ahi_cfg.h"

typedef struct sel_ahi_backend {
    const char* id;
    const char* name;
    /* Opens the device for `rate`; returns a context or NULL. Two buffers of `frames` stereo s16 frames follow. */
    void* (*open)(const sel_host* host, uint32_t rate, uint32_t frames);
    void (*close)(const sel_host* host, void* ctx);
    /* Hands buffer `idx` (0 or 1) to the device, linked behind the other one. Does not block. */
    void (*submit)(void* ctx, uint32_t idx, const int16_t* pcm, uint32_t frames);
    /* Blocks until buffer `idx` has finished playing (returns at once if it was never submitted). */
    void (*wait)(void* ctx, uint32_t idx);
    /* 1 when buffer `idx` was submitted and has finished playing. */
    int (*finished)(void* ctx, uint32_t idx);
} sel_ahi_backend;

void* sel_ahi_out_open(const sel_ahi_backend* be, const sel_host* host, uint32_t rate, uint32_t channels);
/* Fills `hooks` (12 tags) and `desc` (5 tags) for a plugin entry. */
const sel_tag* sel_ahi_out_describe(const sel_ahi_backend* be, sel_out_open_fn open, sel_tag* hooks, sel_tag* desc);

#endif
