/* SPDX-License-Identifier: GPL-2.0-or-later
 * Selecta glue for GoatTracker 1 and 2 songs (row I710al). Written for Selecta; it contains no bbmp code. It drives GoatTracker 2.77's own play routine
 * (gplay.c) and song loader (gsong.c, which also imports GoatTracker 1 "GTS!" and GoatTracker 2.0 "GTS2" songs) and writes the SID registers into
 * reSID (Dag Lem, GPL-2.0-or-later) exactly as GoatTracker's gsid.cpp does: 50 Hz frames, register write order sid_getorder(), 14 cycle write delay.
 * Song header defaults apply: PAL clock, MOS 6581, speed multiplier 1, hard restart 0x0f00 (GoatTracker's own defaults); the song file carries none of them.
 * The loader trusts its input (unchecked counts), so the structure of every song is validated here first. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include <unordered_set>
#include <vector>

extern "C" {
#include "gt_headless.h"
}
#include "sid.h"

extern "C" {
extern unsigned char filtercutoff, filtertime, filterptr, funktable[2];
extern unsigned char masterfader;
extern int songinit, psnum;
extern INSTR instr[MAX_INSTR];
void gtsong_unused(void);

unsigned char sidreg[NUMSIDREGS];
FILTERPARAMS filterparams;
}

/* Editor state: defined once; the play routine and the loader read them. */
extern "C" {
int followplay = 0, defaultpatternlength = 64;
unsigned multiplier = 1, adparam = 0x0f00, finevibrato = 1, optimizepulse = 1, optimizerealtime = 1;
int epnum[MAX_CHN], espos[MAX_CHN], esend[MAX_CHN];
int eppos, epview, epchn, epcolumn, epmarkchn, esnum, eschn, escolumn, eseditpos, esview, esmarkchn, enpos;
char songfilename[MAX_FILENAME], loadedsongfilename[MAX_FILENAME], instrfilename[MAX_FILENAME];
int instrused[MAX_INSTR];
int pattused[MAX_PATT];
/* ginstr.c editor state that ginstr.h declares and gsong.c touches (gtable.c defines its own et* state). */
int einum, eipos, eicolumn;
INSTR instrcopybuffer;

/* Editor-only entry points that gsong.c references from loadinstrument() and optimizeeverything(). The headless engine only calls
   loadsong(), which reaches neither, so they are empty; a call would be a bug and is a no-op rather than editor behaviour. */
extern "C" void deleteinstrtable(int) {}
extern "C" void optimizetable(int) {}

/* ginstr.c clearinstr, verbatim: the rest of ginstr.c is the instrument editor and is not vendored. */
extern "C" void clearinstr(int num) {
    memset(&instr[num], 0, sizeof(INSTR));
    if (num) {
        if (multiplier) instr[num].gatetimer = 2 * multiplier;
        else instr[num].gatetimer = 1;
        instr[num].firstwave = 0x9;
    }
}

static const unsigned char* g_mem;
static size_t g_memsize;

FILE* gt_open_song(void) { return g_mem ? fmemopen((void*)g_mem, g_memsize, "rb") : NULL; }
int fread8(FILE* h) { int c = fgetc(h); return c == EOF ? 0 : c; }
void fwrite8(FILE* h, unsigned char v) { (void)h; (void)v; }
void songchange(void) {}
void resettime(void) {}
void incrementtime(void) {}
void sound_suspend(void) {}
void sound_flush(void) {}
}

static const int CLOCK_PAL = 985248;
static const int FRAME_HZ = 50;
static const unsigned MAX_SECONDS = 20 * 60;

static unsigned char kSidOrder[] = {0x15, 0x16, 0x18, 0x17, 0x05, 0x06, 0x02, 0x03, 0x00, 0x01, 0x04, 0x0c, 0x0d,
                                    0x09, 0x0a, 0x07, 0x08, 0x0b, 0x13, 0x14, 0x10, 0x11, 0x0e, 0x0f, 0x12};
static unsigned char kAltSidOrder[] = {0x15, 0x16, 0x18, 0x17, 0x04, 0x00, 0x01, 0x02, 0x03, 0x05, 0x06, 0x0b, 0x07,
                                       0x08, 0x09, 0x0a, 0x0c, 0x0d, 0x12, 0x0e, 0x0f, 0x10, 0x11, 0x13, 0x14};
extern "C" unsigned char sid_getorder(unsigned char index) { return adparam >= 0xf000 ? kAltSidOrder[index] : kSidOrder[index]; }

static uint32_t g_rate = 44100;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

struct gtsong {
    unsigned char* data;
    size_t size;
    char title[MAX_STR + 1], author[MAX_STR + 1], format[24];
    uint32_t frames; /* play-routine frames until the song's loop repeats */
};

