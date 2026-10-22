/* Fixed-point mixer (design doc section 5). Integer only: 16.16 phase,
 * 32-bit accumulate, int16 saturate. Loop, one-shot and queue handling
 * mirrors settle() in nuade_mixer.c (emoon/nuade @ 111c965, l.322) so the
 * A/B test against stock nuade is meaningful. */
#include "mixer.h"

#include <selecta/machine.h>

#include <string.h>

#define MIX_CHUNK 256u

void sel_mixer_init(sel_mixer* m, uint32_t rate, uint32_t voice_count, uint32_t resampler) {
    uint32_t i;
    memset(m, 0, sizeof *m);
    m->rate = rate;
    m->voice_count = voice_count > SEL_MIXER_MAX_VOICES ? SEL_MIXER_MAX_VOICES : voice_count;
    m->resampler = resampler;
    for (i = 0; i < SEL_MIXER_MAX_VOICES; i++) m->voices[i].volume_q16 = 65536u;
    sel_mixer_voices(m);
}

uint32_t sel_mixer_period_step(uint32_t period, uint32_t rate) {
    uint64_t div = (uint64_t)period * rate;
    if (div == 0) return 0;
    return (uint32_t)((((uint64_t)SEL_PAULA_PAL_CLOCK) << 16) / div);
}

uint32_t sel_mixer_hz_step(uint32_t hz_q8, uint32_t rate) {
    if (rate == 0) return 0;
    return (uint32_t)((((uint64_t)hz_q8) << 8) / rate);
}

uint32_t sel_mixer_default_resampler(const sel_tag* machine) {
    uint32_t cpu = (uint32_t)sel_tag_find(machine, SEL_MACHINE_CPU, SEL_CPU_UNKNOWN);
    uint32_t fpu = (uint32_t)sel_tag_find(machine, SEL_MACHINE_FPU, 0);
    if (cpu == SEL_CPU_DESKTOP) return SEL_MIX_RESAMPLE_SINC;
    if (fpu != 0 && (cpu == SEL_CPU_68040 || cpu == SEL_CPU_68060)) return SEL_MIX_RESAMPLE_CUBIC;
    return SEL_MIX_RESAMPLE_LINEAR;
}

static uint32_t sample_end(const sel_sample* s) {
    return s->loop_length != 0 ? s->loop_start + s->loop_length : s->length;
}

/* Moves the position back inside the sample: wraps a loop, moves on to the
 * queued sample or ends the voice. Returns 0 when the voice is idle after. */
static int settle(sel_mixer_voice* v) {
    while (v->position >= sample_end(&v->current)) {
        uint32_t end = sample_end(&v->current);
        if (v->has_queued) {
            v->current = v->queued;
            v->has_queued = 0;
            v->position -= end;
        } else if (v->current.loop_length != 0) {
            while (v->position >= end) v->position -= v->current.loop_length;
        } else {
            v->playing = 0;
            return 0;
        }
    }
    return 1;
}

void sel_mixer_mix(sel_mixer* m, int16_t* stereo, uint32_t frames) {
    int32_t acc[MIX_CHUNK * 2];
    uint32_t done = 0;
    m->mixed_frames += frames;
    while (done < frames) {
        uint32_t n = frames - done;
        uint32_t i, k;
        if (n > MIX_CHUNK) n = MIX_CHUNK;
        memset(acc, 0, (size_t)n * 2u * sizeof acc[0]);
        for (k = 0; k < m->voice_count; k++) {
            sel_mixer_voice* v = &m->voices[k];
            /* Q15 gain: volume (Q16) * pan split (Q15) >> 18, including nuade's 0.5 headroom. */
            int32_t gl = (int32_t)(((uint32_t)v->volume_q16 * (uint32_t)(32768 - v->pan)) >> 18);
            int32_t gr = (int32_t)(((uint32_t)v->volume_q16 * (uint32_t)(32768 + v->pan)) >> 18);
            for (i = 0; i < n && v->playing; i++) {
                int32_t s;
                uint32_t phase;
                if (!settle(v)) break;
                s = sel_resample(&v->current, v->position, v->frac, m->resampler);
                acc[i * 2] += (s * gl) >> 15;
                acc[i * 2 + 1] += (s * gr) >> 15;
                phase = v->frac + v->step;
                v->position += phase >> 16;
                v->frac = phase & 0xFFFFu;
            }
        }
        for (i = 0; i < n * 2u; i++) {
            int32_t x = acc[i];
            stereo[done * 2u + i] = (int16_t)(x > 32767 ? 32767 : (x < -32768 ? -32768 : x));
        }
        done += n;
    }
}
