/* Song length database reader and baker (I605); format in songlen.h. */
#include "songlen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

int sel_songlen_lookup(const char* path, const char* key, uint32_t subsong, uint32_t* ms, uint32_t* reads) {
    FILE* f;
    uint8_t want[6], hdr[SEL_SONGLEN_HEADER], ent[SEL_SONGLEN_ENTRY];
    uint32_t lo = 0, hi, lengths_at, nreads = 0;
    int found = 0;

    if (reads != NULL) *reads = 0;
    if (path == NULL || key == NULL || !sel_songlen_key_bytes(key, want)) return 0;
    f = fopen(path, "rb");
    if (f == NULL) return 0;
    nreads++;
    if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr || memcmp(hdr, "SLDB", 4) != 0 || be32(hdr + 4) != 1u) {
        fclose(f);
        return 0;
    }
    hi = be32(hdr + 8);
    lengths_at = be32(hdr + 12);
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        int c;
        if (fseek(f, (long)(SEL_SONGLEN_HEADER + (uint64_t)mid * SEL_SONGLEN_ENTRY), SEEK_SET) != 0) break;
        nreads++;
        if (fread(ent, 1, sizeof ent, f) != sizeof ent) break;
        c = memcmp(want, ent, 6);
        if (c < 0) {
            hi = mid;
        } else if (c > 0) {
            lo = mid + 1u;
        } else {
            uint32_t first = ent[6], count = ent[7];
            if (subsong >= first && subsong < first + count) {
                uint8_t lb[4];
                uint32_t idx = be32(ent + 8) + (subsong - first);
                if (fseek(f, (long)(lengths_at + (uint64_t)idx * 4u), SEEK_SET) == 0) {
                    nreads++;
                    if (fread(lb, 1, 4, f) == 4 && be32(lb) != 0u) {
                        if (ms != NULL) *ms = be32(lb);
                        found = 1;
                    }
                }
            }
            break;
        }
    }
    fclose(f);
    if (reads != NULL) *reads = nreads;
    return found;
}
