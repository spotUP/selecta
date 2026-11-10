/* I1246a/b: Krakatoa onset-to-visible latency and kick displacement, measured on the GPU image.
 *
 * The audio goes through the real engine tap (sel_viz_pcm_push / begin / publish / acquire / analyse, src/engine/viz.c)
 * at one UI frame per 20 ms block, then through the plugin's own chain: krk_audio_step, krk_brush_at,
 * krk_uniforms_build, krk_gpu_step (SDL3 GPU, synchronous readback of the same step). The emitter is fixed (auto 0) so
 * the plume geometry only moves with the music. Per frame the image gives the smoke's luminance-weighted spread (RMS
 * distance from its centroid, in pixels of a 640x400 output).
 *
 *   krk_latency_test synth        regression: a 120 BPM 55 Hz kick train over a quiet bed. Asserts every kick is
 *                                 detected, the spread rises by >= KICK_MIN_PX within the kick window, and half the
 *                                 rise is visible within LAT_BUDGET_MS of the audio onset (block audio time).
 *   krk_latency_test wav <file>   measurement: same chain on a 16-bit stereo WAV (a real song rendered with
 *                                 selecta-cli render); onsets are the mapper's own kick detections. Prints the table.
 * Prints SELECTA_SKIP when no GPU device comes up. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <selecta/viz.h>

#include "core/krk_core.h"
#include "sdl/krk_smoke_gpu.h"

#define OW 640u
#define OH 400u
#define RATE 44100u
#define KICK_MIN_PX 6.0   /* a kick must widen the plume by at least this (640x400 output) */
#define LAT_BUDGET_MS 60  /* onset to half the visible rise, against the audio clock */
#define WARM_S 3.0
#define MAX_KICKS 512

static uint32_t img[OW * OH];

static double spread_px(void) {
    double bg = 0.0, sw = 0.0, sx = 0.0, sy = 0.0, s2 = 0.0, cx, cy;
    uint32_t x, y;
    for (x = 0; x < OW; x++) { /* the top row is background */
        uint32_t p = img[x];
        bg += (double)(((p >> 16) & 255u) + ((p >> 8) & 255u) + (p & 255u)) / 3.0;
    }
    bg /= OW;
    for (y = 0; y < OH; y++) {
        for (x = 0; x < OW; x++) {
            uint32_t p = img[y * OW + x];
            double l = (double)(((p >> 16) & 255u) + ((p >> 8) & 255u) + (p & 255u)) / 3.0 - bg - 4.0;
            if (l > 0.0) {
                sw += l;
                sx += l * x;
                sy += l * y;
            }
        }
    }
    if (sw <= 0.0) return 0.0;
    cx = sx / sw;
    cy = sy / sw;
    for (y = 0; y < OH; y++) {
        for (x = 0; x < OW; x++) {
            uint32_t p = img[y * OW + x];
            double l = (double)(((p >> 16) & 255u) + ((p >> 8) & 255u) + (p & 255u)) / 3.0 - bg - 4.0;
            if (l > 0.0) s2 += l * ((x - cx) * (x - cx) + (y - cy) * (y - cy));
        }
    }
    return sqrt(s2 / sw);
}

static int16_t* load_wav(const char* path, uint32_t* frames) {
    FILE* fp = fopen(path, "rb");
    unsigned char h[12], ch[8];
    int16_t* pcm = NULL;
    if (fp == NULL || fread(h, 1, 12, fp) != 12) goto out;
    while (fread(ch, 1, 8, fp) == 8) {
        uint32_t n = ch[4] | ch[5] << 8 | ch[6] << 16 | (uint32_t)ch[7] << 24;
        if (memcmp(ch, "data", 4) == 0) {
            pcm = (int16_t*)malloc(n);
            *frames = (uint32_t)(fread(pcm, 1, n, fp) / 4u);
            break;
        }
        fseek(fp, (long)(n + (n & 1u)), SEEK_CUR);
    }
out:
    if (fp) fclose(fp);
    return pcm;
}

