/* I710ba (4): the BeRoTracker engine keeps all state in one brt_song, so any number of songs render at once. Opens the songs given on
 * the command line (two or more, or the same file twice), renders each alone to a checksum, then renders all of them interleaved in
 * odd-sized chunks and requires the same checksums. With the engine state in one process-wide global the interleaved run would
 * mix the songs and the checksums differ (or only the last open song would exist at all).
 * usage: berotracker_engine <song.brt> <song.brt> [...]    prints berotracker_engine_ok */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "brt_engine.h"

enum { kMaxSongs = 8, kRate = 22050, kFrames = 3 * kRate };

static uint8_t* slurp(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    uint8_t* b;
    long n;
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (uint8_t*)malloc((size_t)n);
    if (b == NULL || fread(b, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(b);
        return NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return b;
}

static uint64_t fnv(uint64_t h, const int16_t* p, uint32_t frames) {
    uint32_t i;
    for (i = 0; i < frames * 2u; ++i) {
        h = (h ^ (uint16_t)p[i]) * 1099511628211ULL;
    }
    return h;
}

static brt_song* open_song(const char* path) {
    size_t size = 0;
    uint8_t* data = slurp(path, &size);
    brt_info* bi = (brt_info*)calloc(1, sizeof *bi);
    brt_song* s = data != NULL && bi != NULL ? brt_open(data, size, bi) : NULL;
    free(data);
    free(bi);
    return s;
}

int main(int argc, char** argv) {
    uint64_t alone[kMaxSongs], mixed[kMaxSongs];
    brt_song* s[kMaxSongs];
    int16_t* buf = (int16_t*)malloc((size_t)4096 * 4u);
    int n = argc - 1, i;
    uint32_t done, chunk = 1;
    if (n < 2 || n > kMaxSongs || buf == NULL) {
        fprintf(stderr, "usage: berotracker_engine <song.brt> <song.brt> [...]\n");
        return 2;
    }
    for (i = 0; i < n; ++i) {
        uint32_t left = kFrames;
        uint64_t h = 14695981039346656037ULL;
        brt_song* one = open_song(argv[i + 1]);
        if (one == NULL || !brt_start(one, kRate)) {
            fprintf(stderr, "FAIL: cannot open or start %s\n", argv[i + 1]);
            return 1;
        }
        while (left > 0) {
            uint32_t got = brt_render(one, buf, left < 4096u ? left : 4096u);
            if (got == 0u) {
                break;
            }
            h = fnv(h, buf, got);
            left -= got;
        }
        brt_close(one);
        alone[i] = h;
        mixed[i] = 14695981039346656037ULL;
    }
    for (i = 0; i < n; ++i) {
        s[i] = open_song(argv[i + 1]);
        if (s[i] == NULL || !brt_start(s[i], kRate)) {
            fprintf(stderr, "FAIL: song %d did not open beside the others (state is global)\n", i);
            return 1;
        }
    }
    for (done = 0; done < kFrames;) {
        uint32_t want = kFrames - done < chunk ? kFrames - done : chunk;
        for (i = 0; i < n; ++i) {
            uint32_t got = brt_render(s[i], buf, want);
            mixed[i] = fnv(mixed[i], buf, got);
        }
        done += want;
        chunk = chunk * 3u % 4001u + 1u;
    }
    for (i = 0; i < n; ++i) {
        brt_close(s[i]);
        if (mixed[i] != alone[i]) {
            fprintf(stderr, "FAIL: %s rendered differently beside other songs (%016llx vs %016llx)\n", argv[i + 1],
                    (unsigned long long)mixed[i], (unsigned long long)alone[i]);
            return 1;
        }
    }
    free(buf);
    puts("berotracker_engine_ok");
    return 0;
}
