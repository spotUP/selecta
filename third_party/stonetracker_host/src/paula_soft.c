// paula_soft.c — Amiga Paula chip emulator, the one copy every software-Paula
// replayer builds (see paula_soft.h). Fourteen engine-local copies drifted
// apart before this: some applied AUDxLC/AUDxLEN writes at once (PumaTracker
// writes LEN=1 right after starting a sample and cut every note to 2 bytes),
// some never looped, some restarted a running channel on every DMACON set.
#include "paula_soft.h"
#include <string.h>

typedef struct {
    // Buffer playing now, latched from the registers.
    const int8_t* sample;
    uint32_t      sample_len;   // bytes
    float         pos;          // fractional byte position within the buffer
    float         step;         // bytes per output frame = clock / (period * rate)
    float         volume;       // 0-1
    int           dma_on;
    // Registers (AUDxLC / AUDxLEN): latched on DMA start and at every wrap.
    const int8_t* reg_sample;
    uint32_t      reg_len;      // bytes
    uint16_t      reg_period;   // raw AUDxPER, for lock tests
    uint8_t       reg_vol;      // raw AUDxVOL, for lock tests
    // audio.device layer: one-shot buffers and the queued follow-on.
    int           loop;         // 1 = chip repeat (default), 0 = one-shot
    const int8_t* next_sample;
    uint32_t      next_len;
    int           next_loop;
    uint16_t      next_period;  // 0 = keep current
    uint8_t       next_vol;
    int           next_valid;
    int           completed;    // a one-shot ended since the last poll
    int           latched;      // LC/LEN latched since the last poll (AUDx interrupt)
} PaulaChannel;

static PaulaChannel s_ch[PAULA_CHANNELS];
static float        s_paula_clock = PAULA_CLOCK_PAL;
static float        s_output_rate = (float)PAULA_RATE_PAL;
static uint16_t     s_last_dmacon = 0;
// Mixer state: outside s_ch so paula_reset (every song load) keeps mute/solo.
static float        s_channel_gain[PAULA_CHANNELS] = { 1.0f, 1.0f, 1.0f, 1.0f };
static float        s_channel_peaks[PAULA_CHANNELS];
// Cumulative note starts per channel: DMA off->on edges, the chip's own note trigger.
static uint32_t     s_note_starts[PAULA_CHANNELS];

#define SCOPE_LEN 256
static int16_t s_scope[PAULA_CHANNELS][SCOPE_LEN];
static int     s_scope_pos = 0;

static PaulaNoteEvent s_capture[PAULA_CAPTURE_MAX];
static int            s_capture_count = 0;
static int            s_capturing = 0;
static uint32_t       s_capture_tick = 0;
static uint16_t       s_prev_period[PAULA_CHANNELS];
static uint8_t        s_prev_volume[PAULA_CHANNELS];
static uint8_t        s_dirty[PAULA_CHANNELS];

static int valid_ch(int ch) { return ch >= 0 && ch < PAULA_CHANNELS; }

static float step_for(uint16_t period) {
    return s_paula_clock / ((float)period * s_output_rate);
}

void paula_reset(void) {
    int i;
    memset(s_ch, 0, sizeof(s_ch));
    for (i = 0; i < PAULA_CHANNELS; i++) { s_ch[i].loop = 1; s_channel_peaks[i] = 0.0f; }
    s_last_dmacon = 0;
    memset(s_note_starts, 0, sizeof(s_note_starts));
}

void paula_set_clock(float clock) { s_paula_clock = clock; }

void paula_set_output_rate(float rate) {
    if (rate > 0.0f) s_output_rate = rate;
}

void paula_set_channel_gain(int ch, float gain) {
    if (!valid_ch(ch)) return;
    s_channel_gain[ch] = gain < 0.0f ? 0.0f : (gain > 1.0f ? 1.0f : gain);
}

void paula_set_sample_ptr(int ch, const int8_t* data) {
    if (!valid_ch(ch)) return;
    s_ch[ch].reg_sample = data;
}

void paula_set_length(int ch, uint16_t len_words) {
    if (!valid_ch(ch)) return;
    s_ch[ch].reg_len = (uint32_t)len_words * 2;
}

void paula_set_period(int ch, uint16_t period) {
    if (!valid_ch(ch) || period == 0) return;
    s_ch[ch].reg_period = period;
    s_ch[ch].step = step_for(period);
    if (s_capturing && period != s_prev_period[ch]) { s_prev_period[ch] = period; s_dirty[ch] = 1; }
}

