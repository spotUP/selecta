/* Visualisation taps (design doc section 8, plan M1.3 Phase 1).
 * Audio thread: block bookkeeping, the post-mix ring, the triple buffer
 * writer. Pump thread: the triple buffer reader, correlation and spectrum.
 * Integer only, no locks; the only allocation is the visual delay queue (I1042). */
#include <selecta/viz.h>

#include <stdlib.h>
#include <string.h>

#include "viz_table.h"

/* The triple buffer swaps one word. On the 68k (single core, no compare and
 * swap before the 68020) a plain volatile store is enough, as in spsc.c; the
 * audio and pump threads there never run on two cores. Elsewhere the swap is
 * an atomic exchange. */
static uint32_t swap_middle(volatile uint32_t* p, uint32_t v) {
#if defined(__m68k__) || !defined(__GNUC__)
    uint32_t old = *p;
    *p = v;
    return old;
#else
    return __atomic_exchange_n(p, v, __ATOMIC_ACQ_REL);
#endif
}

void sel_viz_tap_init(sel_viz_tap* t) {
    memset(t, 0, sizeof *t);
    t->write_idx = 0;
    t->middle = 1;
    t->read_idx = 2;
}

uint32_t sel_viz_block_left(sel_viz_tap* t, uint32_t rate) {
    if (t->block_left == 0) {
        uint32_t n = rate / SEL_VIZ_BLOCKS_PER_SECOND;
        t->block_left = n ? n : 1;
    }
    return t->block_left;
}

int sel_viz_pcm_push(sel_viz_tap* t, const int16_t* stereo, uint32_t frames) {
    uint32_t done = 0;
    if (frames > t->block_left) frames = t->block_left;
    while (done < frames) {
        uint32_t n = frames - done;
        if (n > SEL_VIZ_PCM_FRAMES - t->pcm_head) n = SEL_VIZ_PCM_FRAMES - t->pcm_head;
        memcpy(&t->pcm[t->pcm_head * 2u], &stereo[done * 2u], (size_t)n * 2u * sizeof stereo[0]);
        t->pcm_head = (t->pcm_head + n) & (SEL_VIZ_PCM_FRAMES - 1u);
        done += n;
    }
    t->block_left -= frames;
    return t->block_left == 0;
}

sel_viz_frame* sel_viz_begin(sel_viz_tap* t, uint32_t rate) {
    sel_viz_frame* f = &t->frames[t->write_idx];
    uint32_t first = SEL_VIZ_PCM_FRAMES - t->pcm_head; /* frames from the oldest to the ring end */
    memset(f, 0, sizeof *f);
    f->seq = ++t->seq;
    f->rate = rate;
    memcpy(f->pcm, &t->pcm[t->pcm_head * 2u], (size_t)first * 2u * sizeof f->pcm[0]);
    memcpy(&f->pcm[first * 2u], t->pcm, (size_t)t->pcm_head * 2u * sizeof f->pcm[0]);
    return f;
}

void sel_viz_publish(sel_viz_tap* t) {
    uint32_t old = swap_middle(&t->middle, t->write_idx | 4u);
    t->write_idx = old & 3u;
}

int16_t sel_viz_vu_step(int16_t level, int16_t peak) {
    int32_t fall;
    if (peak > level) return peak;
    fall = level >> 3;
    if (fall < 1) fall = 1;
    return (int16_t)(level > fall ? level - fall : 0);
}

/* Control-register bit b of voice v is flag [v][b]; flag [v][8] is the filter routing bit v of $17. */
static uint32_t sid_flag(const uint8_t* reg, uint32_t v, uint32_t b) {
    return b < 8u ? (uint32_t)((reg[4u + 7u * v] >> b) & 1u) : (uint32_t)((reg[0x17] >> v) & 1u);
}

static uint8_t glow_step(uint32_t was_on, uint32_t on, uint8_t counter) {
    if (on) return 0;
    if (was_on) return 4;
    return counter > 0u ? (uint8_t)(counter - 1u) : 0;
}

void sel_viz_sid_glow(sel_viz_sid_chip* now, const uint8_t prev_reg[SEL_VIZ_SID_REGS], const sel_viz_sid_chip* prev) {
    uint32_t v, b;
    for (v = 0; v < 3u; v++) {
        for (b = 0; b < 9u; b++) {
            now->glow[v][b] = glow_step(prev_reg ? sid_flag(prev_reg, v, b) : 0u, sid_flag(now->reg, v, b),
                                        prev ? prev->glow[v][b] : 0);
        }
        /* mode bits LP, BP, HP are $18 bits 4..6 */
        now->mode_glow[v] = glow_step(prev_reg ? (uint32_t)((prev_reg[0x18] >> (4u + v)) & 1u) : 0u,
                                      (uint32_t)((now->reg[0x18] >> (4u + v)) & 1u), prev ? prev->mode_glow[v] : 0);
    }
}

