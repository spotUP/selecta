/* xxHash32 (public algorithm by Yann Collet, BSD-2); see xxh32.h. */
#include "xxh32.h"

#include <stdio.h>
#include <stdlib.h>

#define P1 2654435761u
#define P2 2246822519u
#define P3 3266489917u
#define P4 668265263u
#define P5 374761393u

static uint32_t rotl(uint32_t x, unsigned r) {
    return (x << r) | (x >> (32u - r));
}

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t round1(uint32_t acc, uint32_t in) {
    return rotl(acc + in * P2, 13) * P1;
}

uint32_t sel_xxh32(const void* data, size_t len, uint32_t seed) {
    const uint8_t* p = data;
    const uint8_t* end = p + len;
    uint32_t h;

    if (len >= 16) {
        const uint8_t* limit = end - 16;
        uint32_t v1 = seed + P1 + P2, v2 = seed + P2, v3 = seed, v4 = seed - P1;

        do {
            v1 = round1(v1, rd32(p));
            v2 = round1(v2, rd32(p + 4));
            v3 = round1(v3, rd32(p + 8));
            v4 = round1(v4, rd32(p + 12));
            p += 16;
        } while (p <= limit);
        h = rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18);
    } else {
        h = seed + P5;
    }
    h += (uint32_t)len;
    while (p + 4 <= end) {
        h = rotl(h + rd32(p) * P3, 17) * P4;
        p += 4;
    }
    while (p < end) {
        h = rotl(h + *p * P5, 11) * P1;
        p++;
    }
    h ^= h >> 15;
    h *= P2;
    h ^= h >> 13;
    h *= P3;
    h ^= h >> 16;
    return h;
}

void sel_uade_key(const void* head, size_t head_len, uint64_t total_len, char out[SEL_UADE_KEY_CHARS + 1u]) {
    static const char hex[] = "0123456789abcdef";
    uint32_t h = sel_xxh32(head, head_len < SEL_UADE_KEY_HASH_BYTES ? head_len : SEL_UADE_KEY_HASH_BYTES, 0);
    uint32_t l = (uint32_t)(total_len & 0xFFFFu);
    unsigned i;

    for (i = 0; i < 8; i++) {
        out[i] = hex[(h >> (28u - 4u * i)) & 15u];
    }
    for (i = 0; i < 4; i++) {
        out[8 + i] = hex[(l >> (12u - 4u * i)) & 15u];
    }
    out[12] = '\0';
}

long sel_file_head(const char* path, void* buf, size_t cap, size_t* got) {
    FILE* f = fopen(path, "rb");
    long size;

    if (f == NULL) return -1;
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    *got = fread(buf, 1, cap, f);
    fclose(f);
    return size;
}

long sel_file_key(const char* path, char out[SEL_UADE_KEY_CHARS + 1u]) {
    /* On demand, not a 256 KB static: a resident bss buffer is image size on an Amiga (I271). */
    unsigned char* head = malloc(SEL_UADE_KEY_HASH_BYTES);
    size_t got = 0;
    long size;

    if (head == NULL) return -1;
    size = sel_file_head(path, head, SEL_UADE_KEY_HASH_BYTES, &got);
    if (size >= 0) sel_uade_key(head, got, (uint64_t)size, out);
    free(head);
    return size;
}

int sel_uade_key_file(const char* path, char out[SEL_UADE_KEY_CHARS + 1u]) {
    return sel_file_key(path, out) >= 0;
}
