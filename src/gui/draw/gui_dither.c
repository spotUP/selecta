/* Atkinson dither to a 1-bit bitmap (MUI-P5-7). Integer only. */
#include <selecta/gui/gui_draw.h>

#include <stdlib.h>
#include <string.h>

uint32_t sel_gui_luma(uint32_t argb) {
    return (77u * ((argb >> 16) & 0xFFu) + 151u * ((argb >> 8) & 0xFFu) + 28u * (argb & 0xFFu)) >> 8;
}

/* Three error rows (this, next, the one after) with two cells of margin on both sides. */
int sel_gui_dither_atkinson(const uint32_t* px, int32_t w, int32_t h, uint8_t* out, uint32_t pitch) {
    int32_t x, y;
    size_t stride = (size_t)w + 4u;
    int16_t* err;
    int16_t* r0;
    int16_t* r1;
    int16_t* r2;
    if (w <= 0 || h <= 0 || pitch < ((uint32_t)w + 7u) / 8u) {
        return 0;
    }
    err = (int16_t*)calloc(stride * 3u, sizeof *err);
    if (err == NULL) {
        return 0;
    }
    r0 = err + 2;
    r1 = err + stride + 2;
    r2 = err + 2u * stride + 2;
    memset(out, 0, (size_t)pitch * (size_t)h);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int32_t v = (int32_t)sel_gui_luma(px[(size_t)y * (size_t)w + (size_t)x]) + r0[x];
            int32_t e;
            if (v >= 128) {
                out[(size_t)y * pitch + ((uint32_t)x >> 3)] |= (uint8_t)(0x80u >> ((uint32_t)x & 7u));
                e = (v - 255) / 8;
            } else {
                e = v / 8;
            }
            r0[x + 1] = (int16_t)(r0[x + 1] + e);
            r0[x + 2] = (int16_t)(r0[x + 2] + e);
            r1[x - 1] = (int16_t)(r1[x - 1] + e);
            r1[x] = (int16_t)(r1[x] + e);
            r1[x + 1] = (int16_t)(r1[x + 1] + e);
            r2[x] = (int16_t)(r2[x] + e);
        }
        memcpy(r0 - 2, r1 - 2, stride * sizeof *err);
        memcpy(r1 - 2, r2 - 2, stride * sizeof *err);
        memset(r2 - 2, 0, stride * sizeof *err);
    }
    free(err);
    return 1;
}