uint32_t sel_viz_set_delay(sel_viz_tap* t, uint32_t blocks) {
    if (blocks > SEL_VIZ_MAX_DELAY_BLOCKS) blocks = SEL_VIZ_MAX_DELAY_BLOCKS;
    if (blocks == t->delay) return t->delay;
    free(t->queue);
    t->queue = NULL;
    t->q_head = t->q_count = 0;
    t->delay = 0;
    if (blocks > 0) {
        t->queue = (sel_viz_frame*)calloc((size_t)blocks + 2u, sizeof(sel_viz_frame));
        if (t->queue != NULL) t->delay = blocks;
    }
    return t->delay;
}

uint32_t sel_viz_delay(const sel_viz_tap* t) {
    return t->delay;
}

void sel_viz_tap_free(sel_viz_tap* t) {
    free(t->queue);
    t->queue = NULL;
    t->delay = 0;
}

/* The frame the UI sees: the newest one, or with a visual delay the held-back one (I1042). */
static sel_viz_frame* shown_frame(sel_viz_tap* t) {
    return t->delay != 0 ? &t->queue[t->delay + 1u] : &t->frames[t->read_idx];
}

int sel_viz_acquire(sel_viz_tap* t) {
    uint32_t old;
    sel_viz_frame* shown;
    if (!(t->middle & 4u)) return 0;
    old = swap_middle(&t->middle, t->read_idx);
    t->read_idx = old & 3u;
    if (t->delay == 0) return 1;
    /* Delayed: queue the new frame; the oldest one is shown once more than `delay` blocks are held. */
    shown = &t->queue[t->delay + 1u];
    if (t->q_count == t->delay + 1u) { /* full: drop the oldest */
        t->q_head = (t->q_head + 1u) % (t->delay + 1u);
        t->q_count--;
    }
    t->queue[(t->q_head + t->q_count) % (t->delay + 1u)] = t->frames[t->read_idx];
    t->q_count++;
    if (t->q_count <= t->delay) return 0;
    *shown = t->queue[t->q_head];
    t->q_head = (t->q_head + 1u) % (t->delay + 1u);
    t->q_count--;
    return 1;
}

const sel_viz_frame* sel_viz_current(const sel_viz_tap* t) {
    return shown_frame((sel_viz_tap*)t);
}

