/* Song length database reader (I605, v2 flags I774k); format in songlen.h. */
#include "songlen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* subsong == SEL_FIND_ONLY: only locate the entry. */
#define SEL_FIND_ONLY 0xffffffffu

static int search(const char* path, const char* key, uint32_t subsong, uint32_t* ms, uint32_t* songend, uint32_t* dup,
                  uint32_t* entry, uint32_t* reads) {
    FILE* f;
    uint8_t want[6], hdr[SEL_SONGLEN_HEADER], ent[SEL_SONGLEN_ENTRY];
    uint32_t lo = 0, hi, lengths_at, version, nreads = 0;
    int found = 0;

    if (reads != NULL) *reads = 0;
    if (path == NULL || key == NULL || !sel_songlen_key_bytes(key, want)) return 0;
    f = fopen(path, "rb");
    if (f == NULL) return 0;
    nreads++;
    version = 0;
    if (fread(hdr, 1, sizeof hdr, f) == sizeof hdr && memcmp(hdr, "SLDB", 4) == 0) version = be32(hdr + 4);
    if (version != 1u && version != 2u) {
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
            if (entry != NULL) *entry = mid;
            if (subsong == SEL_FIND_ONLY) {
                found = 1;
            } else if (subsong >= first && subsong < first + count) {
                uint8_t lb[4];
                uint32_t idx = be32(ent + 8) + (subsong - first);
                if (fseek(f, (long)(lengths_at + (uint64_t)idx * 4u), SEEK_SET) == 0) {
                    nreads++;
                    if (fread(lb, 1, 4, f) == 4) {
                        uint32_t w = be32(lb), v = version == 2u ? (w & SEL_SONGLEN_MS_MASK) : w;
                        if (v != 0u) {
                            if (ms != NULL) *ms = v;
                            if (songend != NULL) *songend = version == 2u ? (w >> SEL_SONGLEN_END_SHIFT) & 15u : 0u;
                            if (dup != NULL) *dup = version == 2u ? (w >> 31) : 0u;
                            found = 1;
                        }
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

int sel_songlen_lookup(const char* path, const char* key, uint32_t subsong, uint32_t* ms, uint32_t* reads) {
    return search(path, key, subsong, ms, NULL, NULL, NULL, reads);
}

int sel_songlen_lookup_ex(const char* path, const char* key, uint32_t subsong, uint32_t* ms, uint32_t* songend,
                          uint32_t* dup, uint32_t* entry) {
    return search(path, key, subsong, ms, songend, dup, entry, NULL);
}

int sel_songlen_find(const char* path, const char* key, uint32_t* entry) {
    return search(path, key, SEL_FIND_ONLY, NULL, NULL, NULL, entry, NULL);
}
