/* T15 nuade-ab: render the test song through stock nuade (its own float
 * mixer, host only) and compare 20 ms RMS envelopes of the mono sum with
 * the WAV that `selecta-cli render --resample linear` wrote. Method of
 * DEViLBOX tools/nuade/nuadeCompare.ts (envelope20, Pearson correlation),
 * at lag 0 because both sides start the song at frame 0.
 * usage: nuade_ab <song> <selecta.wav>   prints `windows=W correlation=C`.
 * Exit 0 when W == 500 and C >= 0.98. Test code: float and double allowed. */
#include <nuade/nuade.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RATE 44100u
#define SECONDS 10u
#define WIN 882u /* 20 ms at 44100 */
#define THRESHOLD 0.98

static size_t envelope(const double* mono, size_t frames, double* out) {
    size_t k, i, n = frames / WIN;
    for (k = 0; k < n; k++) {
        double s = 0.0;
        for (i = k * WIN; i < (k + 1) * WIN; i++) {
            s += mono[i] * mono[i];
        }
        out[k] = sqrt(s / WIN);
    }
    return n;
}

static double pearson(const double* a, const double* b, size_t n) {
    double ma = 0, mb = 0, saa = 0, sbb = 0, sab = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        ma += a[i];
        mb += b[i];
    }
    ma /= (double)n;
    mb /= (double)n;
    for (i = 0; i < n; i++) {
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
        sab += (a[i] - ma) * (b[i] - mb);
    }
    return (saa > 0 && sbb > 0) ? sab / sqrt(saa * sbb) : NAN;
}

static void* slurp(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    void* buf;
    long n;
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return buf;
}

int main(int argc, char** argv) {
    const size_t frames = (size_t)RATE * SECONDS;
    NuadeSongResult r;
    float* ref;
    double *mono_a, *mono_b, *env_a, *env_b;
    uint8_t* wav;
    size_t wav_size, i, w1, w2;
    double c;

    if (argc != 3) {
        fprintf(stderr, "usage: nuade_ab <song> <selecta.wav>\n");
        return 2;
    }
    r = nuade_open(argv[1]);
    if (!r.song || !nuade_start(r.song, 0, RATE)) {
        fprintf(stderr, "nuade_ab: cannot open or start %s\n", argv[1]);
        return 2;
    }
    ref = (float*)calloc(frames * 2, sizeof(float));
    mono_a = (double*)malloc(frames * sizeof(double));
    mono_b = (double*)malloc(frames * sizeof(double));
    env_a = (double*)malloc(frames / WIN * sizeof(double));
    env_b = (double*)malloc(frames / WIN * sizeof(double));
    wav = (uint8_t*)slurp(argv[2], &wav_size);
    if (!ref || !mono_a || !mono_b || !env_a || !env_b || !wav || wav_size < 44 + frames * 4) {
        fprintf(stderr, "nuade_ab: cannot read %s (or out of memory)\n", argv[2]);
        return 2;
    }
    {
        NuadeRenderResult rr = nuade_render(r.song, ref, NULL, (uint32_t)frames);
        if (rr.frames != frames) {
            fprintf(stderr, "nuade_ab: nuade rendered %u of %zu frames\n", rr.frames, frames);
            return 2;
        }
    }
    for (i = 0; i < frames; i++) {
        int16_t l = (int16_t)(wav[44 + 4 * i] | (wav[45 + 4 * i] << 8));
        int16_t rt = (int16_t)(wav[46 + 4 * i] | (wav[47 + 4 * i] << 8));
        mono_a[i] = (double)ref[2 * i] + (double)ref[2 * i + 1];
        mono_b[i] = ((double)l + (double)rt) / 32768.0;
    }
    w1 = envelope(mono_a, frames, env_a);
    w2 = envelope(mono_b, frames, env_b);
    c = pearson(env_a, env_b, w1 < w2 ? w1 : w2);
    printf("windows=%zu correlation=%.4f\n", w1 < w2 ? w1 : w2, c);
    nuade_close(r.song);
    return (w1 == 500 && w2 == 500 && c >= THRESHOLD) ? 0 : 1;
}
