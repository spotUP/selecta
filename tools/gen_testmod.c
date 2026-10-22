/* gen_testmod: writes selecta-test.mod, the module the render tests play
 * (plan Phase 3, T13). Host build tool, never part of the product.
 *
 * 4 channels, 4 patterns of 64 rows, 4 samples synthesised with integer
 * arithmetic only (square, saw, LFSR noise, decaying click). Effects used:
 * 0xx arpeggio, 1xx slide up, 2xx slide down, 3xx tone portamento,
 * 4xx vibrato, Axx volume slide, Cxx set volume, Fxx speed. Tempo stays at
 * the default 125 BPM, so 50 ticks per second. The output is byte for byte
 * reproducible; tests/songs/selecta-test.mod.sha256 pins it.
 *
 * The file is what the protracker4 replayer reads: an IFF form (FORM MODL)
 * with the chunks VERS, INFO (title, start tempo 125, flags 0) and PTDT, the
 * latter holding an ordinary 31 sample ProTracker module followed by the
 * sample data. Chunk sizes include the 8 header bytes, as that replayer
 * steps from chunk to chunk by them.
 *
 * With --short the order list holds only the first pattern and the title is
 * "selecta short": one pass of about eight seconds, after which the order
 * list wraps and the replayer reports a loop (the queue tests need a song
 * that ends). The default output is unchanged.
 *
 * usage: gen_testmod [--short] <output.mod> */
#include <stdio.h>
#include <string.h>

#define PATTERNS 4
#define ROWS 64
#define CHANNELS 4
#define SAMPLES 31
#define PATTERN_BYTES (ROWS * CHANNELS * 4)

typedef struct sample_def {
    unsigned char data[512];
    unsigned length;       /* bytes, even */
    unsigned loop_start;   /* bytes */
    unsigned loop_length;  /* bytes; 2 means no loop */
    unsigned volume;
} sample_def;

static const char* g_title = "selecta test";
static unsigned g_song_length = PATTERNS;
static sample_def g_samples[4];
static unsigned char g_patterns[PATTERNS][PATTERN_BYTES];

static void make_samples(void) {
    unsigned i;
    unsigned lfsr = 0xACE1u;
    sample_def* s;

    s = &g_samples[0]; /* square, 64 bytes, looped */
    s->length = 64;
    s->loop_start = 0;
    s->loop_length = 64;
    s->volume = 64;
    for (i = 0; i < 64; i++) {
        s->data[i] = (unsigned char)(i < 32 ? 100 : 156);
    }

    s = &g_samples[1]; /* saw, 64 bytes, looped */
    s->length = 64;
    s->loop_start = 0;
    s->loop_length = 64;
    s->volume = 56;
    for (i = 0; i < 64; i++) {
        int v = (int)i * 4 - 128;
        s->data[i] = (unsigned char)(signed char)(v > 127 ? 127 : v);
    }

    s = &g_samples[2]; /* LFSR noise, 256 bytes, looped */
    s->length = 256;
    s->loop_start = 0;
    s->loop_length = 256;
    s->volume = 40;
    for (i = 0; i < 256; i++) {
        unsigned bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
        lfsr = (lfsr >> 1) | (bit << 15);
        s->data[i] = (unsigned char)(signed char)((int)(lfsr & 0xffu) - 128);
    }

    s = &g_samples[3]; /* decaying click, 512 bytes, one shot */
    s->length = 512;
    s->loop_start = 0;
    s->loop_length = 2;
    s->volume = 64;
    for (i = 0; i < 512; i++) {
        int amp = 127 - (int)(i * 127u / 512u);
        s->data[i] = (unsigned char)(signed char)((i & 8u) ? amp : -amp);
    }
}

/* One cell: sample 1..4 (0 keeps the channel's), period (0 none), effect. */
static void cell(int pat, int row, int ch, unsigned sample, unsigned period, unsigned effect, unsigned param) {
    unsigned char* c = &g_patterns[pat][(row * CHANNELS + ch) * 4];
    c[0] = (unsigned char)((sample & 0xf0u) | ((period >> 8) & 0x0fu));
    c[1] = (unsigned char)(period & 0xffu);
    c[2] = (unsigned char)(((sample & 0x0fu) << 4) | (effect & 0x0fu));
    c[3] = (unsigned char)param;
}

/* Periods for C-2, D-2, E-2, F-2, G-2, A-2, B-2, C-3. */
static const unsigned g_scale[8] = {428, 381, 340, 321, 285, 254, 226, 214};

