/* Album artwork of the desktop UI (design doc 2.7, DUI-P2-4). See art.h. */
#include "art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ART_TITLES 256u
#define ART_TITLE_MAX 512u /* a queue path */
#define ART_THUMB_MAX (SEL_GUI_THUMB_HEAD + 4u * DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE)

typedef struct art_entry {
    char title[ART_TITLE_MAX];
    int64_t album; /* 0 = no such album */
    uint32_t dominant; /* I678e: the cover's dominant colour, 0xAARRGGBB */
    uint8_t dominant_ready;
} art_entry;

struct desk_art {
    const sel_ui_art* src;
    sel_gui_art_cache* cache;
    art_entry entry[ART_TITLES];
    uint32_t count;
    int64_t bad[ART_TITLES]; /* albums whose cover failed to load */
    uint32_t bad_n;
    uint32_t decodes;
    uint32_t dominants; /* I678e: dominant colours computed (once per art entry, not per frame) */
    /* The current load: written by the caller before the worker starts, read by the worker, quiet after the
     * join. The cache is touched by one thread at a time. */
    const char* const* titles;
    uint32_t n;
};

desk_art* art_open(const sel_ui_art* src) {
    static const uint16_t edge[SEL_GUI_ART_SIZES] = {DESK_ART_TILE_EDGE, 256, 192, 16};
    static const uint8_t slots[SEL_GUI_ART_SIZES] = {32, 4, 8, 8};
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

void art_close(desk_art* a) {
    if (a != NULL) {
        sel_gui_art_cache_close(a->cache);
        free(a);
    }
}

static int is_bad(const desk_art* a, int64_t album) {
    uint32_t i;
    for (i = 0; i < a->bad_n; i++) {
        if (a->bad[i] == album) {
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

/* Thumb row first; else decode the cover, scale it to the tile size and write the thumb row back. */
static int load_album(desk_art* a, int64_t album, const char* thumb_kind) {
    uint32_t len = 0u;
    uint32_t* px = sel_gui_art_cache_get(a->cache, SEL_GUI_ART_TILE, album) != NULL
                       ? NULL
                       : sel_gui_art_cache_put(a->cache, SEL_GUI_ART_TILE, album);
    uint8_t* blob;
    int32_t w = 0, h = 0;

    if (px == NULL) {
        return 1; /* already cached */
    }
    blob = fetch(a->src, album, thumb_kind, &len);
    if (blob != NULL) {
        int ok = sel_gui_thumb_unpack(blob, len, px, DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE, &w, &h) &&
                 w == DESK_ART_TILE_EDGE && h == DESK_ART_TILE_EDGE;
        free(blob);
        if (ok) {
            return 1;
        }
    }
    blob = fetch(a->src, album, "cover", &len);
    if (blob != NULL) {
        uint32_t* img = desk_image_decode(blob, len, &w, &h);
        free(blob);
        if (img != NULL) {
            int32_t side = w < h ? w : h;
            uint32_t* sq = (uint32_t*)malloc((size_t)side * (size_t)side * sizeof *sq);
            uint8_t* out = (uint8_t*)malloc(ART_THUMB_MAX);
            int32_t y;
            a->decodes++;
            if (sq != NULL && out != NULL) {
                for (y = 0; y < side; y++) { /* the centred square of the cover */
                    memcpy(sq + (size_t)y * (size_t)side, img + (size_t)(y + (h - side) / 2) * (size_t)w + (size_t)((w - side) / 2),
                           (size_t)side * sizeof *sq);
                }
                sel_gui_image_scale(sq, side, side, px, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE);
                len = sel_gui_thumb_pack(px, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, out, ART_THUMB_MAX);
                if (len > 0u) {
                    a->src->put(a->src->ctx, album, thumb_kind, out, len);
                }
            }
            free(sq);
            free(out);
            free(img);
            return 1;
        }
    }
    memset(px, 0, (size_t)DESK_ART_TILE_EDGE * DESK_ART_TILE_EDGE * sizeof *px);
    return 0;
}

static void worker(void* ctx) {
    desk_art* a = (desk_art*)ctx;
    char kind[24];
    uint32_t i;

    snprintf(kind, sizeof kind, "thumb%d", DESK_ART_TILE_EDGE);
    for (i = 0; i < a->n; i++) {
        art_entry* e = find_entry(a, a->titles[i]);
        if (e == NULL && a->count < ART_TITLES && strlen(a->titles[i]) < ART_TITLE_MAX) {
            e = &a->entry[a->count++];
            strcpy(e->title, a->titles[i]);
            e->album = a->src->find(a->src->ctx, e->title);
            if (e->album != 0 && is_bad(a, e->album)) {
                e->album = 0; /* another title already found this album without a usable cover */
            } else if (e->album != 0 && !load_album(a, e->album, kind)) {
                if (a->bad_n < ART_TITLES) {
                    a->bad[a->bad_n++] = e->album; /* its cache slot stays zeroed and would read as cached for the next title (I469) */
                }
                e->album = 0; /* an album without a usable cover */
            }
        }
    }
}

int art_known(desk_art* a, const char* title) {
    return a == NULL || find_entry(a, title) != NULL || a->count >= ART_TITLES || strlen(title) >= ART_TITLE_MAX;
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
}

void art_load(desk_art* a, const char* const* titles, uint32_t n) {
    if (a == NULL) {
        return;
    }
    a->titles = titles;
    a->n = n;
    art_thread_join(art_thread_start(worker, a));
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
