/* Per-song loudness gain and the -1 dBFS peak limiter of the mixer (plan M3 I189, decision D-M3-9, design 6a).
 * Integer only, no allocation. The gain is the stored centi-dB value of the library (I187, I188) turned into a
 * 16.16 multiplier; the limiter runs after the gain and keeps every output sample at or below -1 dBFS. */
#ifndef SELECTA_MIXER_LOUDNESS_GAIN_H
#define SELECTA_MIXER_LOUDNESS_GAIN_H

#include <stdint.h>

#include <selecta/voices.h>

SEL_EXTERN_C_BEGIN

#define SEL_GAIN_UNITY 65536    /* 1.0 in 16.16 */
#define SEL_GAIN_MAX_CDB 2400   /* the stored gain is clamped to +-24 dB */
#define SEL_LIMIT_CEILING 29203 /* 32767 * 10^(-1/20): -1 dBFS */

/* 16.16 multiplier for `cdb` centi-dB, clamped to +-SEL_GAIN_MAX_CDB; within 1 LSB of the exact value. */
uint32_t sel_gain_q16_from_cdb(int32_t cdb);

typedef struct sel_loudness_gain {
    uint32_t active;    /* 0: bypass (loudness off, or no gain known) */
    uint32_t gain_q16;
    uint32_t limit_q16; /* limiter gain in force, SEL_GAIN_UNITY = no reduction */
    uint32_t hits;      /* frames whose peak the limiter reduced */
    uint32_t frames;    /* frames that ran through gain and limiter (the call-count sentinel of T90) */
} sel_loudness_gain;

/* Sets the gain for the next song and re-arms the limiter; `active` 0 makes process a no-op. */
void sel_loudness_gain_set(sel_loudness_gain* g, uint32_t active, uint32_t gain_q16);
/* Gain then limiter, in place, on `frames` interleaved L/R frames. Counts only when active. */
void sel_loudness_gain_process(sel_loudness_gain* g, int16_t* stereo, uint32_t frames);

SEL_EXTERN_C_END

#endif /* SELECTA_MIXER_LOUDNESS_GAIN_H */