/* ---- structural validation (the loader's counts index fixed arrays) ---- */

struct cursor {
    const unsigned char* p;
    size_t n, at;
    bool take(size_t k) { if (k > n - at) return false; at += k; return true; }
    int byte() { return at < n ? p[at++] : -1; }
};

static bool tables_ok(cursor& c, int tables) {
    for (int t = 0; t < tables; t++) {
        int len = c.byte();
        if (len < 0 || !c.take((size_t)len * 2)) return false;
    }
    return true;
}

static bool orders_ok(cursor& c) {
    int songs = c.byte();
    if (songs < 1 || songs > MAX_SONGS) return false;
    for (int s = 0; s < songs; s++)
        for (int ch = 0; ch < MAX_CHN; ch++) {
            int len = c.byte();
            if (len < 0 || len > MAX_SONGLEN || !c.take((size_t)len + 1)) return false;
        }
    return true;
}

static bool gt2_ok(const unsigned char* d, size_t n, int tables) {
    cursor c = {d, n, 4};
    if (!c.take(3 * MAX_STR) || !orders_ok(c)) return false;
    int instrs = c.byte();
    if (instrs < 0 || instrs >= MAX_INSTR || !c.take((size_t)instrs * (9 + MAX_INSTRNAMELEN))) return false;
    if (!tables_ok(c, tables)) return false;
    int pats = c.byte();
    if (pats < 0 || pats > MAX_PATT) return false;
    for (int p = 0; p < pats; p++) {
        int rows = c.byte();
        if (rows < 1 || rows > MAX_PATTROWS + 1 || !c.take((size_t)rows * 4)) return false;
    }
    return c.at == n;
}

static bool gt1_ok(const unsigned char* d, size_t n) {
    cursor c = {d, n, 4};
    if (!c.take(3 * MAX_STR) || !orders_ok(c)) return false;
    unsigned wave_entries = 0;
    for (int i = 1; i < 32; i++) {
        if (!c.take(6)) return false;
        int filt = c.byte();
        int wl = c.byte();
        if (filt < 0 || filt > 63 || wl < 0 || !c.take(MAX_INSTRNAMELEN)) return false;
        int ent = wl / 2;
        if (ent) {
            /* the converter walks a wavetable to its 0xff end: demand one in every non-empty table */
            bool end = false;
            for (int e = 0; e < ent; e++) {
                int l = d[c.at + (size_t)e * 2];
                if (l == 0xff) end = true;
            }
            if (!end) return false;
        }
        wave_entries += (unsigned)ent;
        if (!c.take((size_t)ent * 2)) return false;
        if (wl & 1) return false;
    }
    if (wave_entries >= MAX_TABLELEN - 8) return false;
    int pats = c.byte();
    if (pats < 0 || pats > MAX_PATT) return false;
    for (int p = 0; p < pats; p++) {
        int len = c.byte();
        if (len < 0) return false;
        int rows = len / 3;
        size_t base = c.at;
        if (!c.take((size_t)rows * 3)) return false;
        for (int r = 0; r < rows; r++) {
            int cmd = d[base + (size_t)r * 3 + 1] & 7;
            int data = d[base + (size_t)r * 3 + 2];
            if (cmd == 5 && data > 63) return false;
        }
    }
    if (!c.take(256)) return false;
    for (int s = 0; s < 64; s++)
        if (d[c.at - 256 + (size_t)s * 4 + 3] > 63) return false;
    return c.at == n;
}

static int kind_of(const unsigned char* d, size_t n) {
    if (n < 4 + 3 * MAX_STR + 8 || n > 1u << 20) return 0;
    if (!memcmp(d, "GTS!", 4)) return gt1_ok(d, n) ? 1 : 0;
    if (!memcmp(d, "GTS2", 4)) return gt2_ok(d, n, 3) ? 2 : 0;
    if (!memcmp(d, "GTS3", 4) || !memcmp(d, "GTS4", 4) || !memcmp(d, "GTS5", 4)) return gt2_ok(d, n, 4) ? 2 : 0;
    return 0;
}

/* ---- running the song ---- */

static bool load_locked(gtsong* s) {
    g_mem = s->data;
    g_memsize = s->size;
    snprintf(songfilename, sizeof songfilename, "memory");
    clearsong(1, 1, 1, 1, 1);
    loadsong();
    g_mem = NULL;
    if (!songlen[0][0] || !songlen[0][1] || !songlen[0][2]) return false;
    return true;
}

static void start_locked(void) {
    multiplier = 1;
    adparam = 0x0f00;
    memset(sidreg, 0, sizeof sidreg);
    masterfader = 0x0f;
    initchannels();
    initsong(0, PLAY_BEGINNING);
}

