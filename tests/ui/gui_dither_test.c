/* gui-dither-atkinson and mui-art-planar-cache (MUI-P5-7), plan 2026-10-25-amiga-mui.md.
 * Usage: gui_dither_test dither|cache <scratch dir> */
#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_image.h>

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

#define E 16
#define P 2

static uint64_t fnv(const uint8_t* b, size_t n) {
    uint64_t h = 1469598103934665603ull;
    size_t i;
    for (i = 0; i < n; i++) {
        h = (h ^ b[i]) * 1099511628211ull;
    }
    return h;
}

static void gradient(uint32_t* px) {
    int x, y;
    for (y = 0; y < E; y++) {
        for (x = 0; x < E; x++) {
            uint32_t g = (uint32_t)x * 17u;
            px[y * E + x] = 0xFF000000u | (g << 16) | (g << 8) | g;
        }
    }
}

static void cover(uint32_t* px) { /* fixed pseudo-random colours */
    uint32_t s = 0x1234567u;
    int i;
    for (i = 0; i < E * E; i++) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        px[i] = 0xFF000000u | (s & 0xFFFFFFu);
    }
}

static int ones(const uint8_t* b) {
    int n = 0, i;
    for (i = 0; i < P * E; i++) {
        uint8_t v = b[i];
        while (v != 0u) {
            n += v & 1u;
            v >>= 1;
        }
    }
    return n;
}

static int test_dither(void) {
    uint32_t px[E * E];
    uint8_t out[P * E];
    int i;
    for (i = 0; i < E * E; i++) px[i] = 0xFF000000u;
    CHECK(sel_gui_dither_atkinson(px, E, E, out, P) == 1 && ones(out) == 0, "all black gives ones");
    for (i = 0; i < E * E; i++) px[i] = 0xFFFFFFFFu;
    CHECK(sel_gui_dither_atkinson(px, E, E, out, P) == 1 && ones(out) == E * E, "all white gives zeros");
    gradient(px);
    sel_gui_dither_atkinson(px, E, E, out, P);
    CHECK(fnv(out, sizeof out) == GRADIENT_GOLDEN, "gradient hash %llx", (unsigned long long)fnv(out, sizeof out));
    CHECK(ones(out) > 64 && ones(out) < 192, "gradient density %d", ones(out));
    cover(px);
    sel_gui_dither_atkinson(px, E, E, out, P);
    CHECK(fnv(out, sizeof out) == COVER_GOLDEN, "cover hash %llx", (unsigned long long)fnv(out, sizeof out));
    CHECK(sel_gui_dither_atkinson(px, E, E, out, 1) == 0, "pitch too small accepted");
    return failures;
}

static int test_cache(const char* dir) {
    uint32_t px[E * E];
    uint8_t first[P * E];
    sel_gui_bitart* c = sel_gui_bitart_open(E, 2, dir);
    const uint8_t* a;
    char path[512];
    FILE* f;
    long size;
    cover(px);
    (void)snprintf(path, sizeof path, "%s/%x.bit", dir, 77u);
    (void)remove(path); /* a rerun starts cold */
    CHECK(c != NULL && sel_gui_bitart_pitch(c) == P, "open");
    CHECK(sel_gui_bitart_get(c, 77, NULL) == NULL, "miss without pixels");
    a = sel_gui_bitart_get(c, 77, px);
    CHECK(a != NULL && sel_gui_bitart_dithers(c) == 1u, "first request dithers once");
    memcpy(first, a, sizeof first);
    a = sel_gui_bitart_get(c, 77, px);
    CHECK(a != NULL && sel_gui_bitart_dithers(c) == 1u && memcmp(a, first, sizeof first) == 0, "memory hit dithered again");
    f = fopen(path, "rb");
    CHECK(f != NULL, "disk file missing");
    if (f != NULL) {
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fclose(f);
        CHECK(size == 6 + P * E, "file size %ld", size);
    }
    sel_gui_bitart_close(c);
    c = sel_gui_bitart_open(E, 2, dir); /* new process: memory empty, file present */
    a = sel_gui_bitart_get(c, 77, NULL);
    CHECK(a != NULL && sel_gui_bitart_dithers(c) == 0u && memcmp(a, first, sizeof first) == 0, "disk round trip");
    sel_gui_bitart_close(c);
    return failures;
}

int main(int argc, char** argv) {
    int r = 1;
    if (argc > 1 && strcmp(argv[1], "dither") == 0) {
        r = test_dither();
    } else if (argc > 2 && strcmp(argv[1], "cache") == 0) {
        r = test_cache(argv[2]);
    }
    printf("%s\n", r == 0 ? "PASS" : "FAILED");
    return r == 0 ? 0 : 1;
}
