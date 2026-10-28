#include "mui_aa.h"

static int isqrt(int v) {
    int r = 0;
    while ((r + 1) * (r + 1) <= v) {
        r++;
    }
    return r;
}

int mui_aa_round_inset(int w, int h, int r, int y) {
    int dy2, half;
    if (w <= 0 || h <= 0 || y < 0 || y >= h) {
        return 0;
    }
    if (r > h / 2) {
        r = h / 2;
    }
    if (r > w / 2) {
        r = w / 2;
    }
    if (r <= 0) {
        return 0;
    }
    if (y >= h - r) {
        y = h - 1 - y; /* mirror the bottom rows onto the top */
    }
    if (y >= r) {
        return 0;
    }
    /* Row centre at distance (r - y - 0.5) above the circle centre, doubled to stay integer: d = 2r - 2y - 1. */
    dy2 = 2 * r - 2 * y - 1;
    half = isqrt(4 * r * r - dy2 * dy2); /* twice the half chord */
    return r - (half + 1) / 2;
}

int mui_aa_ramp_index(int coverage, int steps) {
    if (steps < 2) {
        return 0;
    }
    if (coverage < 0) {
        coverage = 0;
    }
    if (coverage > 255) {
        coverage = 255;
    }
    return (coverage * (steps - 1) + 127) / 255;
}

uint32_t mui_aa_ramp_rgb(uint32_t fg, uint32_t bg, int steps, int i) {
    uint32_t out = 0;
    int sh;
    if (steps < 2) {
        return bg;
    }
    if (i < 0) {
        i = 0;
    }
    if (i > steps - 1) {
        i = steps - 1;
    }
    for (sh = 16; sh >= 0; sh -= 8) {
        int f = (int)((fg >> sh) & 0xFFu);
        int b = (int)((bg >> sh) & 0xFFu);
        out |= (uint32_t)(b + (f - b) * i / (steps - 1)) << sh;
    }
    return out;
}

uint32_t mui_aa_pressed_rgb(uint32_t face) {
    uint32_t out = 0;
    int sh;
    uint32_t lum = ((face >> 16) & 0xFFu) * 2126u / 10u + ((face >> 8) & 0xFFu) * 7152u / 10u + (face & 0xFFu) * 722u / 10u;
    int light = lum > 128000u; /* lum runs 0..255000 */
    for (sh = 16; sh >= 0; sh -= 8) {
        int c = (int)((face >> sh) & 0xFFu);
        c = light ? c * 4 / 5 : c + (255 - c) / 5;
        out |= (uint32_t)c << sh;
    }
    return out;
}
