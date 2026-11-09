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

#define SEL_VIZ_SID_CHIPS 3u
#define SEL_VIZ_SID_REGS 25u /* $D400-$D418 */
typedef struct sel_viz_sid_chip {
    uint8_t reg[SEL_VIZ_SID_REGS]; /* raw register mirror at block close */
    uint8_t env[3];                /* envelope level per voice, 0..63 */
    uint8_t glow[3][9];            /* per voice: control bits 0..7 (Gate..Noise), [8] = routed to filter; 0..4 */
    uint8_t mode_glow[3];          /* LP, BP, HP; 0..4 */
} sel_viz_sid_chip;

typedef struct sel_viz_frame {
    uint32_t seq;            /* 0: no frame yet, else counts up by one per block */
    uint32_t rate;           /* output rate the block was mixed at */
    uint32_t voice_count;    /* voices of the VOICE-kind replayer; 0 for RENDER kind */
    uint32_t spectrum_valid; /* pump side: bands computed */
    uint32_t falloff_runs;   /* pump side: times the peak falloff ran on this tap (I292b sentinel) */
    int16_t peak[SEL_VIZ_MAX_VOICES]; /* Q15, post-volume, pre-pan, this block */
    int16_t vu[SEL_VIZ_MAX_VOICES];   /* Q15, instant attack, release level -= level >> 3 per block */
    uint32_t pitch_q8[SEL_VIZ_MAX_VOICES];   /* Hz in 24.8, 0 when the voice is idle */
    uint16_t instrument[SEL_VIZ_MAX_VOICES]; /* 0: the replayer does not report it */
    uint8_t note_on[SEL_VIZ_MAX_VOICES];     /* 1: a sample started during this block */
    uint32_t sid_chips;                      /* 0: no register feed this block; 1..3 (I434j) */
    sel_viz_sid_chip sid[SEL_VIZ_SID_CHIPS];
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
    int16_t smooth[SEL_VIZ_BANDS]; /* pump private: the bands after the falloff (I292b) */
    uint32_t smooth_seq, falloff_runs;
    uint32_t block_left;      /* frames until the current block closes; 0: start a new one */
    uint32_t pcm_head;        /* next write index of the ring, in frames */
    int16_t pcm[SEL_VIZ_PCM_FRAMES * 2u];
    int16_t vu[SEL_VIZ_MAX_VOICES];
    sel_viz_sid_chip sid_state[SEL_VIZ_SID_CHIPS]; /* audio private: previous block, for the afterglow */
    /* Pump private: the visual delay (I1042). `delay` blocks of latency hold back the frames the UI sees, so the scope
     * matches what the output is playing. `queue` has delay + 1 slots (oldest first from `q_head`) and a last slot,
     * `shown`, that holds the frame handed out as current. NULL while delay is 0. */
    sel_viz_frame* queue;
    uint32_t delay, q_head, q_count;
} sel_viz_tap;

void sel_viz_tap_init(sel_viz_tap* t);
/* Pump thread. Holds the frames the pump sees back by `blocks` blocks (0: none; at most SEL_VIZ_MAX_DELAY_BLOCKS), the
 * output latency in blocks. Returns the delay now in force. Allocates the queue; sel_viz_tap_free releases it. */
#define SEL_VIZ_MAX_DELAY_BLOCKS 250u
uint32_t sel_viz_set_delay(sel_viz_tap* t, uint32_t blocks);
uint32_t sel_viz_delay(const sel_viz_tap* t);
void sel_viz_tap_free(sel_viz_tap* t);

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

/* Afterglow of one chip, once per block (HippoPlayer .updateValue): a flag that is on sets its counter to 0; a flag
 * that drops from on to off sets it to 4; an off flag counts its counter down to 0. `prev_reg` is the previous block's
 * register mirror, `prev` its glow counters (NULL: all off). Fills now->glow and now->mode_glow from now->reg. */