static uint64_t position_hash(void) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    for (int c = 0; c < MAX_CHN; c++) {
        const CHN& k = chn[c];
        mix(k.songptr); mix(k.pattptr); mix(k.pattnum); mix(k.repeat); mix(k.tick); mix(k.tempo); mix(k.trans); mix(k.instr);
        mix(k.advance); mix(k.gatetimer);
    }
    mix(funktable[0]); mix(funktable[1]);
    return h;
}

/* Runs the play routine until the song position repeats; calls sink once per frame (after the registers are set). */
template <class F> static uint32_t run_locked(F sink) {
    std::unordered_set<uint64_t> seen;
    uint32_t frames = 0;
    while (frames < MAX_SECONDS * FRAME_HZ) {
        if (!seen.insert(position_hash()).second) break;
        playroutine();
        if (songinit == PLAY_STOPPED) break;
        sink();
        frames++;
    }
    return frames;
}

extern "C" {

int gtsong_probe(const uint8_t* d, size_t n) { return kind_of(d, n) != 0; }

gtsong* gtsong_open(const uint8_t* d, size_t n) {
    int kind = kind_of(d, n);
    if (!kind) return NULL;
    gtsong* s = (gtsong*)calloc(1, sizeof *s);
    if (!s) return NULL;
    s->data = (unsigned char*)malloc(n);
    if (!s->data) { free(s); return NULL; }
    memcpy(s->data, d, n);
    s->size = n;
    pthread_mutex_lock(&g_lock);
    bool ok = load_locked(s);
    if (ok) {
        memcpy(s->title, songname, MAX_STR);
        memcpy(s->author, authorname, MAX_STR);
        snprintf(s->format, sizeof s->format, "GoatTracker %d", kind);
        start_locked();
        s->frames = run_locked([] {});
    }
    pthread_mutex_unlock(&g_lock);
    if (!ok || !s->frames) { free(s->data); free(s); return NULL; }
    return s;
}

void gtsong_close(gtsong* s) { if (s) { free(s->data); free(s); } }
const char* gtsong_title(const gtsong* s) { return s->title; }
const char* gtsong_author(const gtsong* s) { return s->author; }
const char* gtsong_format(const gtsong* s) { return s->format; }
uint32_t gtsong_duration_ms(const gtsong* s) { return (uint32_t)((uint64_t)s->frames * 1000u / FRAME_HZ); }

/* One frame of SID output, the order and timing of GoatTracker's sid_fillbuffer (without its optional random bad-line delay). */
static int fill_frame(SID* sid, short* ptr, int samples) {
    int total = 0, result;
    int tdelta = CLOCK_PAL * samples / (int)g_rate;
    if (tdelta <= 0) return 0;
    for (int c = 0; c < NUMSIDREGS; c++) {
        unsigned char o = sid_getorder((unsigned char)c);
        sid->write(o, sidreg[o]);
        int d2 = SIDWRITEDELAY;
        result = sid->clock(d2, ptr, samples);
        total += result; ptr += result; samples -= result;
        tdelta -= SIDWRITEDELAY;
        if (tdelta <= 0) return total;
    }
    result = sid->clock(tdelta, ptr, samples);
    total += result; ptr += result; samples -= result;
    while (samples) {
        tdelta = CLOCK_PAL * samples / (int)g_rate;
        if (tdelta <= 0) return total;
        result = sid->clock(tdelta, ptr, samples);
        total += result; ptr += result; samples -= result;
    }
    return total;
}

/* Renders the whole song to stereo 16-bit PCM (malloc'd, caller frees). */
int gtsong_render(gtsong* s, uint32_t rate, int16_t** pcm, uint64_t* frames) {
    int ok = 0;
    pthread_mutex_lock(&g_lock);
    if (load_locked(s)) {
        SID* sid = new SID;
        g_rate = rate;
        sid->set_chip_model(MOS6581);
        sid->set_sampling_parameters(CLOCK_PAL, SAMPLE_INTERPOLATE, rate);
        sid->reset();
        start_locked();
        uint64_t cap = (uint64_t)s->frames * rate / FRAME_HZ + rate / FRAME_HZ + 64u;
        int16_t* out = (int16_t*)malloc((size_t)cap * 4);
        if (out) {
            uint64_t n = 0, acc = 0;
            std::vector<short> mono(rate / FRAME_HZ + 2);
            run_locked([&] {
                acc += rate;
                int want = (int)(acc / FRAME_HZ);
                acc %= FRAME_HZ;
                int got = fill_frame(sid, mono.data(), want);
                for (int i = 0; i < got && n < cap; i++, n++) out[n * 2] = out[n * 2 + 1] = mono[i];
            });
            *pcm = out;
            *frames = n;
            ok = 1;
        }
        delete sid;
    }
    pthread_mutex_unlock(&g_lock);
    return ok;
}

}
