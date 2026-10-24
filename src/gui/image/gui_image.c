/* Artwork scaling, thumbnail packing and the artwork cache (design doc 2.7, DUI-P2-4). Integer only. */
#include <selecta/gui/gui_image.h>

#include <selecta/gui/gui_draw.h>

#include <stdlib.h>
#include <string.h>

void sel_gui_image_scale(const uint32_t* src, int32_t sw, int32_t sh, uint32_t* dst, int32_t dw, int32_t dh) {
    sel_gui_canvas c;
    sel_gui_rect r;

    if (src == NULL || dst == NULL || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
        return;
    }
    memset(dst, 0, (size_t)dw * (size_t)dh * sizeof *dst);
    sel_gui_canvas_init(&c, SEL_GUI_RGBA32, dw, dh, dst);
    r.x = 0;
    r.y = 0;
    r.w = dw;
    r.h = dh;
    sel_gui_blit_image(&c, r, src, sw, sh, sw, 1); /* the blit's box filter is the one scaler */
}

static void put_le(uint8_t* p, uint32_t v, unsigned n) {
    unsigned i;
    for (i = 0; i < n; i++) {
        p[i] = (uint8_t)(v >> (8u * i));
    }
}

static uint32_t get_le(const uint8_t* p, unsigned n) {
    uint32_t v = 0u;
    unsigned i;
    for (i = 0; i < n; i++) {
        v |= (uint32_t)p[i] << (8u * i);
    }
    return v;
}

uint32_t sel_gui_thumb_pack(const uint32_t* px, int32_t w, int32_t h, uint8_t* out, uint32_t cap) {
    uint32_t n, i, total;

    if (w <= 0 || h <= 0 || w > 65535 || h > 65535) {
        return 0u;
    }
    n = (uint32_t)w * (uint32_t)h;
    total = SEL_GUI_THUMB_HEAD + 4u * n;
    if (total > cap) {
        return 0u;
    }
    memcpy(out, "THM1", 4);
    put_le(out + 4, (uint32_t)w, 2);
    put_le(out + 6, (uint32_t)h, 2);
    for (i = 0; i < n; i++) {
        put_le(out + SEL_GUI_THUMB_HEAD + 4u * i, px[i], 4);
    }
    return total;
}

int sel_gui_thumb_unpack(const uint8_t* in, uint32_t len, uint32_t* px, uint32_t cap_px, int32_t* w, int32_t* h) {
    uint32_t tw, th, n, i;

    if (len < SEL_GUI_THUMB_HEAD || memcmp(in, "THM1", 4) != 0) {
        return 0;
    }
    tw = get_le(in + 4, 2);
    th = get_le(in + 6, 2);
    n = tw * th;
    if (tw == 0u || th == 0u || n > cap_px || len != SEL_GUI_THUMB_HEAD + 4u * n) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        px[i] = get_le(in + SEL_GUI_THUMB_HEAD + 4u * i, 4);
    }
    *w = (int32_t)tw;
    *h = (int32_t)th;
    return 1;
}

typedef struct art_pool {
    uint32_t edge;
    uint32_t slots;
    uint32_t* pixels;
    int64_t* key;
    uint32_t* used; /* tick of the last use, 0 = free */
} art_pool;

struct sel_gui_art_cache {
    art_pool pool[SEL_GUI_ART_SIZES];
    uint32_t tick;
    uint32_t hits;
    uint32_t misses;
};

void sel_gui_art_cache_close(sel_gui_art_cache* c) {
    int i;

    if (c == NULL) {
        return;
    }
    for (i = 0; i < SEL_GUI_ART_SIZES; i++) {
        free(c->pool[i].pixels);
        free(c->pool[i].key);
        free(c->pool[i].used);
    }
    free(c);
}

sel_gui_art_cache* sel_gui_art_cache_open(const uint16_t edge[SEL_GUI_ART_SIZES], const uint8_t slots[SEL_GUI_ART_SIZES]) {
    sel_gui_art_cache* c = (sel_gui_art_cache*)calloc(1, sizeof *c);
    int i;

    if (c == NULL) {
        return NULL;
    }
    for (i = 0; i < SEL_GUI_ART_SIZES; i++) {
        art_pool* p = &c->pool[i];

        p->edge = edge[i];
        p->slots = slots[i];
        p->pixels = (uint32_t*)calloc((size_t)p->slots * p->edge * p->edge + 1u, sizeof(uint32_t));
        p->key = (int64_t*)calloc(p->slots + 1u, sizeof(int64_t));
        p->used = (uint32_t*)calloc(p->slots + 1u, sizeof(uint32_t));
        if (p->pixels == NULL || p->key == NULL || p->used == NULL) {
            sel_gui_art_cache_close(c);
            return NULL;
        }
    }
    return c;
}

const uint32_t* sel_gui_art_cache_get(sel_gui_art_cache* c, int size, int64_t key) {
    art_pool* p = &c->pool[size];
    uint32_t i;

    for (i = 0; i < p->slots; i++) {
        if (p->used[i] != 0u && p->key[i] == key) {
            p->used[i] = ++c->tick;
            c->hits++;
            return p->pixels + (size_t)i * p->edge * p->edge;
        }
    }
    c->misses++;
    return NULL;
}

uint32_t* sel_gui_art_cache_put(sel_gui_art_cache* c, int size, int64_t key) {
    art_pool* p = &c->pool[size];
    uint32_t i, victim = 0u;

    for (i = 0; i < p->slots; i++) {
        if (p->used[i] == 0u) {
            victim = i;
            break;
        }
        if (p->used[i] < p->used[victim]) {
            victim = i;
        }
    }
    p->key[victim] = key;
    p->used[victim] = ++c->tick;
    return p->pixels + (size_t)victim * p->edge * p->edge;
}

uint32_t sel_gui_art_cache_hits(const sel_gui_art_cache* c) {
    return c->hits;
}

uint32_t sel_gui_art_cache_misses(const sel_gui_art_cache* c) {
    return c->misses;
}
