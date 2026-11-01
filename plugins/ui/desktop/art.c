/* Album artwork of the desktop UI (design doc 2.7, DUI-P2-4). See art.h. */
#include "art.h"

#include <selecta/engine.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ART_TITLES 256u
#define ART_TITLE_MAX 512u /* a queue path */
#define ART_THUMB_MAX (SEL_GUI_THUMB_HEAD + 4u * DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE)
#define ART_BATCH 32u /* the tile cache slot count (SEL_GUI_ART_TILE): a batch cannot hold more tiles than the cache */

typedef struct art_entry {
    char title[ART_TITLE_MAX];
    int64_t album; /* 0 = no such album */
    uint32_t dominant; /* I678e: the cover's dominant colour, 0xAARRGGBB */
    uint8_t dominant_ready;
} art_entry;

/* I906c: one background batch. The worker reads `src`, `title` and writes only the result arrays; the caller thread owns the entry
 * table and the cache and merges the results after the worker finished (art_pump). The UI thread never joins a thread that waits
 * on the library worker. */
typedef struct art_job {
    const sel_ui_art* src;
    char* title[ART_BATCH];
    int64_t album[ART_BATCH]; /* 0: no album, or one without a usable cover */
    uint32_t* px[ART_BATCH];  /* the tile of album[i] (DESK_ART_TILE_EDGE squared), shared by the titles of one album */
    uint8_t owns[ART_BATCH];  /* px[i] is this title's own allocation */
    uint32_t n;
    int64_t bad[ART_TITLES + ART_BATCH]; /* albums without a usable cover: the table at the start plus the ones found here */
    uint32_t bad_n, bad_start;
    uint32_t decodes;
    uint32_t gen; /* the table generation the job started in (art_forget_misses moves it) */
} art_job;

struct desk_art {
    const sel_ui_art* src;
    sel_gui_art_cache* cache;
    art_entry entry[ART_TITLES];
    uint32_t count;
    int64_t bad[ART_TITLES]; /* albums whose cover failed to load */
    uint32_t bad_n;
    uint32_t decodes;
    uint32_t dominants; /* I678e: dominant colours computed (once per art entry, not per frame) */
    uint32_t gen;       /* bumped by art_forget_misses: the misses of a running job are stale */
    uint32_t merged;    /* I906c: batches merged (art_batches) */
    art_job* job;       /* the running batch, NULL when idle */
    void* th;           /* its thread handle; NULL when the batch ran inline */
    char** pend;        /* titles waiting for the next batch (copies) */
    uint32_t pend_n, pend_cap;
};

desk_art* art_open(const sel_ui_art* src) {
    static const uint16_t edge[SEL_GUI_ART_SIZES] = {DESK_ART_TILE_EDGE, 256, 192, 16};
    static const uint8_t slots[SEL_GUI_ART_SIZES] = {ART_BATCH, 4, 8, 8};
    desk_art* a;

    if (src == NULL) {
        return NULL;
    }
    a = (desk_art*)calloc(1, sizeof *a);
    if (a == NULL) {
        return NULL;
    }
    a->src = src;
    a->cache = sel_gui_art_cache_open(edge, slots);
    if (a->cache == NULL) {
        free(a);
        return NULL;
    }
    return a;
}

static void job_free(art_job* j) {
    uint32_t i;
    for (i = 0; i < j->n; i++) {
        free(j->title[i]);
        if (j->owns[i]) {
            free(j->px[i]);
        }
    }
    free(j);
}

void art_close(desk_art* a) {
    uint32_t i;
    if (a == NULL) {
        return;
    }
    if (a->job != NULL) {
        art_thread_join(a->th); /* shutdown: the batch is dropped, not merged */
        job_free(a->job);
    }
    for (i = 0; i < a->pend_n; i++) {
        free(a->pend[i]);
    }
    free(a->pend);
    sel_gui_art_cache_close(a->cache);
    free(a);
}

static int is_bad(const int64_t* bad, uint32_t n, int64_t album) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        if (bad[i] == album) {
            return 1;
        }
    }
    return 0;
}

static art_entry* find_entry(desk_art* a, const char* title) {
    uint32_t i;
    for (i = 0; i < a->count; i++) {
        if (strcmp(a->entry[i].title, title) == 0) {
            return &a->entry[i];
        }
    }
    return NULL;
}

/* Fetches the blob (kind) of the album into a new buffer. */
static uint8_t* fetch(const sel_ui_art* src, int64_t album, const char* kind, uint32_t* len) {
    uint8_t* buf;
    *len = src->get(src->ctx, album, kind, NULL, 0);
    if (*len == 0u) {
        return NULL;
    }
    buf = (uint8_t*)malloc(*len);
    if (buf != NULL && src->get(src->ctx, album, kind, buf, *len) != *len) {
        free(buf);
        return NULL;
    }
    return buf;
}

/* Worker side. Thumb row first; else decode the cover, scale it to the tile size and write the thumb row back. Fills `px`
 * (DESK_ART_TILE_EDGE squared); 0 when the album has no usable cover. */