static int16_t* synth(uint32_t* frames, double seconds, double* onsets, uint32_t* n_on) {
    uint32_t n = (uint32_t)(seconds * RATE), i;
    int16_t* pcm = (int16_t*)malloc((size_t)n * 4u);
    uint32_t seed = 1u;
    *n_on = 0;
    for (i = 0; i < n; i++) {
        double t = (double)i / RATE, beat = fmod(t, 0.5), k = 0.0, v;
        int32_t s;
        if (t >= 1.0) { /* first kick at 1.0 s; the onset sits mid block on purpose (block edges at k * 20 ms) */
            k = 0.85 * exp(-beat / 0.15) * sin(2.0 * M_PI * (55.0 * beat + 40.0 * 0.03 * (1.0 - exp(-beat / 0.03))));
        }
        seed = seed * 1664525u + 1013904223u;
        v = k + 0.02 * ((double)(seed >> 9) / 4194304.0 - 1.0) + 0.05 * sin(2.0 * M_PI * 3000.0 * t);
        s = (int32_t)(v * 32767.0);
        pcm[i * 2u] = pcm[i * 2u + 1u] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
    }
    for (i = 0; i < MAX_KICKS; i++) {
        double t = 1.0 + 0.5 * i + 0.0101; /* 10.1 ms into its block */
        if (t > seconds - 0.4) break;
        onsets[(*n_on)++] = t;
    }
    /* shift the synthetic hits so they start 10.1 ms into a block */
    memmove(pcm + (size_t)(0.0101 * RATE) * 2u, pcm, ((size_t)n - (size_t)(0.0101 * RATE)) * 4u);
    memset(pcm, 0, (size_t)(0.0101 * RATE) * 4u);
    *frames = n;
    return pcm;
}

