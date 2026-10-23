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

/* Integer adaptive leveler (plan M3 I190, design 6a): for a song with no stored gain. It measures the unscaled signal
 * in short chunks, keeps a running mean-square (a plain running mean for the first LEVELER_TAU_MS, then a slow
 * exponential average) and moves its gain toward the design target with a slow slew in both directions, inside the
 * same +-SEL_GAIN_MAX_CDB clamp. A cumulative gated mean-square gives the estimate that is written back as a
 * provisional gain. State is per song (per slot); no float, no allocation. */
#define SEL_LEVELER_CHUNK 256u         /* frames per measurement chunk */
#define SEL_LEVELER_TARGET_CDB (-1800) /* -18 LUFS (D-M3-9), on the unweighted RMS of both channels */
#define SEL_LEVELER_SLEW_CDB_S 600     /* gain moves at most 6 dB per second */
#define SEL_LEVELER_TAU_MS 2000u       /* averaging time of the running estimate */
#define SEL_LEVELER_MIN_MS 3000u       /* gated signal needed before the estimate is worth storing */

typedef struct sel_leveler {
    uint64_t p;         /* running mean of L^2+R^2 per frame */
    uint64_t sum_e;     /* cumulative gated energy */
    uint64_t sum_n;     /* cumulative gated frames */
    int64_t cur_q16;    /* gain in force, centi-dB in 16.16 */
    int32_t want_cdb;   /* gain the running estimate asks for */
    uint32_t frames;    /* frames measured (all, gated or not): the sentinel */
} sel_leveler;

void sel_leveler_init(sel_leveler* lv);
/* Measures `frames` (<= SEL_LEVELER_CHUNK) unscaled interleaved frames, advances the gain and returns the 16.16
 * gain to apply to exactly these frames. */
uint32_t sel_leveler_chunk(sel_leveler* lv, const int16_t* stereo, uint32_t frames, uint32_t rate);
/* Upper bound of the gain over the next `frames` frames from now (for the call's headroom reference). */
uint32_t sel_leveler_ceiling_q16(const sel_leveler* lv, uint32_t frames, uint32_t rate);
/* Gain in centi-dB the whole gated signal so far asks for; 0 when fewer than SEL_LEVELER_MIN_MS were measured. */
int sel_leveler_estimate_cdb(const sel_leveler* lv, uint32_t rate, int32_t* cdb);
/* 10*log10(x) in centi-dB for x >= 1, integer only (within 1 cdb). */
int32_t sel_db10_cdb(uint64_t x);

SEL_EXTERN_C_END

#endif /* SELECTA_MIXER_LOUDNESS_GAIN_H */
