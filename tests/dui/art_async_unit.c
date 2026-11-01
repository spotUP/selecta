/* I906c: art_load never joins a thread that waits on the library (ledger row I906c, design
 * thoughts/shared/research/2026-11-01-ui-thread-library-waits.md section 2b/4).
 *
 * The fixture's art table plays a busy library worker: find() sleeps FIND_MS per title, like a request queued behind a sync job.
 * Through the real entry points (art_load, art_known, art_tile, art_pump) the test asserts
 *   - art_load returns within BUDGET_MS although the lookups take seconds in total (the old start-then-join blocked for all of them),
 *   - the lookups ran on another thread (the sentinel that the background batch ran, not an inline call),
 *   - nothing is in the tile table while the batch runs, and art_known already answers 1 for a queued title (no second request),
 *   - every art_pump call returns within BUDGET_MS (worst measured and printed),
 *   - after the worker finished art_pump merges: the tile pixels equal the thumb blob's, art_batches advanced, art_decodes is 0
 *     (the thumb row served the tile),
 *   - 70 titles split into batches of at most 32 (the cache slot count): 3 batches merge, the newest 32 tiles are in the cache. */
#include "art.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum { FIND_MS = 150, BUDGET_MS = 50, TITLES = 70 };

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static pthread_t g_main;
static volatile int g_off_thread, g_on_main;
static uint8_t g_blob[SEL_GUI_THUMB_HEAD + 4u * DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE];
static uint32_t g_blob_len;

static long ms_since(const struct timespec* t0) {
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (long)((t1.tv_sec - t0->tv_sec) * 1000L + (t1.tv_nsec - t0->tv_nsec) / 1000000L);
}

/* Title "tNN" is album NN + 1; every album has the same thumb row. */
static int64_t fx_find(void* ctx, const char* ref) {
    (void)ctx;
    if (pthread_equal(pthread_self(), g_main)) g_on_main = 1; else g_off_thread = 1;
    usleep(FIND_MS * 1000);
    return ref[0] == 't' ? 1 + (int64_t)atoi(ref + 1) : 0;
}

static uint32_t fx_get(void* ctx, int64_t album, const char* kind, uint8_t* out, uint32_t cap) {
    (void)ctx;
    (void)album;
    if (strcmp(kind, "thumb144") != 0) return 0;
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
    static uint32_t px[DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE], want[DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE];
    static char names[TITLES][12];
    const char* keys[TITLES];
    sel_ui_art src = {NULL, fx_find, fx_get, fx_put};
    struct timespec t0;
    desk_art* a;
    long ms, worst_pump = 0, load_ms;
    int32_t w = 0, h = 0, i;
    uint32_t k;

    g_main = pthread_self();
    for (i = 0; i < DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE; i++) px[i] = 0xFF000000u | (uint32_t)(i * 2654435761u >> 8);
    g_blob_len = sel_gui_thumb_pack(px, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, g_blob, sizeof g_blob);
    CHECK(g_blob_len > 0u);
    CHECK(sel_gui_thumb_unpack(g_blob, g_blob_len, want, DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE, &w, &h));
    for (k = 0; k < TITLES; k++) {
        snprintf(names[k], sizeof names[k], "t%u", (unsigned)k);
        keys[k] = names[k];
    }

    a = art_open(&src);
    CHECK(a != NULL);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    art_load(a, keys, TITLES);
    load_ms = ms_since(&t0);
    printf("i906c art_load of %d titles with a %d ms lookup each: returned in %ld ms (budget %d)\n", TITLES, FIND_MS, load_ms, BUDGET_MS);
    CHECK(load_ms < BUDGET_MS);
    CHECK(art_busy(a));
    CHECK(art_known(a, "t0") && art_known(a, "t69")); /* queued: the caller does not ask again */
    CHECK(art_tile(a, "t0") == NULL);                 /* nothing was computed on this thread */

    /* The batches land one pump at a time; no pump may take long. */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (art_busy(a) && ms_since(&t0) < 20000) {
        struct timespec p0;
        clock_gettime(CLOCK_MONOTONIC, &p0);
        (void)art_pump(a);
        ms = ms_since(&p0);
        if (ms > worst_pump) worst_pump = ms;
        usleep(2000);
    }
    printf("i906c worst art_pump %ld ms (budget %d), batches merged %u, lookups off the calling thread %d, on it %d\n", worst_pump,
           BUDGET_MS, (unsigned)art_batches(a), g_off_thread, g_on_main);
    CHECK(worst_pump < BUDGET_MS);
    CHECK(g_off_thread == 1 && g_on_main == 0);
    CHECK(art_batches(a) == 3u); /* 70 titles: 32 + 32 + 6 */
    CHECK(!art_busy(a));
    CHECK(art_decodes(a) == 0u); /* the thumb row served every tile */
    for (k = TITLES - 32u; k < TITLES; k++) { /* the cache holds 32 tiles: the last batches stay, the first one was evicted */
        const uint32_t* t = art_tile(a, keys[k]);
        CHECK(t != NULL);
        if (t == NULL) break;
        CHECK(memcmp(t, want, sizeof want) == 0);
    }
    art_close(a);
    if (g_fail == 0) printf("art_async_ok\n");
    return g_fail != 0;
}
