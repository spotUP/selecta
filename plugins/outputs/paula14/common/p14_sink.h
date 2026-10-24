/* paula14 sink (plan M2 phase 4, I123): four Paula channels (0 left high, 1 right high, 2 right low, 3 left low)
 * loop over a double buffer in chip RAM. `submit` splits one buffer of stereo s16 into the idle half and points
 * AUDxLC at it; Paula picks it up when the playing half ends (the audio task, paced by the CIA tick, calls
 * submit again). The register bus is the Paula one, so the hardware output and the shadow twin share this file.
 * Integer only (T54). */
#ifndef SELECTA_P14_SINK_H
#define SELECTA_P14_SINK_H

#include <selecta/host.h>

#include "paula/paula_voices.h"
#include "p14_split.h"

#define SEL_P14_CHANNELS 4u
#define SEL_P14_DRAIN_SPINS 50000000u /* cap on the close drain loop if the host clock stands still */

typedef struct sel_p14 {
    const sel_host* host;
    sel_paula_bus bus;
    sel_p14_table table;
    int8_t* half[SEL_P14_CHANNELS][2]; /* chip RAM */
    uint32_t buf_frames;               /* frames (= bytes per channel) per half, even */
    uint32_t period;
    uint32_t scheduled; /* half Paula plays or has been told to play next */
    uint32_t frames;    /* frames split so far */
    uint32_t bufs;      /* buffer swaps */
    uint32_t hash;      /* FNV-1a over every split byte */
} sel_p14;

/* round(clock / rate); 0 when the period falls outside the hardware range. */
uint32_t sel_p14_period_for_rate(uint32_t rate);
/* The highest rate the chip takes: clock / minimum period. What the output answers SEL_OUT_RATE_FOR with (D-M2-5). */
uint32_t sel_p14_max_rate(void);
/* Allocates the chip halves, writes LEN/PER/VOL, starts the DMA on silence. 0 on success. `strict`: a rate
 * needing a period under the hardware minimum fails (the real chip); else the period clamps (the shadow twin). */
int sel_p14_open(sel_p14* p, const sel_host* host, const sel_paula_bus* bus, uint32_t rate, uint32_t buf_frames,
                 int strict);
void sel_p14_close(sel_p14* p);
/* `pcm`: buf_frames stereo frames. Fills the idle half and schedules it. */
void sel_p14_submit(sel_p14* p, const int16_t* pcm);
/* A final partial buffer: `pcm` still holds buf_frames frames, the caller having zero-padded after the first
 * `n`; only `n` count as frames out. */
void sel_p14_submit_n(sel_p14* p, const int16_t* pcm, uint32_t n);

#endif