/* Integer square root, rounded down. */
static uint64_t isqrt64(uint64_t x) {
    uint64_t r = 0;
    uint64_t bit = (uint64_t)1 << 62;
    while (bit > x) bit >>= 2;
    while (bit) {
        if (x >= r + bit) {
            x -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

int32_t sel_viz_correlation(const int16_t* stereo, uint32_t frames) {
    int64_t ll = 0, lr = 0, rr = 0;
    uint64_t den;
    int64_t c;
    uint32_t i;
    for (i = 0; i < frames; i++) {
        int32_t l = stereo[i * 2u], r = stereo[i * 2u + 1u];
        ll += (int64_t)l * l;
        lr += (int64_t)l * r;
        rr += (int64_t)r * r;
    }
    /* Scale down so ll * rr fits 64 bits: 1024 frames of 2^30 are 2^40; divide by 1024. */
    ll /= 1024;
    lr /= 1024; /* rounds toward zero, symmetric for both signs */
    rr /= 1024;
    den = isqrt64((uint64_t)ll * (uint64_t)rr);
    if (den == 0) return 0;
    c = lr * 32767 / (int64_t)den;
    if (c > 32767) c = 32767;
    if (c < -32767) c = -32767;
    return (int32_t)c;
}

#define FFT_N 256u

/* FFT bin nearest to f Hz at the given rate. */
static uint32_t bin_of(uint32_t hz, uint32_t rate) {
    return (uint32_t)(((uint64_t)hz * FFT_N + rate / 2u) / rate);
}

/* 256-point radix-2 decimation-in-time FFT in Q15 with a right shift per
 * stage, so the result is the true transform divided by 256 and cannot
 * overflow 32 bits. */
static void fft256(int32_t* re, int32_t* im) {
    uint32_t i, j, len, k, step;
    for (i = 1, j = 0; i < FFT_N; i++) {
        uint32_t bit = FFT_N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            int32_t t = re[i];
            re[i] = re[j];
            re[j] = t;
            t = im[i];
            im[i] = im[j];
            im[j] = t;
        }
    }
    for (len = 2; len <= FFT_N; len <<= 1) {
        step = FFT_N / len;
        for (i = 0; i < FFT_N; i += len) {
            for (k = 0; k < len / 2u; k++) {
                int32_t wr = sel_viz_cos[k * step], wi = sel_viz_sin[k * step];
                uint32_t a = i + k, b = i + k + len / 2u;
                int32_t tr = (int32_t)(((int64_t)re[b] * wr - (int64_t)im[b] * wi) >> 15);
                int32_t ti = (int32_t)(((int64_t)re[b] * wi + (int64_t)im[b] * wr) >> 15);
                int32_t ur = re[a], ui = im[a];
                re[a] = (ur + tr) >> 1;
                im[a] = (ui + ti) >> 1;
                re[b] = (ur - tr) >> 1;
                im[b] = (ui - ti) >> 1;
            }
        }
    }
}

/* I292b: the whole ring is analysed, not its newest 256 frames. A 20 ms block is 882 frames; one window saw 29 percent
 * of it, so a bar jumped with whichever part of the block fell into the window. The ring is cut into consecutive 256
 * frame windows (1024 frames: four, the newest block and a little older sound) and the bin magnitudes are averaged. */
void sel_viz_spectrum(const int16_t* stereo, uint32_t frames, uint32_t rate, int16_t band[SEL_VIZ_BANDS]) {
    int32_t re[FFT_N], im[FFT_N];
    uint32_t sum[FFT_N / 2u];
    int16_t mag[FFT_N / 2u];
    uint32_t i, b, w, windows;
    memset(band, 0, SEL_VIZ_BANDS * sizeof band[0]);
    if (frames < FFT_N || rate == 0) return;
    windows = frames / FFT_N;
    stereo += (size_t)(frames - windows * FFT_N) * 2u; /* the windows end at the newest frame */
    memset(sum, 0, sizeof sum);
    for (w = 0; w < windows; w++) {
        const int16_t* src = stereo + (size_t)w * FFT_N * 2u;
        for (i = 0; i < FFT_N; i++) {
            int32_t mono = ((int32_t)src[i * 2u] + src[i * 2u + 1u]) >> 1;
            re[i] = (mono * sel_viz_hann[i]) >> 15;
            im[i] = 0;
        }
        fft256(re, im);
        for (i = 0; i < FFT_N / 2u; i++) {
            uint64_t m = isqrt64((uint64_t)((int64_t)re[i] * re[i] + (int64_t)im[i] * im[i])) * 4u;
            sum[i] += (uint32_t)(m > 32767u ? 32767u : m);
        }
    }
    for (i = 0; i < FFT_N / 2u; i++) mag[i] = (int16_t)(sum[i] / windows);
    for (b = 0; b < SEL_VIZ_BANDS; b++) {
        uint32_t lo = bin_of(sel_viz_edge_hz[b], rate), hi = bin_of(sel_viz_edge_hz[b + 1u], rate);
        int16_t peak = 0;
        if (lo < 1) lo = 1;
        if (hi < lo + 1u) hi = lo + 1u;
        if (hi > FFT_N / 2u) hi = FFT_N / 2u;
        for (i = lo; i < hi; i++) {
            if (mag[i] > peak) peak = mag[i];
        }
        band[b] = peak;
    }
}

/* I292b: peak falloff, the one place both UIs get it from (they draw f->band). Rises at once, falls linearly. */
int sel_viz_falloff(int16_t level[], const int16_t raw[], uint32_t n, uint32_t dt_ms) {
    uint32_t i, dec;
    if (dt_ms > 2000u) dt_ms = 2000u;
    dec = (SEL_VIZ_FALL_PER_SECOND * dt_ms) / 1000u;
    for (i = 0; i < n; i++) {
        int32_t v = (int32_t)level[i] - (int32_t)dec;
        if (raw[i] >= v) v = raw[i];
        if (v < 0) v = 0;
        level[i] = (int16_t)v;
    }
    return 1; /* the step ran: the caller counts it for the sentinel */
}

void sel_viz_analyse(sel_viz_tap* t, int with_spectrum) {
    sel_viz_frame* f = shown_frame(t); /* the shown frame, not the newest: with a visual delay they differ (I1042) */
    if (f->seq == 0) return;
    f->correlation = (int16_t)sel_viz_correlation(f->pcm, SEL_VIZ_PCM_FRAMES);
    if (with_spectrum) {
        sel_viz_spectrum(f->pcm, SEL_VIZ_PCM_FRAMES, f->rate, f->band);
        /* Time between shown frames is blocks times 20 ms, so the fall does not depend on how often the UI polls. */
        if (t->smooth_seq == 0u || f->seq < t->smooth_seq) {
            memcpy(t->smooth, f->band, sizeof t->smooth);
        } else {
            t->falloff_runs += (uint32_t)sel_viz_falloff(t->smooth, f->band, SEL_VIZ_BANDS,
                                                         (f->seq - t->smooth_seq) * (1000u / SEL_VIZ_BLOCKS_PER_SECOND));
        }
        t->smooth_seq = f->seq;
        memcpy(f->band, t->smooth, sizeof t->smooth);
        f->falloff_runs = t->falloff_runs;
        f->spectrum_valid = 1;
    }
}
