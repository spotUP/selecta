/* 1-bit artwork cache for planar screens (MUI-P5-7, I509): memory slots plus one file per cover. */
#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sel_gui_bitart {
    uint16_t edge;
    uint8_t slots;
    uint32_t pitch, bytes, tick, dithers;
    char* dir;
    uint8_t* bits;
    int64_t* key;
    uint32_t* used;
};

void sel_gui_bitart_close(sel_gui_bitart* c) {
    if (c != NULL) {
        free(c->dir);
        free(c->bits);
        free(c->key);
        free(c->used);
        free(c);
    }
}

sel_gui_bitart* sel_gui_bitart_open(uint16_t edge, uint8_t slots, const char* dir) {
    sel_gui_bitart* c;
    if (edge == 0u || slots == 0u) {
        return NULL;
    }
    c = (sel_gui_bitart*)calloc(1, sizeof *c);
    if (c == NULL) {
        return NULL;
    }
    c->edge = edge;
    c->slots = slots;
    c->pitch = ((uint32_t)edge + 7u) / 8u;
    c->bytes = c->pitch * edge;
    c->bits = (uint8_t*)calloc((size_t)slots, c->bytes);
    c->key = (int64_t*)calloc(slots, sizeof *c->key);
    c->used = (uint32_t*)calloc(slots, sizeof *c->used);
    c->dir = dir != NULL ? strdup(dir) : NULL;
    if (c->bits == NULL || c->key == NULL || c->used == NULL || (dir != NULL && c->dir == NULL)) {
        sel_gui_bitart_close(c);
        return NULL;
    }
    return c;
}

static void bitart_path(const sel_gui_bitart* c, int64_t key, char* out, size_t cap) {
    (void)snprintf(out, cap, "%s/%llx.bit", c->dir, (unsigned long long)key);
}

/* File: "BIT1", edge as little-endian u16, then pitch * edge bytes. */
static int bitart_load(const sel_gui_bitart* c, int64_t key, uint8_t* dst) {
    char path[512], head[6];
    FILE* f;
    int ok;
    if (c->dir == NULL) {
        return 0;
    }
    bitart_path(c, key, path, sizeof path);
    f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    ok = fread(head, 1, sizeof head, f) == sizeof head && memcmp(head, "BIT1", 4) == 0 &&
         ((uint8_t)head[4] | ((uint32_t)(uint8_t)head[5] << 8)) == c->edge && fread(dst, 1, c->bytes, f) == c->bytes;
    fclose(f);
    return ok;
}

static void bitart_store(const sel_gui_bitart* c, int64_t key, const uint8_t* src) {
    char path[512], head[6] = {'B', 'I', 'T', '1', 0, 0};
    FILE* f;
    if (c->dir == NULL) {
        return;
    }
    head[4] = (char)(c->edge & 0xFFu);
    head[5] = (char)(c->edge >> 8);
    bitart_path(c, key, path, sizeof path);
    f = fopen(path, "wb");
    if (f != NULL) {
        (void)fwrite(head, 1, sizeof head, f);
        (void)fwrite(src, 1, c->bytes, f);
        fclose(f);
    }
}

const uint8_t* sel_gui_bitart_get(sel_gui_bitart* c, int64_t key, const uint32_t* px) {
    uint32_t i, victim = 0u;
    uint8_t* slot;
    for (i = 0; i < c->slots; i++) {
        if (c->used[i] != 0u && c->key[i] == key) {
            c->used[i] = ++c->tick;
            return c->bits + (size_t)i * c->bytes;
        }
    }
    for (i = 0; i < c->slots; i++) {
        if (c->used[i] == 0u) {
            victim = i;
            break;
        }
        if (c->used[i] < c->used[victim]) {
            victim = i;
        }
    }
    slot = c->bits + (size_t)victim * c->bytes;
    if (!bitart_load(c, key, slot)) {
        if (px == NULL || !sel_gui_dither_atkinson(px, c->edge, c->edge, slot, c->pitch)) {
            return NULL;
        }
        c->dithers++;
        bitart_store(c, key, slot);
    }
    c->key[victim] = key;
    c->used[victim] = ++c->tick;
    return slot;
}

uint32_t sel_gui_bitart_dithers(const sel_gui_bitart* c) { return c->dithers; }
uint32_t sel_gui_bitart_pitch(const sel_gui_bitart* c) { return c->pitch; }
