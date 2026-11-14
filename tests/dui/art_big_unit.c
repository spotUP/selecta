/* I1384d: a view that draws a cover bigger than the 144 px tile gets the cover decoded from the stored full image at its own size.
 * Through art_load / art_big / art_pump: the first art_big call returns NULL at once (async), after art_pump the pixels are edge x edge,
 * aspect kept with black bars (a 2:1 red picture: top and bottom rows black, centre red), a second call returns the cached pixels,
 * a size at or below the tile edge gives NULL. */
#include "art.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const uint8_t g_png[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,40,0,0,0,20,8,2,0,0,0,112,36,232,236,0,0,0,36,73,68,65,84,120,156,237,205,49,13,0,0,12,4,161,247,111,186,149,113,11,9,59,187,45,209,172,98,177,88,44,22,139,197,137,7,125,250,29,14,125,226,90,57,0,0,0,0,73,69,78,68,174,66,96,130}; /* 40x20 solid red */
static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static int64_t fx_find(void* ctx, const char* ref) {
    (void)ctx;
    (void)ref;
    return 7;
}

static uint32_t fx_get(void* ctx, int64_t album, const char* kind, uint8_t* out, uint32_t cap) {
    (void)ctx;
    (void)album;
    if (strcmp(kind, "cover") != 0) return 0;
    if (out != NULL && cap >= sizeof g_png) memcpy(out, g_png, sizeof g_png);
    return (uint32_t)sizeof g_png;
}

static int fx_put(void* ctx, int64_t album, const char* kind, const uint8_t* data, uint32_t len) {
    (void)ctx;
    (void)album;
    (void)kind;
    (void)data;
    (void)len;
    return 1;
}

static void drain(desk_art* a) {
    time_t t0 = time(NULL);
    while (art_busy(a) && time(NULL) - t0 < 10) {
        (void)art_pump(a);
        usleep(2000);
    }
}

int main(void) {
    enum { EDGE = 200 };
    sel_ui_art src = {NULL, fx_find, fx_get, fx_put};
    const char* keys[1] = {"song"};
    desk_art* a = art_open(&src);
    const uint32_t* big;

    CHECK(a != NULL);
    art_load(a, keys, 1);
    drain(a);
    CHECK(art_tile(a, "song") != NULL);
    CHECK(art_big(a, "song", DESK_ART_TILE_EDGE) == NULL); /* the tile serves this size */
    CHECK(art_big(a, "song", EDGE) == NULL);               /* the decode starts on a thread */
    drain(a);
    big = art_big(a, "song", EDGE);
    CHECK(big != NULL);
    if (big != NULL) {
        CHECK(big[0] == 0xFF000000u && big[(EDGE - 1) * EDGE + 5] == 0xFF000000u); /* bars above and below */
        CHECK((big[(EDGE / 2) * EDGE + EDGE / 2] & 0x00FFFFFFu) == 0x00FF0000u);   /* the picture in the middle */
        CHECK(art_big(a, "song", EDGE) == big);                                    /* cached */
    }
    art_close(a);
    if (g_fail == 0) printf("art_big_ok\n");
    return g_fail != 0;
}