void paula_set_volume(int ch, uint8_t vol) {
    if (!valid_ch(ch)) return;
    uint8_t v = vol > 64 ? 64 : vol;
    s_ch[ch].reg_vol = v;
    s_ch[ch].volume = (float)v / 64.0f;
    if (s_capturing && v != s_prev_volume[ch]) { s_prev_volume[ch] = v; s_dirty[ch] = 1; }
}

void paula_dma_write(uint16_t dmacon) {
    int enable = (dmacon & 0x8000) != 0;
    int i;
    s_last_dmacon = dmacon;
    for (i = 0; i < PAULA_CHANNELS; i++) {
        PaulaChannel* c = &s_ch[i];
        if (!(dmacon & (1 << i))) continue;
        if (enable) {
            if (c->dma_on) continue;  // already running: the chip ignores it
            c->sample     = c->reg_sample;
            c->sample_len = c->reg_len;
            c->pos        = 0.0f;
            c->completed  = 0;
            c->dma_on     = 1;
            c->latched    = 1;
            s_note_starts[i]++;
        } else {
            c->dma_on = 0;
        }
    }
}

void paula_set_loop(int ch, int loop) {
    if (!valid_ch(ch)) return;
    s_ch[ch].loop = loop;
}

void paula_set_next(int ch, const int8_t* data, uint16_t len_words, int loop,
                    uint16_t period, uint8_t vol) {
    if (!valid_ch(ch)) return;
    PaulaChannel* c = &s_ch[ch];
    c->next_sample = data;
    c->next_len    = (uint32_t)len_words * 2;
    c->next_loop   = loop;
    c->next_period = period;
    c->next_vol    = vol > 64 ? 64 : vol;
    c->next_valid  = 1;
}

int paula_is_active(int ch) { return valid_ch(ch) ? s_ch[ch].dma_on : 0; }

int paula_poll_completion(int ch) {
    if (!valid_ch(ch)) return 0;
    int done = s_ch[ch].completed;
    s_ch[ch].completed = 0;
    return done;
}

void paula_channel_dma_off(int ch) {
    if (!valid_ch(ch)) return;
    s_ch[ch].dma_on     = 0;
    s_ch[ch].next_valid = 0;
    s_ch[ch].completed  = 0;
}

// End of the playing buffer. Returns 0 if the channel stopped.
static int buffer_end(PaulaChannel* c) {
    c->pos -= (float)c->sample_len;
    if (c->loop) {
        // The chip's repeat: re-latch the registers. A replayer that wants a
        // one-shot has pointed them at a silent word (or LEN 0) by now.
        c->sample     = c->reg_sample;
        c->sample_len = c->reg_len;
        c->latched    = 1;
    } else {
        c->completed = 1;
        if (!c->next_valid) { c->dma_on = 0; return 0; }
        // audio.device double-buffering: swap in the queued block, gapless.
        c->sample     = c->reg_sample = c->next_sample;
        c->sample_len = c->reg_len    = c->next_len;
        c->loop       = c->next_loop;
        if (c->next_period > 0) c->step = step_for(c->next_period);
        c->volume     = (float)c->next_vol / 64.0f;
        c->next_valid = 0;
        c->pos        = 0.0f;
        c->latched    = 1;
    }
    if (!c->sample || c->sample_len == 0) { c->dma_on = 0; return 0; }
    if ((uint32_t)c->pos >= c->sample_len) c->pos = 0.0f;
    return 1;
}

static float sample_channel(PaulaChannel* c) {
    if (!c->dma_on || c->step <= 0.0f || !c->sample || c->sample_len == 0) return 0.0f;
    if ((uint32_t)c->pos >= c->sample_len && !buffer_end(c)) return 0.0f;
    uint32_t idx = (uint32_t)c->pos;
#ifdef __wasm__
    // A replayer bug that points LC outside linear memory must not trap the
    // whole engine: stop the channel instead.
    uintptr_t memsize = (uintptr_t)__builtin_wasm_memory_size(0) * 65536u;
    if ((uintptr_t)c->sample < 1024u || (uintptr_t)c->sample + idx >= memsize) {
        c->dma_on = 0;
        return 0.0f;
    }
#endif
    float s = (float)c->sample[idx] / 128.0f;
    c->pos += c->step;
    return s * c->volume;
}

