/* Fixed-point mixer (design doc section 5, plan M0b Phase 2). 16.16 source
 * phase, 32-bit stereo accumulate, int16 saturated output. No floating
 * point, no allocation after sel_mixer_init: voice storage is inside the
 * struct. Loop, one-shot and queue semantics follow nuade_mixer.c settle(). */
#ifndef SELECTA_MIXER_H
#define SELECTA_MIXER_H

#include <selecta/voices.h>

SEL_EXTERN_C_BEGIN

#define SEL_MIXER_MAX_VOICES 32u
#define SEL_PAULA_PAL_CLOCK 3546895u

/* Values of the SEL_MIX_RESAMPLE tag. */
enum {
    SEL_MIX_RESAMPLE_NONE = 0,  /* nearest, nuade's own behaviour */
    SEL_MIX_RESAMPLE_LINEAR,    /* 2 points, Q15 fraction */
    SEL_MIX_RESAMPLE_CUBIC,     /* 4-point Catmull-Rom, Q15 coefficients */
    SEL_MIX_RESAMPLE_SINC       /* 8 taps, 256 phases, Q15 table generated at build time */
};

typedef struct sel_mixer_voice {
    uint32_t playing;
    uint32_t has_queued;
    sel_sample current;
    sel_sample queued;
    uint32_t position; /* source frames */
    uint32_t frac;     /* 16-bit fraction of the next frame */
    uint32_t step;     /* 16.16 source frames per output frame */
    uint32_t pitch_q8;
    uint32_t volume_q16;
    int32_t pan;
} sel_mixer_voice;

typedef struct sel_mixer {
    sel_voices api; /* the voices backend; ctx points back at this struct */
    uint32_t rate;
    uint32_t voice_count;
    uint32_t resampler;
    sel_mixer_voice voices[SEL_MIXER_MAX_VOICES];
} sel_mixer;

/* Zeroes the mixer and fills the voices table. voice_count is clamped to
 * SEL_MIXER_MAX_VOICES; resampler is a SEL_MIX_RESAMPLE_* value. */
void sel_mixer_init(sel_mixer* m, uint32_t rate, uint32_t voice_count, uint32_t resampler);

/* The sel_voices backend over this mixer (voices.c). */
const sel_voices* sel_mixer_voices(sel_mixer* m);

/* Overwrites `stereo` (interleaved L/R int16) with `frames` mixed frames. */
void sel_mixer_mix(sel_mixer* m, int16_t* stereo, uint32_t frames);

/* 16.16 source step for a Paula period or a 24.8 pitch at output `rate`. */
uint32_t sel_mixer_period_step(uint32_t period, uint32_t rate);
uint32_t sel_mixer_hz_step(uint32_t hz_q8, uint32_t rate);

/* Resampler for a machine (tags from sel_machine_probe): desktop SINC,
 * 68040/68060 with FPU CUBIC, everything else LINEAR. */
uint32_t sel_mixer_default_resampler(const sel_tag* machine);

/* Resampler core: value at source `pos` + `frac`/65536 of `s`, int16 range.
 * Taps past the end wrap into the loop or read silence; before 0 read frame 0. */
int32_t sel_resample(const sel_sample* s, uint32_t pos, uint32_t frac, uint32_t resampler);

/* Row `phase` (0..255) of the SINC table: 8 Q15 taps for frames pos-3 .. pos+4,
 * summing to 32768. */
const int32_t* sel_resample_sinc_row(uint32_t phase);

SEL_EXTERN_C_END

#endif /* SELECTA_MIXER_H */