static void make_patterns(void) {
    int p, r;
    memset(g_patterns, 0, sizeof g_patterns);
    for (p = 0; p < PATTERNS; p++) {
        for (r = 0; r < ROWS; r += 4) {
            unsigned step = (unsigned)((r / 4 + p * 3) % 8);
            /* channel 0: square melody, one note per beat */
            cell(p, r, 0, 1, g_scale[step], 0, 0);
            /* channel 1: saw bass an octave lower, every second beat */
            if ((r % 8) == 0) {
                cell(p, r, 1, 2, g_scale[(step + 4u) % 8] * 2u, 0, 0);
            }
            /* channel 3: click on the beat */
            cell(p, r, 3, 4, 428, 0, 0);
        }
        /* channel 2: noise hits on the off beats */
        for (r = 2; r < ROWS; r += 8) {
            cell(p, r, 2, 3, 214, 0, 0);
        }
    }
    /* pattern 0: speed, volume, arpeggio */
    cell(0, 0, 0, 1, 428, 0xF, 6);
    cell(0, 8, 0, 0, 0, 0xC, 48);
    cell(0, 12, 0, 0, 0, 0x0, 0x37); /* arpeggio */
    cell(0, 16, 0, 1, 340, 0x0, 0x47);
    cell(0, 24, 0, 0, 0, 0xC, 64);
    /* pattern 1: portamento up and down, tone portamento */
    cell(1, 0, 0, 1, 428, 0x1, 0x08);
    cell(1, 4, 0, 0, 0, 0x1, 0x08);
    cell(1, 8, 0, 0, 0, 0x2, 0x08);
    cell(1, 12, 0, 0, 0, 0x2, 0x08);
    cell(1, 16, 0, 1, 428, 0x3, 0x0);
    cell(1, 20, 0, 0, 285, 0x3, 0x10);
    cell(1, 24, 0, 0, 0, 0x3, 0x0);
    /* pattern 2: vibrato and volume slide */
    cell(2, 0, 0, 1, 340, 0x4, 0x46);
    cell(2, 8, 0, 0, 0, 0x4, 0x46);
    cell(2, 16, 1, 2, 428, 0xA, 0x04);
    cell(2, 20, 1, 0, 0, 0xA, 0x40);
    cell(2, 32, 1, 2, 428, 0xC, 24);
    /* pattern 3: all of it, with a speed change to 4 */
    cell(3, 0, 0, 1, 285, 0x4, 0x8a);
    cell(3, 16, 0, 0, 0, 0x0, 0x47);
    cell(3, 32, 0, 0, 0, 0xF, 4);
    cell(3, 48, 0, 0, 0, 0xF, 6);
}

static void put_name(FILE* f, const char* s, unsigned width) {
    unsigned i;
    for (i = 0; i < width; i++) {
        fputc(i < strlen(s) ? (unsigned char)s[i] : 0, f);
    }
}

static void put_word(FILE* f, unsigned v) {
    fputc((int)((v >> 8) & 0xffu), f);
    fputc((int)(v & 0xffu), f);
}

#define VERS_BYTES 0x12u
#define INFO_BYTES 0x34u
#define PTDT_HEADER 0x43Cu

static void put_long(FILE* f, unsigned long v) {
    put_word(f, (unsigned)((v >> 16) & 0xffffu));
    put_word(f, (unsigned)(v & 0xffffu));
}

static void put_zeros(FILE* f, unsigned n) {
    while (n-- > 0) {
        fputc(0, f);
    }
}

static int write_mod(const char* path) {
    FILE* f = fopen(path, "wb");
    unsigned i;
    int p;
    unsigned long sample_bytes = 0;
    unsigned long ptdt_bytes;
    if (f == NULL) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        sample_bytes += g_samples[i].length;
    }
    ptdt_bytes = 8ul + PTDT_HEADER + (unsigned long)PATTERNS * PATTERN_BYTES + sample_bytes;
    fwrite("FORM", 1, 4, f);
    put_long(f, 4ul + VERS_BYTES + INFO_BYTES + ptdt_bytes);
    fwrite("MODL", 1, 4, f);
    fwrite("VERS", 1, 4, f);
    put_long(f, VERS_BYTES);
    put_zeros(f, VERS_BYTES - 8u);
    fwrite("INFO", 1, 4, f);
    put_long(f, INFO_BYTES);
    put_name(f, g_title, 32);
    put_zeros(f, 8);
    put_word(f, 125); /* start tempo */
    put_word(f, 0);   /* flags: Fxx always sets the speed */
    fwrite("PTDT", 1, 4, f);
    put_long(f, ptdt_bytes);
    put_name(f, g_title, 20);
    for (i = 0; i < SAMPLES; i++) {
        if (i < 4) {
            const sample_def* s = &g_samples[i];
            char name[23];
            sprintf(name, "test sample %u", i + 1);
            put_name(f, name, 22);
            put_word(f, s->length / 2u);
            fputc(0, f); /* finetune */
            fputc((int)s->volume, f);
            put_word(f, s->loop_start / 2u);
            put_word(f, s->loop_length / 2u);
        } else {
            put_name(f, "", 22);
            put_word(f, 0);
            fputc(0, f);
            fputc(0, f);
            put_word(f, 0);
            put_word(f, 1);
        }
    }
    fputc((int)g_song_length, f); /* song length */
    fputc(127, f);      /* restart */
    for (i = 0; i < 128; i++) {
        fputc(i < PATTERNS ? (int)i : 0, f);
    }
    fwrite("M.K.", 1, 4, f);
    for (p = 0; p < PATTERNS; p++) {
        fwrite(g_patterns[p], 1, PATTERN_BYTES, f);
    }
    for (i = 0; i < 4; i++) {
        fwrite(g_samples[i].data, 1, g_samples[i].length, f);
    }
    return fclose(f) == 0;
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "--short") == 0) {
        g_title = "selecta short";
        g_song_length = 1;
        argv++;
        argc--;
    }
    if (argc != 2) {
        fputs("usage: gen_testmod [--short] <output.mod>\n", stderr);
        return 2;
    }
    make_samples();
    make_patterns();
    if (!write_mod(argv[1])) {
        fprintf(stderr, "gen_testmod: cannot write %s\n", argv[1]);
        return 1;
    }
    return 0;
}
