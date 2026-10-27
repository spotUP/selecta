/* Copper tint of the 1-bit covers (MUI-P5-8); see mui_copper.h. */
#include "mui_copper.h"

#include <stddef.h>

void mui_copper_row_colors(const uint32_t* px, int32_t w, int32_t h, uint32_t* rows) {
    int32_t x, y;
    for (y = 0; y < h; y++) {
        uint32_t r = 0, g = 0, b = 0;
        for (x = 0; x < w; x++) {
            uint32_t p = px[(size_t)y * (size_t)w + (size_t)x];
            r += (p >> 16) & 0xFFu;
            g += (p >> 8) & 0xFFu;
            b += p & 0xFFu;
        }
        rows[y] = w > 0 ? (((r / (uint32_t)w) << 16) | ((g / (uint32_t)w) << 8) | (b / (uint32_t)w)) : 0u;
    }
}

static uint16_t rgb4_of(uint32_t c) { return (uint16_t)(((c >> 20) & 0xFu) << 8 | ((c >> 12) & 0xFu) << 4 | ((c >> 4) & 0xFu)); }

uint32_t mui_copper_cover_list(const uint32_t* rows, int32_t h, int32_t top, mui_copper_entry* out, uint32_t cap) {
    uint32_t n = 0;
    int32_t y;
    for (y = 0; y < h && n < cap; y++) {
        uint16_t c = rgb4_of(rows[y]);
        if (n == 0u || out[n - 1u].rgb4 != c) {
            out[n].y = top + y;
            out[n].rgb4 = c;
            n++;
        }
    }
    return n;
}

uint32_t mui_copper_grid_row_list(int grid_tint, const uint32_t* row_colors, int32_t th, int32_t top, mui_copper_entry* out,
                                  uint32_t cap) {
    return grid_tint ? mui_copper_cover_list(row_colors, th, top, out, cap) : 0u;
}
