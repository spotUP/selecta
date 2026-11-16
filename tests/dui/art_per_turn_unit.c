/* I1416f: without a thread (Amiga) art_load only queues and each art_pump turn decodes exactly one cover, in queue order.
 * art.c is linked with amiga/art_thread.c (inline worker) to prove it on the host. */
#include "art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TITLES = 5 };

static int g_fail, g_finds;
static uint8_t g_blob[SEL_GUI_THUMB_HEAD + 4u * DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE];
static uint32_t g_blob_len;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static int64_t fx_find(void* ctx, const char* ref) {
    (void)ctx;
    g_finds++;
    return 1 + (int64_t)atoi(ref + 1);
}

static uint32_t fx_get(void* ctx, int64_t album, const char* kind, uint8_t* out, uint32_t cap) {
    (void)ctx;
    (void)album;
    if (strcmp(kind, "thumbf144") != 0) return 0;
    if (out != NULL && cap >= g_blob_len) memcpy(out, g_blob, g_blob_len);
    return g_blob_len;
}

static int fx_put(void* ctx, int64_t album, const char* kind, const uint8_t* data, uint32_t len) {
    (void)ctx;
    (void)album;
    (void)kind;
    (void)data;
    (void)len;
    return 1;
}

int main(void) {
    static uint32_t px[DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE];
    static char names[TITLES][4];
    const char* keys[TITLES];
    sel_ui_art src = {NULL, fx_find, fx_get, fx_put, NULL};
    desk_art* a;
    uint32_t k;
    int i;

    for (i = 0; i < DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE; i++) px[i] = 0xFF000000u | (uint32_t)(i * 2654435761u >> 8);
    g_blob_len = sel_gui_thumb_pack(px, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, g_blob, sizeof g_blob);
    for (k = 0; k < TITLES; k++) {
        snprintf(names[k], sizeof names[k], "t%u", (unsigned)k);
        keys[k] = names[k];
    }
    a = art_open(&src);
    CHECK(a != NULL);
    art_load(a, keys, TITLES);
    CHECK(g_finds == 0 && art_busy(a)); /* queued, nothing decoded inside art_load */
    for (k = 0; k < TITLES; k++) {
        CHECK(art_pump(a) == 1);
        CHECK(g_finds == (int)k + 1); /* exactly one cover this turn */
        CHECK(art_tile(a, keys[k]) != NULL);
        CHECK(k + 1 == TITLES || art_tile(a, keys[k + 1]) == NULL); /* in queue order */
    }
    CHECK(!art_busy(a) && art_pump(a) == 0);
    art_close(a);
    if (g_fail == 0) printf("art_per_turn_ok\n");
    return g_fail != 0;
}
