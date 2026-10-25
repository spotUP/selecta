/* Visualisation frame and taps (design doc section 8, plan M1.3 Phase 1).
 * The engine fills one sel_viz_frame per 20 ms of rendered audio on the audio
 * thread (preallocated, integer only, no locks) and publishes it through a
 * triple buffer. The pump thread acquires the newest frame, adds the spectrum
 * and the phase correlation, and the snapshot (sel_state.viz) points at it.
 * UIs and visualizer plugins read only this frame. */
#ifndef SELECTA_VIZ_H
#define SELECTA_VIZ_H

#include <selecta/api.h>

#include <stdint.h>

SEL_EXTERN_C_BEGIN

#define SEL_VIZ_MAX_VOICES 32u
#define SEL_VIZ_SCOPE_FRAMES 256u /* last mixed frames kept per voice */
#define SEL_VIZ_PCM_FRAMES 1024u  /* post-mix stereo ring (phase view, spectrum) */
#define SEL_VIZ_BANDS 16u
#define SEL_VIZ_FFT_SIZE 256u
#define SEL_VIZ_BLOCKS_PER_SECOND 50u /* one frame per 20 ms: rate / 50 audio frames */

typedef struct sel_viz_frame {
    uint32_t seq;            /* 0: no frame yet, else counts up by one per block */
    uint32_t rate;           /* output rate the block was mixed at */
    uint32_t voice_count;    /* voices of the VOICE-kind replayer; 0 for RENDER kind */
    uint32_t spectrum_valid; /* pump side: bands computed */
    int16_t peak[SEL_VIZ_MAX_VOICES]; /* Q15, post-volume, pre-pan, this block */
    int16_t vu[SEL_VIZ_MAX_VOICES];   /* Q15, instant attack, release level -= level >> 3 per block */
    uint32_t pitch_q8[SEL_VIZ_MAX_VOICES];   /* Hz in 24.8, 0 when the voice is idle */
    uint16_t instrument[SEL_VIZ_MAX_VOICES]; /* 0: the replayer does not report it */
    uint8_t note_on[SEL_VIZ_MAX_VOICES];     /* 1: a sample started during this block */
    int16_t scope[SEL_VIZ_MAX_VOICES][SEL_VIZ_SCOPE_FRAMES]; /* oldest first */
    int16_t pcm[SEL_VIZ_PCM_FRAMES * 2u];                    /* interleaved L/R, oldest first */
    int16_t band[SEL_VIZ_BANDS]; /* Q15, 43 Hz to 16 kHz log spaced (pump side) */
    int16_t correlation;         /* Q15, +32767 mono, -32767 inverted (pump side) */
} sel_viz_frame;

/* The audio-side tap: the post-mix ring, the VU state and the triple buffer.
 * Lives inside the engine, filled by sel_engine_render. */
typedef struct sel_viz_tap {
    sel_viz_frame frames[3];
    uint32_t write_idx;       /* audio private */
    uint32_t read_idx;        /* pump private */
    volatile uint32_t middle; /* shared: buffer index, plus 4 when it holds an unread frame */
    uint32_t seq;
    uint32_t block_left;      /* frames until the current block closes; 0: start a new one */
    uint32_t pcm_head;        /* next write index of the ring, in frames */
    int16_t pcm[SEL_VIZ_PCM_FRAMES * 2u];
    int16_t vu[SEL_VIZ_MAX_VOICES];
} sel_viz_tap;

void sel_viz_tap_init(sel_viz_tap* t);

/* Audio thread. Frames of the open block still to render; opens a block of
 * `rate / 50` frames (at least 1) when none is open. */
uint32_t sel_viz_block_left(sel_viz_tap* t, uint32_t rate);
/* Appends `frames` rendered stereo frames to the post-mix ring and counts
 * them against the open block. Returns 1 when the block is now complete. */
int sel_viz_pcm_push(sel_viz_tap* t, const int16_t* stereo, uint32_t frames);
/* Audio thread. Starts the frame of the closed block: sequence, rate and the
 * post-mix ring copied oldest first. The caller fills the voice fields, then
 * calls sel_viz_publish. */
sel_viz_frame* sel_viz_begin(sel_viz_tap* t, uint32_t rate);
void sel_viz_publish(sel_viz_tap* t);

/* VU ballistics: instant attack, release `level -= level >> 3` (at least 1)
 * per block. */
int16_t sel_viz_vu_step(int16_t level, int16_t peak);

/* Pump thread. Takes the newest published frame when there is one (returns 1
 * and makes it current). The current frame is valid until the next call. */
int sel_viz_acquire(sel_viz_tap* t);
const sel_viz_frame* sel_viz_current(const sel_viz_tap* t);
/* Pump thread. Fills band[], correlation and spectrum_valid of the current
 * frame from its PCM ring. `with_spectrum` 0 leaves the bands at zero. */
void sel_viz_analyse(sel_viz_tap* t, int with_spectrum);

/* Pure functions (unit tests, pump thread). */
/* Phase correlation of interleaved stereo, Q15: sum(LR) / sqrt(sum(LL) * sum(RR)).
 * 0 for silence. */
int32_t sel_viz_correlation(const int16_t* stereo, uint32_t frames);
/* 256-point integer FFT of the mono sum (L + R) / 2 of the last 256 frames of
 * `stereo` (Hann window, Q15), reduced to 16 log-spaced bands, Q15. */
void sel_viz_spectrum(const int16_t* stereo, uint32_t frames, uint32_t rate, int16_t band[SEL_VIZ_BANDS]);

/* ---- Render target of the viz plugins (design doc section 8; plan DUI-P4-11, moved from M4-P3-3) ----
 * A viz plugin (kind VIZ) draws one sel_viz_frame into a sel_surface. The host owns the pixels and the colours: the
 * desktop UI hands an RGBA32 surface and pixel values taken from its theme tokens, an Amiga UI hands an INDEX8
 * (chunky or planar-converted) surface and palette indices. The plugin never learns which; it only writes the `pixel`
 * values it is given. Tags: VIZ_NAME (STR), VIZ_FRAME (render hook, sel_viz_render_fn), VIZ_OPEN / VIZ_CLOSE (optional). */
typedef enum sel_pixfmt {
    SEL_PIX_INDEX8 = 0, /* one byte per pixel, palette index */
    SEL_PIX_RGBA32 = 1  /* one uint32_t per pixel, 0xAARRGGBB */
} sel_pixfmt;

typedef struct sel_surface {
    uint32_t w, h;
    uint32_t pitch; /* bytes per row */
    sel_pixfmt fmt;
    void* pixels;
    const uint32_t* palette; /* INDEX8: 0xAARRGGBB per index, NULL for RGBA32 */
} sel_surface;

/* Pixel values for the plugin, already in the surface's format (palette index or 0xAARRGGBB). */
typedef struct sel_viz_colors {
    uint32_t background; /* the plugin clears to this */
    uint32_t grid;       /* centre lines, bar tracks */
    uint32_t primary;    /* main trace or bars */
    uint32_t secondary;  /* second channel, peaks */
    uint32_t accent;     /* markers */
} sel_viz_colors;

/* VIZ_FRAME hook: draws `f` over the whole surface. Returns the number of primitives drawn (0: nothing, e.g. no frame
 * yet). Integer only; runs on the UI thread. */
typedef uint32_t (*sel_viz_render_fn)(const sel_viz_frame* f, sel_surface* s, const sel_viz_colors* c);

SEL_EXTERN_C_END

#endif /* SELECTA_VIZ_H */