static int load_album(art_job* j, int64_t album, const char* thumb_kind, uint32_t* px) {
    uint32_t len = 0u;
    uint8_t* blob;
    int32_t w = 0, h = 0;

    blob = fetch(j->src, album, thumb_kind, &len);
    if (blob != NULL) {
        int ok = sel_gui_thumb_unpack(blob, len, px, DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE, &w, &h) &&
                 w == DESK_ART_TILE_EDGE && h == DESK_ART_TILE_EDGE;
        free(blob);
        if (ok) {
            return 1;
        }
    }
    blob = fetch(j->src, album, "cover", &len);
    if (blob != NULL) {
        uint32_t* img = desk_image_decode(blob, len, &w, &h);
        free(blob);
        if (img != NULL) {
            int32_t side = w < h ? w : h;
            uint32_t* sq = (uint32_t*)malloc((size_t)side * (size_t)side * sizeof *sq);
            uint8_t* out = (uint8_t*)malloc(ART_THUMB_MAX);
            int32_t y;
            j->decodes++;
            if (sq != NULL && out != NULL) {
                for (y = 0; y < side; y++) { /* the centred square of the cover */
                    memcpy(sq + (size_t)y * (size_t)side, img + (size_t)(y + (h - side) / 2) * (size_t)w + (size_t)((w - side) / 2),
                           (size_t)side * sizeof *sq);
                }
                sel_gui_image_scale(sq, side, side, px, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE);
                len = sel_gui_thumb_pack(px, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, out, ART_THUMB_MAX);
                if (len > 0u) {
                    j->src->put(j->src->ctx, album, thumb_kind, out, len);
                }
            }
            free(sq);
            free(out);
            free(img);
            return 1;
        }
    }
    return 0;
}

static void worker(void* ctx) {
    art_job* j = (art_job*)ctx;
    char kind[24];
    uint32_t i, k;

    snprintf(kind, sizeof kind, "thumb%d", DESK_ART_TILE_EDGE);
    for (i = 0; i < j->n; i++) {
        int64_t album = j->src->find(j->src->ctx, j->title[i]);
        j->album[i] = 0;
        if (album == 0 || is_bad(j->bad, j->bad_n, album)) { /* a bad album: another title already found it without a usable cover */
            continue;
        }
        for (k = 0; k < i; k++) { /* one tile per album in a batch */
            if (j->album[k] == album) {
                j->album[i] = album;
                j->px[i] = j->px[k];
                break;
            }
        }
        if (j->album[i] == album) {
            continue;
        }
        j->px[i] = (uint32_t*)calloc((size_t)DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE, sizeof(uint32_t));
        if (j->px[i] == NULL) {
            continue;
        }
        j->owns[i] = 1u;
        if (load_album(j, album, kind, j->px[i])) {
            j->album[i] = album;
        } else {
            free(j->px[i]);
            j->px[i] = NULL;
            j->owns[i] = 0u;
            if (j->bad_n < sizeof j->bad / sizeof j->bad[0]) {
                j->bad[j->bad_n++] = album; /* an album without a usable cover (I469) */
            }
        }
    }
}

/* Caller side: moves the finished batch into the entry table and the cache. */
static void job_merge(desk_art* a) {
    art_job* j = a->job;
    uint32_t i;

    for (i = 0; i < j->n; i++) {
        art_entry* e;
        if (j->album[i] == 0 && j->gen != a->gen) {
            continue; /* a miss from before art_forget_misses: it is looked up again */
        }
        if (find_entry(a, j->title[i]) != NULL || a->count >= ART_TITLES) {
            continue;
        }
        e = &a->entry[a->count++];
        memset(e, 0, sizeof *e);
        strcpy(e->title, j->title[i]);
        e->album = j->album[i];
        if (e->album != 0 && sel_gui_art_cache_get(a->cache, SEL_GUI_ART_TILE, e->album) == NULL) {
            uint32_t* dst = sel_gui_art_cache_put(a->cache, SEL_GUI_ART_TILE, e->album);
            if (dst != NULL) {
                memcpy(dst, j->px[i], (size_t)DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE * sizeof *dst);
            } else {
                e->album = 0;
            }
        }
    }
    if (j->gen == a->gen) {
        for (i = j->bad_start; i < j->bad_n && a->bad_n < ART_TITLES; i++) {
            a->bad[a->bad_n++] = j->bad[i];
        }
    }
    a->decodes += j->decodes;
    a->merged++;
    job_free(j);
    a->job = NULL;
    a->th = NULL;
}

