/* CHIPNSFX engine wrapper (row I432): includes the tracker's source (GPL-3.0-or-later, bbmp's pinned copy, built
 * without its user interface) and exposes the loader and sound code as engine.h. The load-time repairs of damaged
 * songs and the WAVE-export sound setup follow bbmp's csrc/chipnsfx/bbmp-chipnsfx.c. */
#define DEBUG /* the tracker without its user interface */
#define main chipnsfx_main
#include "CHIPNSFX.C"
#undef main

#include "engine.h"

#define CHP_NAME_MAX 16 /* instr[i][8..24] */

/* The song as the loader may read it: lines no longer than its buffer, at most 255 orders (the loader writes them
 * before it counts them), instrument names of at most 16 bytes. */
static char* tidy(const unsigned char* data, size_t size, size_t* out_size) {
    char* out = malloc(size + 2);
    size_t o = 0, line = 0, orders = 0, i = 0;
    int in_orders = 0;
    if (!out) return NULL;
    while (i < size) {
        size_t end = i, len;
        const char* l;
        int keep = 1;
        while (end < size && data[end] != '\n') end++;
        len = end - i;
        if (len > MAXSTRLEN - 2) len = MAXSTRLEN - 2;
        l = (const char*)data + i;
        if (line == 5) in_orders = 1;
        if (in_orders && (len == 0 || l[0] != '#')) in_orders = 0;
        if (in_orders && ++orders > 255) keep = 0;
        if (keep && len > 0 && l[0] == ':') {
            size_t spaces = 0, at = 0, k;
            for (k = 0; k < len; k++)
                if (l[k] == ' ' && ++spaces == 4) { at = k + 1; break; }
            if (at && len - at > CHP_NAME_MAX) len = at + CHP_NAME_MAX;
        }
        if (keep) {
            memcpy(out + o, l, len);
            o += len;
            out[o++] = '\n';
        }
        line++;
        i = end + 1;
    }
    *out_size = o;
    return out;
}

void chp_restart(void) {
    snd_order = 0;
    snd_score = 0;
    flag_n = 1; /* play to the end once; the loop isn't played again */
    snd_play(0);
}

int chp_frame(unsigned char* out) {
    if (!snd_frame(1)) return 0;
    snd_waves(out);
    return 1;
}

int chp_load(const unsigned char* data, size_t size, chp_info* info, const char** why) {
    size_t tidy_size;
    char* song;
    FILE* mem;
    FILE* saved_in = stdin;
    int ok;
    long frames = 0;

    if (size == 0 || size > (1u << 20)) { *why = "can't read the file"; return 0; }
    song = tidy(data, size, &tidy_size);
    if (!song) { *why = "out of memory"; return 0; }
    mem = fmemopen(song, tidy_size, "r");
    if (!mem) { free(song); *why = "out of memory"; return 0; }
    stdin = mem; /* loadsong("-") reads stdin */
    ok = loadsong("-");
    stdin = saved_in;
    fclose(mem);
    free(song);
    if (!ok) { *why = "not a CHIPNSFX song"; return 0; }
    if (!(hertz >= 1 && hertz <= 1000 && divisor >= 1 && divisor <= 255)) {
        *why = "the song's tempo is not one a song can have";
        return 0;
    }
    if (!orders || !scrln[order[0][0][0]] || !scrln[order[1][0][0]] || !scrln[order[2][0][0]]) {
        *why = "the song has nothing to play";
        return 0;
    }
    /* as the tracker's WAVE export sets it up, with its oversampling (-Q) and stereo ABC */
    SND_DEPTH = 1;
    SND_DEPTHSTEP = 125000 << (1 + SND_FIXED - SND_DEPTH);
    SND_DEPTHMASK = (1 << SND_DEPTH) - 1;
    flag_s = 1, flag_ss = 1;
    stereo_u[0] = 2, stereo_u[1] = 1, stereo_u[2] = 0;

    chp_restart();
    while (frames < CHP_MAX_FRAMES && snd_frame(1)) frames++;
    if (!frames) { *why = "the song has nothing to play"; return 0; }
    memset(info, 0, sizeof *info);
    strncpy(info->title, (const char*)title1, sizeof info->title - 1);
    strncpy(info->comment, (const char*)title2, sizeof info->comment - 1);
    info->hertz = hertz;
    info->divisor = divisor;
    info->frames = frames;
    return 1;
}