int paula_poll_block_start(int ch) {
    if (!valid_ch(ch)) return 0;
    int l = s_ch[ch].latched;
    s_ch[ch].latched = 0;
    return l;
}

int paula_render(float* buffer, int frames) {
    return paula_render_voices(buffer, 0, 0, frames);
}

int paula_render_voices(float* buffer, float* voices, int stride, int frames) {
    int i, ch;
    for (i = 0; i < frames; i++) {
        float out[PAULA_CHANNELS];
        for (ch = 0; ch < PAULA_CHANNELS; ch++) {
            out[ch] = sample_channel(&s_ch[ch]) * s_channel_gain[ch];
            float a = out[ch] < 0.0f ? -out[ch] : out[ch];
            if (a > s_channel_peaks[ch]) s_channel_peaks[ch] = a;
            s_scope[ch][s_scope_pos] = (int16_t)(out[ch] * 32767.0f);
            if (voices) voices[ch * stride + i] = out[ch];
        }
        s_scope_pos = (s_scope_pos + 1) & (SCOPE_LEN - 1);
        // Amiga hard panning: ch0,3 -> left; ch1,2 -> right; halved to not clip
        buffer[i * 2 + 0] = (out[0] + out[3]) * 0.5f;
        buffer[i * 2 + 1] = (out[1] + out[2]) * 0.5f;
    }
    return frames;
}

void paula_get_note_start_counts(uint32_t* out, int n) {
    int i;
    for (i = 0; i < n; i++) out[i] = i < PAULA_CHANNELS ? s_note_starts[i] : 0;
}

void paula_get_channel_levels(float* out4) {
    int ch;
    for (ch = 0; ch < PAULA_CHANNELS; ch++) {
        out4[ch] = s_channel_peaks[ch];
        s_channel_peaks[ch] = 0.0f;
    }
}

uintptr_t paula_scope_ptr(int ch) { return valid_ch(ch) ? (uintptr_t)s_scope[ch] : 0; }
int       paula_scope_len(void)   { return SCOPE_LEN; }
int       paula_scope_pos(void)   { return s_scope_pos; }

uint16_t  paula_reg_period(int ch)     { return valid_ch(ch) ? s_ch[ch].reg_period : 0; }
uint8_t   paula_reg_volume(int ch)     { return valid_ch(ch) ? s_ch[ch].reg_vol : 0; }
uint32_t  paula_reg_len_bytes(int ch)  { return valid_ch(ch) ? s_ch[ch].reg_len : 0; }
uintptr_t paula_reg_sample_ptr(int ch) { return valid_ch(ch) ? (uintptr_t)s_ch[ch].reg_sample : 0; }
uint16_t  paula_last_dmacon(void)      { return s_last_dmacon; }

void paula_debug_state(int ch, float* out8) {
    if (!valid_ch(ch)) return;
    PaulaChannel* c = &s_ch[ch];
    out8[0] = c->dma_on ? 1.0f : 0.0f;
    out8[1] = c->volume;
    out8[2] = c->step;
    out8[3] = c->pos;
    out8[4] = (float)c->sample_len;
    out8[5] = c->sample ? 1.0f : 0.0f;
    out8[6] = c->reg_sample ? 1.0f : 0.0f;
    out8[7] = (float)c->reg_len;
}

void paula_capture_start(void) {
    int i;
    s_capture_count = 0;
    s_capture_tick = 0;
    s_capturing = 1;
    for (i = 0; i < PAULA_CHANNELS; i++) { s_prev_period[i] = 0; s_prev_volume[i] = 0; s_dirty[i] = 0; }
}

void paula_capture_stop(void) { s_capturing = 0; }

void paula_capture_tick(void) {
    int i;
    if (!s_capturing) return;
    for (i = 0; i < PAULA_CHANNELS; i++) {
        if (s_dirty[i] && s_capture_count < PAULA_CAPTURE_MAX) {
            PaulaNoteEvent* e = &s_capture[s_capture_count++];
            e->tick    = s_capture_tick;
            e->channel = (uint8_t)i;
            e->period  = s_prev_period[i];
            e->volume  = s_prev_volume[i];
            s_dirty[i] = 0;
        }
    }
    s_capture_tick++;
}

int paula_capture_count(void) { return s_capture_count; }
const PaulaNoteEvent* paula_capture_buffer(void) { return s_capture; }