static int in_pending(const desk_art* a, const char* title) {
    uint32_t i;
    for (i = 0; i < a->pend_n; i++) {
        if (strcmp(a->pend[i], title) == 0) {
            return 1;
        }
    }
    if (a->job != NULL) {
        for (i = 0; i < a->job->n; i++) {
            if (strcmp(a->job->title[i], title) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

/* Starts the next batch from the waiting titles when none runs. */
static void job_start(desk_art* a) {
    art_job* j;
    uint32_t take, i;

    while (a->job == NULL && a->pend_n > 0u) {
        j = (art_job*)calloc(1, sizeof *j);
        if (j == NULL) {
            return;
        }
        take = a->pend_n < ART_BATCH ? a->pend_n : ART_BATCH;
        j->src = a->src;
        j->gen = a->gen;
        for (i = 0; i < take; i++) {
            j->title[i] = a->pend[i];
        }
        j->n = take;
        memmove(a->pend, a->pend + take, (a->pend_n - take) * sizeof *a->pend);
        a->pend_n -= take;
        memcpy(j->bad, a->bad, a->bad_n * sizeof *a->bad);
        j->bad_n = j->bad_start = a->bad_n;
        a->job = j;
        a->th = art_thread_start(worker, j);
        if (a->th == NULL) { /* no thread (Amiga): the batch ran inline */
            job_merge(a);
        }
    }
}

int art_known(desk_art* a, const char* title) {
    return a == NULL || find_entry(a, title) != NULL || a->count >= ART_TITLES || strlen(title) >= ART_TITLE_MAX || in_pending(a, title);
}

void art_forget_misses(desk_art* a) {
    uint32_t i, keep = 0u;
    if (a == NULL) {
        return;
    }
    for (i = 0; i < a->count; i++) {
        if (a->entry[i].album != 0) {
            if (keep != i) {
                a->entry[keep] = a->entry[i];
            }
            keep++;
        }
    }
    a->count = keep;
    a->bad_n = 0u;
    a->gen++;
}

void art_load(desk_art* a, const char* const* titles, uint32_t n) {
    uint32_t i;
    if (a == NULL) {
        return;
    }
    for (i = 0; i < n; i++) {
        char* c;
        if (art_known(a, titles[i])) {
            continue;
        }
        if (a->pend_n == a->pend_cap) {
            uint32_t cap = a->pend_cap != 0u ? a->pend_cap * 2u : 64u;
            char** p = (char**)realloc(a->pend, cap * sizeof *p);
            if (p == NULL) {
                break;
            }
            a->pend = p;
            a->pend_cap = cap;
        }
        c = (char*)malloc(strlen(titles[i]) + 1u);
        if (c == NULL) {
            break;
        }
        strcpy(c, titles[i]);
        a->pend[a->pend_n++] = c;
    }
    job_start(a);
}

int art_pump(desk_art* a) {
    if (a == NULL || a->job == NULL || !art_thread_done(a->th)) {
        return 0;
    }
    art_thread_join(a->th); /* finished: returns at once */
    job_merge(a);
    job_start(a);
    return 1;
}

int art_busy(const desk_art* a) {
    return a != NULL && (a->job != NULL || a->pend_n > 0u);
}

uint32_t art_batches(const desk_art* a) {
    return a != NULL ? a->merged : 0u;
}

const uint32_t* art_tile(desk_art* a, const char* title) {
    art_entry* e;
    if (a == NULL || (e = find_entry(a, title)) == NULL || e->album == 0) {
        return NULL;
    }
    return sel_gui_art_cache_get(a->cache, SEL_GUI_ART_TILE, e->album);
}

/* The dominant colour of a tile: the most populated bucket of a 4x4x4 colour histogram, ignoring near black and near white, as the mean
 * of the pixels in it. A tile of only black or white gives the plain mean. */
static uint32_t tile_dominant(const uint32_t* px) {
    uint32_t count[64] = {0}, sum[64][3] = {{0}}, all[3] = {0}, best = 0, bestn = 0, i, n = (uint32_t)DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE;
    for (i = 0; i < n; i++) {
        uint32_t c = px[i], r = (c >> 16) & 255u, g = (c >> 8) & 255u, b = c & 255u, hi = r > g ? (r > b ? r : b) : (g > b ? g : b);
        uint32_t lo = r < g ? (r < b ? r : b) : (g < b ? g : b), k = (r >> 6) * 16u + (g >> 6) * 4u + (b >> 6);
        all[0] += r;
        all[1] += g;
        all[2] += b;
        if (hi < 40u || lo > 215u) {
            continue;
        }
        count[k]++;
        sum[k][0] += r;
        sum[k][1] += g;
        sum[k][2] += b;
    }
    for (i = 0; i < 64u; i++) {
        if (count[i] > bestn) {
            bestn = count[i];
            best = i;
        }
    }
    if (bestn == 0u) {
        return 0xFF000000u | ((all[0] / n) << 16) | ((all[1] / n) << 8) | (all[2] / n);
    }
    return 0xFF000000u | ((sum[best][0] / bestn) << 16) | ((sum[best][1] / bestn) << 8) | (sum[best][2] / bestn);
}

int art_dominant(desk_art* a, const char* title, uint32_t* argb) {
    art_entry* e;
    const uint32_t* tile;
    if (a == NULL || (e = find_entry(a, title)) == NULL || e->album == 0 || (tile = art_tile(a, title)) == NULL) {
        return 0;
    }
    if (!e->dominant_ready) {
        e->dominant = tile_dominant(tile);
        e->dominant_ready = 1u;
        a->dominants++;
    }
    *argb = e->dominant;
    return 1;
}

uint32_t art_dominants(const desk_art* a) {
    return a != NULL ? a->dominants : 0u;
}

uint32_t art_decodes(const desk_art* a) {
    return a != NULL ? a->decodes : 0u;
}
