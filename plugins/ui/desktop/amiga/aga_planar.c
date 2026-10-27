/* Pure helpers of the aga backend (I439, M4-P6-1); see aga_planar.h. Integer only, no Amiga headers. */
#include "aga_planar.h"

#include <string.h>

static uint32_t join_closest(desk_aga_band* b, uint32_t n) {
    uint32_t best = 0;
    int32_t best_gap = 0x7fffffff;
    for (uint32_t i = 0; i + 1 < n; i++) {
        int32_t g = b[i + 1].y - (b[i].y + b[i].h);
        if (g < best_gap) {
            best_gap = g;
            best = i;
        }
    }
    b[best].h = b[best + 1].y + b[best + 1].h - b[best].y;
    memmove(&b[best + 1], &b[best + 2], (n - best - 2) * sizeof b[0]);
    return n - 1;
}

#define BAND_CAP (DESK_AGA_BANDS_MAX * 2)

/* Inserts [y, y + h) into the sorted, disjoint list b (n bands, room for n + 1), joining overlaps and gaps <= gap. */
static uint32_t insert_band(desk_aga_band* b, uint32_t n, int32_t y, int32_t h, int32_t gap) {
    uint32_t i = 0;
    int32_t y1 = y + h;
    while (i < n && b[i].y + b[i].h + gap < y) {
        i++;
    }
    uint32_t j = i;
    while (j < n && b[j].y <= y1 + gap) {
        if (b[j].y < y) {
            y = b[j].y;
        }
        if (b[j].y + b[j].h > y1) {
            y1 = b[j].y + b[j].h;
        }
        j++;
    }
    if (j > i) { /* bands i .. j-1 collapse into one at i */
        memmove(&b[i + 1], &b[j], (n - j) * sizeof b[0]);
        n -= j - i - 1;
    } else {
        memmove(&b[i + 1], &b[i], (n - i) * sizeof b[0]);
        n++;
    }
    b[i].y = y;
    b[i].h = y1 - y;
    return n;
}

static uint32_t clamp_max(uint32_t max) { return max == 0 ? 1 : (max > BAND_CAP ? BAND_CAP : max); }

uint32_t desk_aga_bands(const sel_gui_rect* r, uint32_t n, int32_t h, int32_t gap, desk_aga_band* out, uint32_t max) {
    desk_aga_band tmp[BAND_CAP + 1];
    uint32_t nb = 0;
    max = clamp_max(max);
    for (uint32_t k = 0; k < n; k++) {
        int32_t y0 = r[k].y < 0 ? 0 : r[k].y;
        int32_t y1 = r[k].y + r[k].h > h ? h : r[k].y + r[k].h;
        if (r[k].w <= 0 || y1 <= y0) {
            continue;
        }
        nb = insert_band(tmp, nb, y0, y1 - y0, gap);
        if (nb > max) {
            nb = join_closest(tmp, nb);
        }
    }
    memcpy(out, tmp, nb * sizeof tmp[0]);
    return nb;
}

uint32_t desk_aga_bands_union(desk_aga_band* a, uint32_t na, const desk_aga_band* b, uint32_t nb, int32_t gap,
                              uint32_t max) {
    desk_aga_band tmp[BAND_CAP + 1];
    max = clamp_max(max);
    if (na > max) {
        na = max;
    }
    memcpy(tmp, a, na * sizeof tmp[0]);
    for (uint32_t k = 0; k < nb; k++) {
        na = insert_band(tmp, na, b[k].y, b[k].h, gap);
        if (na > max) {
            na = join_closest(tmp, na);
        }
    }
    memcpy(a, tmp, na * sizeof tmp[0]);
    return na;
}

uint32_t desk_aga_band_rows(const desk_aga_band* b, uint32_t n) {
    uint32_t rows = 0;
    for (uint32_t i = 0; i < n; i++) {
        rows += (uint32_t)b[i].h;
    }
    return rows;
}

void desk_aga_c2p_ref(const uint8_t* chunky, int32_t w, int32_t y, int32_t rows, uint8_t* planes, uint32_t bplsize) {
    int32_t rb = w / 8;
    for (int32_t row = y; row < y + rows; row++) {
        const uint8_t* src = chunky + (size_t)row * (size_t)w;
        uint8_t* dst = planes + (size_t)row * (size_t)rb;
        for (int32_t bx = 0; bx < rb; bx++) {
            uint8_t out[DESK_AGA_DEPTH] = {0};
            for (int32_t px = 0; px < 8; px++) {
                uint8_t v = src[bx * 8 + px];
                for (int32_t p = 0; p < DESK_AGA_DEPTH; p++) {
                    out[p] = (uint8_t)(out[p] | (((v >> p) & 1u) << (7 - px)));
                }
            }
            for (int32_t p = 0; p < DESK_AGA_DEPTH; p++) {
                dst[(size_t)p * bplsize + (size_t)bx] = out[p];
            }
        }
    }
}