void sel_viz_sid_glow(sel_viz_sid_chip* now, const uint8_t prev_reg[SEL_VIZ_SID_REGS], const sel_viz_sid_chip* prev);

/* Pump thread. Takes the newest published frame when there is one (returns 1
 * and makes it current). The current frame is valid until the next call. */
int sel_viz_acquire(sel_viz_tap* t);
const sel_viz_frame* sel_viz_current(const sel_viz_tap* t);
/* Pump thread. Fills band[], correlation and spectrum_valid of the current
 * frame from its PCM ring. `with_spectrum` 0 leaves the bands at zero. */
void sel_viz_analyse(sel_viz_tap* t, int with_spectrum);

/* Peak falloff (I292b). Q15 per second a band falls: a full scale bar empties in about 1.3 s. */
#define SEL_VIZ_FALL_PER_SECOND 25000u
/* level[i] rises to raw[i] at once, else falls by SEL_VIZ_FALL_PER_SECOND * dt_ms / 1000 (never below raw[i] or 0). */
int sel_viz_falloff(int16_t level[], const int16_t raw[], uint32_t n, uint32_t dt_ms); /* returns 1: the step ran */

/* Pure functions (unit tests, pump thread). */
/* Phase correlation of interleaved stereo, Q15: sum(LR) / sqrt(sum(LL) * sum(RR)).
 * 0 for silence. */
int32_t sel_viz_correlation(const int16_t* stereo, uint32_t frames);
/* 256-point integer FFT of the mono sum (L + R) / 2 over consecutive 256 frame windows of `stereo`, ending at the
 * newest frame (Hann window, Q15); the bin magnitudes are averaged over the windows (I292b), then reduced to 16
 * log-spaced bands, Q15. Cost: frames / 256 FFTs per call (4 for the 1024 frame ring). */
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

/* I1230: the host's own drawing service, so a viz plugin draws text in the app's font and shapes in the app's design
 * (theme radius, extra theme roles) instead of its private pixel glyphs. Optional: a host that leaves sel_surface.pen
 * NULL (the Amiga MUI UI, the plugin unit tests) gets the plugin's own drawing. Every call draws into the surface the
 * plugin was handed, at surface coordinates, clipped to it. Pixel values are in the surface's format, like
 * sel_viz_colors. */
typedef struct sel_viz_pen {
    void* ctx;
    int32_t line_h; /* height of one label line in surface pixels */
    int32_t radius; /* corner radius of cards (theme radius); controls and bars use radius / 2 */
    uint32_t card;      /* card background on the plugin background (raised surface) */
    uint32_t track;     /* empty bar track and unlit control inside a card */
    uint32_t text;      /* headings */
    uint32_t muted;     /* labels */
    uint32_t on_accent; /* text on a primary (accent) fill */
    /* Pixel width of `s` in the label font (strong: the heading weight). */
    int32_t (*text_width)(void* ctx, const char* s, int strong);
    /* One line with its top left at (x, y), ellipsized to max_w. Returns the glyphs drawn. */
    uint32_t (*draw_text)(void* ctx, int32_t x, int32_t y, int32_t max_w, const char* s, uint32_t px, int strong);
    /* Filled rectangle with rounded corners (radius 0: square). */
    void (*round_rect)(void* ctx, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint32_t px);
    /* a towards b by q8 / 256 (afterglow shades). */
    uint32_t (*mix)(void* ctx, uint32_t a, uint32_t b, uint32_t q8);
} sel_viz_pen;

typedef struct sel_surface {
    uint32_t w, h;
    uint32_t pitch; /* bytes per row */
    sel_pixfmt fmt;
    void* pixels;
    const uint32_t* palette; /* INDEX8: 0xAARRGGBB per index, NULL for RGBA32 */
    uint32_t pix_aspect_q8;  /* pixel height over width x 256 of the display (512 on 640x256 PAL); 0 or 256 square. Text and round shapes divide their vertical size by it */
    const sel_viz_pen* pen;  /* I1230: the host's drawing service, NULL = none (the plugin draws on its own) */
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
