/* BS.1770-4 integrated loudness: K-weighting (high shelf + RLB high pass), 400 ms blocks every 100 ms,
 * absolute gate -70 LUFS, relative gate -10 LU. Coefficients are derived from the analogue prototypes so
 * any sample rate works (same derivation as libebur128). */
#include "loudness.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* M_PI is not in strict -std=c99 on glibc. */
#define LOUD_PI 3.14159265358979323846

typedef struct biquad {
    double b0, b1, b2, a1, a2;
    double z1, z2; /* transposed direct form II state */
} biquad;

struct sel_loudness {
    biquad shelf[2], hp[2];
    uint32_t sub_len;  /* samples per 100 ms sub-block */
    uint32_t sub_fill;
    double sub_sum;    /* sum over both channels of K-weighted squares in the current sub-block */
    double ring[4];    /* last four sub-block sums */
    uint32_t ring_n;
    double* blocks;    /* mean-square energy of every 400 ms block */
    uint32_t nblocks, cap;
    double sq_sum;     /* unweighted squares, both channels, for the fallback */
    double peak;
    uint64_t frames;
};

static void make_shelf(biquad* b, double fs) {
    const double f0 = 1681.974450955533, g = 3.999843853973347, q = 0.7071752369554196;
    double k = tan(LOUD_PI * f0 / fs);
    double vh = pow(10.0, g / 20.0);
    double vb = pow(vh, 0.4996667741545416);
    double a0 = 1.0 + k / q + k * k;
    memset(b, 0, sizeof *b);
    b->b0 = (vh + vb * k / q + k * k) / a0;
    b->b1 = 2.0 * (k * k - vh) / a0;
    b->b2 = (vh - vb * k / q + k * k) / a0;
    b->a1 = 2.0 * (k * k - 1.0) / a0;
    b->a2 = (1.0 - k / q + k * k) / a0;
}

static void make_hp(biquad* b, double fs) {
    const double f0 = 38.13547087602444, q = 0.5003270373238773;
    double k = tan(LOUD_PI * f0 / fs);
    double a0 = 1.0 + k / q + k * k;
    memset(b, 0, sizeof *b);
    b->b0 = 1.0;
    b->b1 = -2.0;
    b->b2 = 1.0;
    b->a1 = 2.0 * (k * k - 1.0) / a0;
    b->a2 = (1.0 - k / q + k * k) / a0;
}

static double run(biquad* b, double x) {
    double y = b->b0 * x + b->z1;
    b->z1 = b->b1 * x - b->a1 * y + b->z2;
    b->z2 = b->b2 * x - b->a2 * y;
    return y;
}

sel_loudness* sel_loudness_new(uint32_t rate) {
    sel_loudness* l;
    int c;

    if (rate < 8000u) return NULL;
    l = (sel_loudness*)calloc(1, sizeof *l);
    if (l == NULL) return NULL;
    for (c = 0; c < 2; c++) {
        make_shelf(&l->shelf[c], (double)rate);
        make_hp(&l->hp[c], (double)rate);
    }
    l->sub_len = (rate + 5u) / 10u;
    return l;
}

void sel_loudness_free(sel_loudness* l) {
    if (l == NULL) return;
    free(l->blocks);
    free(l);
}

static void sub_done(sel_loudness* l) {
    uint32_t i;
    double e = 0.0;

    if (l->ring_n < 4u) {
        l->ring[l->ring_n++] = l->sub_sum;
    } else {
        memmove(l->ring, l->ring + 1, 3u * sizeof l->ring[0]);
        l->ring[3] = l->sub_sum;
    }
    l->sub_sum = 0.0;
    l->sub_fill = 0;
    if (l->ring_n < 4u) return;
    for (i = 0; i < 4u; i++) e += l->ring[i];
    e /= (double)(4u * l->sub_len);
    if (l->nblocks == l->cap) {
        uint32_t ncap = l->cap != 0 ? l->cap * 2u : 1024u;
        double* nb = (double*)realloc(l->blocks, (size_t)ncap * sizeof *nb);
        if (nb == NULL) return;
        l->blocks = nb;
        l->cap = ncap;
    }
    l->blocks[l->nblocks++] = e;
}

void sel_loudness_add_f64(sel_loudness* l, const double* stereo, uint32_t frames) {
    uint32_t i;

    for (i = 0; i < frames; i++) {
        double x0 = stereo[2u * i], x1 = stereo[2u * i + 1u];
        double k0 = run(&l->hp[0], run(&l->shelf[0], x0));
        double k1 = run(&l->hp[1], run(&l->shelf[1], x1));
        double a0 = fabs(x0), a1 = fabs(x1);

        l->sub_sum += k0 * k0 + k1 * k1;
        l->sq_sum += x0 * x0 + x1 * x1;
        if (a0 > l->peak) l->peak = a0;
        if (a1 > l->peak) l->peak = a1;
        if (++l->sub_fill == l->sub_len) sub_done(l);
    }
    l->frames += frames;
}

void sel_loudness_add_s16(sel_loudness* l, const int16_t* stereo, uint32_t frames) {
    double buf[512];
    uint32_t done = 0;

    while (done < frames) {
        uint32_t n = frames - done > 256u ? 256u : frames - done;
        uint32_t i;
        for (i = 0; i < 2u * n; i++) buf[i] = (double)stereo[2u * done + i] / 32768.0;
        sel_loudness_add_f64(l, buf, n);
        done += n;
    }
}

static double to_lufs(double energy) { return -0.691 + 10.0 * log10(energy); }

void sel_loudness_result_get(sel_loudness* l, sel_loudness_result* out) {
    double abs_gate = pow(10.0, (-70.0 + 0.691) / 10.0);
    double sum = 0.0, rel;
    uint32_t n = 0, i;

    memset(out, 0, sizeof *out);
    out->frames = l->frames;
    out->fallback_db = l->frames > 0 && l->sq_sum > 0.0 ? 10.0 * log10(l->sq_sum / (double)l->frames) : -120.0;
    out->peak_db = l->peak > 0.0 ? 20.0 * log10(l->peak) : -120.0;
    for (i = 0; i < l->nblocks; i++) {
        if (l->blocks[i] > abs_gate) {
            sum += l->blocks[i];
            n++;
        }
    }
    if (n > 0) {
        double lsum = 0.0;
        uint32_t m = 0;
        rel = pow(10.0, (to_lufs(sum / (double)n) - 10.0 + 0.691) / 10.0);
        for (i = 0; i < l->nblocks; i++) {
            if (l->blocks[i] > abs_gate && l->blocks[i] > rel) {
                lsum += l->blocks[i];
                m++;
            }
        }
        if (m > 0) {
            out->gated = 1;
            out->lufs = to_lufs(lsum / (double)m);
        }
    }
    if (!out->gated) {
        /* Fallback: RMS of all channels (equals LUFS for a 1 kHz sine, within the K-weight gain). */
        out->lufs = out->fallback_db;
    }
    if (out->lufs > -70.0) {
        double g = SEL_LOUDNESS_TARGET_LUFS - out->lufs;
        if (g > 24.0) g = 24.0;
        if (g < -24.0) g = -24.0;
        out->gain_db = g;
    }
    out->gain_cdb = (int32_t)floor(out->gain_db * 100.0 + 0.5);
}