uint32_t desk_aga_c2p_pick(uint32_t attn_flags, uint32_t bplsize) {
    if (attn_flags & ((1u << 3) | (1u << 7))) return 1u; /* DESK_AGA_C2P_KALMS_040 */
    if (attn_flags & ((1u << 1) | (1u << 2))) {
        /* The 030 routine addresses the planes with 16-bit displacements patched from bplsize (BPLSIZE*2(a1),
         * -BPLSIZE-4(a1)); its header limits bplsize to 16 KB. A 640x256 plane is 20480 bytes, so BPLSIZE*2 wraps to
         * -24576 and planes 2 and up land before the buffer. The 040 routine keeps long deltas and runs on any 68020. */
        return bplsize <= DESK_AGA_C2P_030_MAX_BPLSIZE ? 2u /* DESK_AGA_C2P_KALMS_030 */ : 1u;
    }
    return 0u; /* DESK_AGA_C2P_REF */
}

void desk_aga_p2c_ref(const uint8_t* planes, uint32_t bplsize, int32_t w, int32_t h, uint8_t* chunky) {
    int32_t rb = w / 8;
    for (int32_t row = 0; row < h; row++) {
        for (int32_t x = 0; x < w; x++) {
            uint8_t v = 0;
            size_t at = (size_t)row * (size_t)rb + (size_t)(x / 8);
            for (int32_t p = 0; p < DESK_AGA_DEPTH; p++) {
                v = (uint8_t)(v | (((planes[(size_t)p * bplsize + at] >> (7 - (x & 7))) & 1u) << p));
            }
            chunky[(size_t)row * (size_t)w + (size_t)x] = v;
        }
    }
}

size_t desk_aga_buffer_bytes(int32_t w, int32_t h) { return (size_t)w * (size_t)h; }

size_t desk_aga_chip_bytes(int32_t w, int32_t h) { return 2u * desk_aga_buffer_bytes(w, h); }

void desk_aga_rgb32_table(const uint32_t* rgb, uint32_t first, uint32_t n, uint32_t* out) {
    out[0] = (n << 16) | first;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t c = rgb[i];
        out[1 + i * 3 + 0] = ((c >> 16) & 0xffu) * 0x01010101u;
        out[1 + i * 3 + 1] = ((c >> 8) & 0xffu) * 0x01010101u;
        out[1 + i * 3 + 2] = (c & 0xffu) * 0x01010101u;
    }
    out[1 + n * 3] = 0;
}

void desk_aga_copper_regs(uint32_t pen, uint32_t rgb, uint32_t out[4][2]) {
    uint32_t r = (rgb >> 16) & 255u, g = (rgb >> 8) & 255u, b = rgb & 255u;
    uint32_t bplcon3 = DESK_AGA_BPLCON3 | ((pen >> 5) << 13), reg = 0x180u + 2u * (pen & 31u);
    out[0][0] = 0x106u;
    out[0][1] = bplcon3;
    out[1][0] = reg;
    out[1][1] = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
    out[2][0] = 0x106u;
    out[2][1] = bplcon3 | 0x200u;
    out[3][0] = reg;
    out[3][1] = ((r & 15u) << 8) | ((g & 15u) << 4) | (b & 15u);
}

int desk_aga_mode_is_native(uint32_t mode_id) {
    return mode_id != 0xFFFFFFFFu && (mode_id & 0xFFF00000u) == 0u;
}

void desk_aga_fit_size(int32_t max_w, int32_t max_h, int ntsc, int32_t* w, int32_t* h) {
    int32_t lines = ntsc ? 200 : 256;
    int32_t fw = max_w & ~31, fh = max_h & ~7;
    *w = fw >= 640 ? fw : 640;
    *h = fh >= lines ? fh : lines;
}

void desk_aga_fit_clip(const int32_t max_rect[4], int32_t w, int32_t h, int32_t out[4]) {
    out[0] = max_rect[0] + ((max_rect[2] - max_rect[0] + 1) - w) / 2;
    out[1] = max_rect[1] + ((max_rect[3] - max_rect[1] + 1) - h) / 2;
    out[2] = out[0] + w - 1;
    out[3] = out[1] + h - 1;
}

void desk_aga_overscan_area(int overscan, int32_t* max_w, int32_t* max_h) {
    if (!overscan) {
        *max_w = 0;
        *max_h = 0;
    }
}

int desk_aga_border_blank_tags(uint32_t tags[4]) {
    tags[0] = DESK_AGA_VTAG_BORDERBLANK_SET;
    tags[1] = 0u;
    tags[2] = 0u; /* TAG_DONE */
    tags[3] = 0u;
    return 3;
}

int desk_aga_pick_size(int32_t* w, int32_t* h, int ntsc, int32_t max_w, int32_t max_h) {
    int32_t lines = ntsc ? 200 : 256, fw, fh;
    desk_aga_fit_size(max_w, max_h, ntsc, &fw, &fh);
    if (*w <= 0 || *h <= 0) {
        *w = fw;
        *h = fh;
    }
    if (*h == lines && *w == 320) return 0;
    if ((*h == lines && *w == 640) || (*h == fh && *w == fw)) return 1;
    return -1;
}
