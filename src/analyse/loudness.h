/* Loudness analyser (I187, design doc 6a, D-M3-9): ITU-R BS.1770-4 / EBU R128 integrated loudness in double
 * precision, plus the RMS+peak fallback. Desktop only: the Amiga never computes loudness (no float on 68k),
 * this directory is not compiled there. The analyser is a plain sample sink; the render loop that feeds it is
 * the engine's ANALYSE command. */
#ifndef SELECTA_ANALYSE_LOUDNESS_H
#define SELECTA_ANALYSE_LOUDNESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_LOUDNESS_TARGET_LUFS (-18.0) /* ReplayGain 2.0 reference (D-M3-9) */

typedef struct sel_loudness sel_loudness;

typedef struct sel_loudness_result {
    int gated;         /* 1: integrated loudness found (R128); 0: every block was gated out, only the fallback exists */
    double lufs;       /* gated integrated loudness; equals fallback_db when !gated */
    double fallback_db;/* RMS of all channels, no weighting, in dBFS (the fallback loudness) */
    double peak_db;    /* sample peak in dBFS */
    double gain_db;    /* SEL_LOUDNESS_TARGET_LUFS - lufs, limited to +-24 dB; 0 for silence */
    int32_t gain_cdb;  /* gain_db in centi-dB, rounded: the value stored in songs.loudness_gain */
    uint64_t frames;
} sel_loudness_result;

/* Stereo analyser at `rate` Hz. NULL when allocation fails or the rate is below 8000. */
sel_loudness* sel_loudness_new(uint32_t rate);
void sel_loudness_free(sel_loudness* l);
/* Feeds interleaved stereo; the s16 form scales by 1/32768. */
void sel_loudness_add_s16(sel_loudness* l, const int16_t* stereo, uint32_t frames);
void sel_loudness_add_f64(sel_loudness* l, const double* stereo, uint32_t frames);
/* Computes the result from everything fed so far (can be called once at the end). */
void sel_loudness_result_get(sel_loudness* l, sel_loudness_result* out);

#ifdef __cplusplus
}
#endif
#endif
