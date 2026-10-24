/* Generated artwork per format and chip (design doc 2.7, DUI-P2-5). Integer only. */
#include <selecta/gui/gui_genart.h>

#include <string.h>

uint32_t sel_gui_str_hash(const char* s) {
    uint32_t h = 2166136261u;

    for (; *s != '\0'; s++) {
        h = (h ^ (uint8_t)*s) * 16777619u;
    }
    return h;
}

typedef struct chip_row {
    const char* format;
    const char* chip;
} chip_row;

static const chip_row k_chips[] = {{"mod", "paula"}, {"ahx", "paula"}, {"hvl", "paula"}, {"sid", "sid"},
                                   {"ym", "ay"},     {"nsf", "2a03"},  {"xm", "software"}, {"it", "software"},
                                   {"s3m", "software"}};

static void lower(char* dst, const char* src, size_t cap) {
    size_t i;

    for (i = 0; i + 1u < cap && src[i] != '\0'; i++) {
        dst[i] = (src[i] >= 'A' && src[i] <= 'Z') ? (char)(src[i] + 32) : src[i];
    }
    dst[i] = '\0';
}

const char* sel_gui_genart_chip(const char* format) {
    char f[16];
    size_t i;

    lower(f, format, sizeof f);
    for (i = 0; i < sizeof k_chips / sizeof k_chips[0]; i++) {
        if (strcmp(k_chips[i].format, f) == 0) {
            return k_chips[i].chip;
        }
    }
    return "software";
}

/* hue 0..1535 (six regions of 256), saturation and value 0..255. */
static uint32_t hsv(uint32_t hue, uint32_t s, uint32_t v) {
    uint32_t region = (hue / 256u) % 6u, f = hue % 256u;
    uint32_t p = v * (255u - s) / 255u;
    uint32_t q = v * (255u - s * f / 255u) / 255u;
    uint32_t t = v * (255u - s * (255u - f) / 255u) / 255u;
    uint32_t r, g, b;

    switch (region) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

void sel_gui_genart_draw(sel_gui_canvas* c, sel_gui_rect r, const char* format, uint32_t title_hash) {
    char f[16];
    uint32_t fh, ch, hue, a, b, bits;
    int32_t cell, ox, oy, x, y;
    sel_gui_rect g;

    lower(f, format, sizeof f);
    fh = sel_gui_str_hash(f);
    ch = sel_gui_str_hash(sel_gui_genart_chip(f));
    hue = (ch % 1536u + (title_hash >> 8) % 192u) % 1536u; /* the chip sets the colour family, the title nudges it */
    a = hsv(hue, 170u + (fh & 31u), 70u + (title_hash & 31u));
    b = hsv((hue + 128u + (fh >> 5) % 128u) % 1536u, 200u, 200u + ((title_hash >> 16) & 31u));
    sel_gui_gradient(c, r, 1, a, b);

    /* The glyph: a mirrored 5x5 pattern from the format hash, centred. */
    cell = r.w / 8;
    if (cell < 1) {
        return;
    }
    ox = r.x + (r.w - 5 * cell) / 2;
    oy = r.y + (r.h - 5 * cell) / 2;
    bits = fh ^ (fh >> 15);
    for (y = 0; y < 5; y++) {
        for (x = 0; x < 3; x++) {
            if (((bits >> (uint32_t)(y * 3 + x)) & 1u) != 0u) {
                g.y = oy + y * cell;
                g.w = cell;
                g.h = cell;
                g.x = ox + x * cell;
                sel_gui_fill_rect(c, g, 0xFFF4F4F4u);
                g.x = ox + (4 - x) * cell;
                if (x != 2) {
                    sel_gui_fill_rect(c, g, 0xFFF4F4F4u);
                }
            }
        }
    }
}