int main(int argc, char** argv) {
    int is_synth = argc > 1 && strcmp(argv[1], "synth") == 0;
    static double onset[MAX_KICKS], spread[30000];
    static uint32_t beat_frame[MAX_KICKS];
    uint32_t n_on = 0, frames = 0, blk = RATE / SEL_VIZ_BLOCKS_PER_SECOND, nf, i, pos = 0, last_beats = 0, n_beat = 0;
    int16_t* pcm;
    sel_viz_tap* tap = (sel_viz_tap*)calloc(1, sizeof *tap);
    krk_grid g = krk_grid_for(KRK_Q_HIGH, OW, OH);
    krk_params p;
    krk_audio a;
    krk_gpu* gpu;
    float t = 0.0f, dt = 1.0f / SEL_VIZ_BLOCKS_PER_SECOND;
    int fails = 0;
    double lat_sum = 0.0, lat_max = 0.0, disp_sum = 0.0, disp_min = 1e9, pump_peak_ms = 0.0;
    uint32_t measured = 0;
    double gpu_ms = 0.0;
    if (argc < 2 || (!is_synth && argc < 3)) {
        fprintf(stderr, "usage: krk_latency_test synth | wav <file>\n");
        return 2;
    }
    pcm = is_synth ? synth(&frames, 12.0, onset, &n_on) : load_wav(argv[2], &frames);
    if (pcm == NULL) {
        fprintf(stderr, "no pcm\n");
        return 2;
    }
    gpu = krk_gpu_create(&g, OW, OH);
    if (gpu == NULL) {
        printf("SELECTA_SKIP: no GPU device for Krakatoa\n");
        return 0;
    }
    krk_params_default(&p);
    if (getenv("KRK_LAT_AUTO") == NULL) krk_params_set(&p, KRK_K_AUTO, 0.0f);
    krk_audio_init(&a);
    sel_viz_tap_init(tap);
    nf = frames / blk;
    if (nf > 30000u) nf = 30000u;
    for (i = 0; i < nf; i++, pos += blk) {
        krk_uniforms u;
        krk_brush b;
        const sel_viz_frame* f;
        (void)sel_viz_block_left(tap, RATE);
        if (sel_viz_pcm_push(tap, pcm + (size_t)pos * 2u, blk)) {
            sel_viz_begin(tap, RATE);
            sel_viz_publish(tap);
        }
        if (sel_viz_acquire(tap)) sel_viz_analyse(tap, 1);
        f = sel_viz_current(tap);
        t += dt;
        krk_audio_step(&a, f, &p, dt);
        krk_brush_at(&b, t, &g, &p, &a);
        krk_uniforms_build(&u, &g, &p, &a, &b, dt, t, OW / 2u, OH / 2u, i);
        {
            struct timespec t0, t1;
            uint32_t ok;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            ok = krk_gpu_step(gpu, &u, g.jacobi, img);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            gpu_ms += (double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
            if (ok == 0u) {
                fprintf(stderr, "gpu step failed\n");
                return 1;
            }
        }
        spread[i] = spread_px();
        if (getenv("KRK_LAT_TRACE") != NULL && i >= 148u && i <= 175u) {
            static uint32_t prev[OW * OH];
            double md = 0.0;
            uint32_t q;
            for (q = 0; q < OW * OH; q++) md += abs((int)(img[q] & 255u) - (int)(prev[q] & 255u));
            memcpy(prev, img, sizeof prev);
            {
                uint32_t lit = 0, col[8] = {0}, x, y;
                for (y = 0; y < OH; y++)
                    for (x = 0; x < OW; x++)
                        if ((img[y * OW + x] & 255u) > 40u) {
                            lit++;
                            col[x * 8u / OW]++;
                        }
                fprintf(stderr, "lit %.3f cols %u %u %u %u %u %u %u %u top %u mid %u ", (double)lit / (OW * OH), col[0], col[1],
                        col[2], col[3], col[4], col[5], col[6], col[7], img[5] & 255u, img[OH / 2 * OW + OW / 2] & 255u);
            }
            fprintf(stderr, "diff %.3f ", md / (OW * OH));
            fprintf(stderr, "f %u pump %.3f beat_s %.3f r %.2f bp %.1f %.1f %.1f grid %ux%ux%u spread %.2f\n", i, a.pump,
                    a.beat_s, b.radius, b.pos[0], b.pos[1], b.pos[2], g.x, g.y, g.z, spread[i]);
        }
        if (a.beats != last_beats && n_beat < MAX_KICKS) {
            beat_frame[n_beat++] = i;
            last_beats = a.beats;
        }
    }
    krk_gpu_destroy(gpu);
    if (!is_synth) { /* real song: the mapper's detections stand in for the onsets (block start = earliest onset) */
        for (i = 0; i < n_beat; i++) onset[i] = (double)beat_frame[i] * blk / RATE;
        n_on = n_beat;
    }
    printf("kick  onset_ms  detect_ms  visible_ms  rise_px\n");
    for (i = 0; i < n_on; i++) {
        /* frame i holds audio up to (i + 1) blocks; the onset lives in frame floor(onset * 50) */
        uint32_t f0 = (uint32_t)(onset[i] * SEL_VIZ_BLOCKS_PER_SECOND), fdet = 0, j, peak_f = 0, vis_f = 0;
        double base, slope, peak = -1e9, end_ms;
        int found = 0;
        if (f0 < 4u || f0 + 14u >= nf || onset[i] < WARM_S) continue;
        for (j = 0; j < n_beat; j++) {
            if (beat_frame[j] >= f0 && beat_frame[j] <= f0 + 3u) {
                fdet = beat_frame[j];
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("%4u  %8.1f  missed\n", i, onset[i] * 1000.0);
            if (is_synth) fails++;
            continue;
        }
        /* the plume keeps growing between kicks: measure against the pre-kick trend, extrapolated */
        slope = (spread[f0 - 1u] - spread[f0 - 4u]) / 3.0;
        base = spread[f0 - 1u];
        for (j = f0; j <= f0 + 12u; j++) {
            double rise = spread[j] - base - slope * (double)(j - f0 + 1u);
            if (rise > peak) {
                peak = rise;
                peak_f = j;
            }
        }
        vis_f = peak_f;
        for (j = f0; j <= peak_f; j++) {
            if (spread[j] - base - slope * (double)(j - f0 + 1u) >= 0.5 * peak) {
                vis_f = j;
                break;
            }
        }
        end_ms = (double)(vis_f + 1u) * 1000.0 / SEL_VIZ_BLOCKS_PER_SECOND - onset[i] * 1000.0;
        printf("%4u  %8.1f  %9.1f  %10.1f  %7.2f\n", i, onset[i] * 1000.0,
               (double)(fdet + 1u) * 20.0 - onset[i] * 1000.0, end_ms, peak);
        lat_sum += end_ms;
        lat_max = end_ms > lat_max ? end_ms : lat_max;
        disp_sum += peak;
        disp_min = peak < disp_min ? peak : disp_min;
        pump_peak_ms += (double)(peak_f + 1u) * 20.0 - onset[i] * 1000.0;
        measured++;
    }
    if (measured == 0u) {
        printf("no kicks measured\n");
        return 1;
    }
    printf("krk_latency kicks %u visible_ms mean %.1f max %.1f peak_ms mean %.1f rise_px mean %.2f min %.2f "
           "gpu_step_ms mean %.2f (budget %d ms, min %.1f px)\n",
           measured, lat_sum / measured, lat_max, pump_peak_ms / measured, disp_sum / measured, disp_min,
           gpu_ms / nf, LAT_BUDGET_MS, KICK_MIN_PX);
    if (is_synth) {
        if (lat_max > LAT_BUDGET_MS) {
            printf("FAIL: onset to visible %.1f ms over the %d ms budget\n", lat_max, LAT_BUDGET_MS);
            fails++;
        }
        if (disp_min < KICK_MIN_PX) {
            printf("FAIL: a kick widens the plume by %.2f px, want >= %.1f\n", disp_min, KICK_MIN_PX);
            fails++;
        }
        if (fails == 0) printf("krk_latency_ok\n");
    }
    free(pcm);
    free(tap);
    return fails != 0;
}
