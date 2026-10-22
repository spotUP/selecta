/* Voice API handed to VOICE-kind replayers (design doc section 4). The shape
 * of nuade's voice API with integer arguments: no float crosses this
 * boundary, so the backend runs on a 68020 without FPU and under interrupt.
 * `sel_host.voices` returns the backend; append-only like sel_host, T6-style
 * goldens pin the layout per architecture (tests/abi/sel_voices.<arch>.txt). */
#ifndef SELECTA_VOICES_H
#define SELECTA_VOICES_H

#include <selecta/host.h>

SEL_EXTERN_C_BEGIN

enum { SEL_SAMPLE_S8 = 0, SEL_SAMPLE_S16 = 1 };

typedef struct sel_sample {
    const void* data;    /* plugin memory (SEL_MEM_SAMPLE); read live */
    uint32_t format;     /* SEL_SAMPLE_S8 or SEL_SAMPLE_S16 */
    uint32_t length;     /* frames */
    uint32_t loop_start; /* frames */
    uint32_t loop_length; /* frames; 0 = one-shot */
} sel_sample;

typedef struct sel_voice_state {
    uint32_t playing;
    const sel_sample* sample; /* the backend's copy of the playing sample, or NULL */
    uint32_t position;        /* source frames, integer part */
    uint32_t pitch_q8;        /* Hz, 24.8 */
    uint32_t volume_q16;      /* 0..65536 */
    int32_t pan;              /* -32768..32767 */
} sel_voice_state;

typedef struct sel_voices sel_voices;
struct sel_voices {
    uint32_t size; /* sizeof(sel_voices) of the host build */
    /* Start `sample` at `start_frame`, dropping any queued sample. */
    void (*play)(const sel_voices*, uint32_t voice, const sel_sample*, uint32_t start_frame);
    /* Play `sample` when the current one reaches its end (loop or length); starts it now when idle. */
    void (*queue)(const sel_voices*, uint32_t voice, const sel_sample*);
    void (*stop)(const sel_voices*, uint32_t voice);
    /* 0 = hold the current pitch. */
    void (*set_pitch_hz)(const sel_voices*, uint32_t voice, uint32_t hz_q8);
    /* Paula period in PAL colour-clock ticks (3546895 / period Hz); 0 = hold. */
    void (*set_period)(const sel_voices*, uint32_t voice, uint32_t paula_period);
    void (*set_volume)(const sel_voices*, uint32_t voice, uint32_t volume_q16);
    void (*set_pan)(const sel_voices*, uint32_t voice, int32_t pan);
    int (*is_playing)(const sel_voices*, uint32_t voice);
    void (*get_state)(const sel_voices*, uint32_t voice, sel_voice_state* out);
    void* ctx;
    /* append only below this line; never reorder, never remove */
};

SEL_EXTERN_C_END

#endif /* SELECTA_VOICES_H */
