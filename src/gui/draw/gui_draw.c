/* Integer rasterizer (plan DUI-P2-1) and dirty-rectangle list (DUI-P2-2). See gui_draw.h. */
#include "gui_draw.h"

#include <string.h>

static uint32_t div255(uint32_t x) {
    x += 128u;
    return (x + (x >> 8)) >> 8;
}

static int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }

static uint32_t isqrt(uint32_t v) {
    uint32_t r = 0u, bit = 1u << 30;
    while (bit > v) {
        bit >>= 2;
    }
    while (bit != 0u) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

size_t sel_gui_pixel_bytes(uint32_t format, int32_t w, int32_t h) {
    return (size_t)w * (size_t)h * (format == SEL_GUI_RGBA32 ? 4u : 1u);
}

void sel_gui_canvas_init(sel_gui_canvas* c, uint32_t format, int32_t w, int32_t h, void* pixels) {
    memset(c, 0, sizeof *c);
    c->format = format;
    c->w = w;
    c->h = h;
    c->pixels = pixels;
    memset(pixels, 0, sel_gui_pixel_bytes(format, w, h));
    c->clip[0].x = 0;
    c->clip[0].y = 0;
    c->clip[0].w = w;
    c->clip[0].h = h;
    c->clip_n = 1u;
    c->colours = 1u;
}

void sel_gui_set_palette(sel_gui_canvas* c, const uint32_t* rgb, uint32_t first, uint32_t n) {
    uint32_t i, k;
    for (i = 0; i < n && first + i < 256u; i++) {
        c->palette[first + i] = rgb[i] & 0xFFFFFFu;
    }
    if (first + n > c->colours) {
        c->colours = first + n > 256u ? 256u : first + n;
    }
    for (i = 0; i < 32768u; i++) {
        int32_t r = (int32_t)(((i >> 10) & 31u) * 8u + 4u), g = (int32_t)(((i >> 5) & 31u) * 8u + 4u),
                b = (int32_t)((i & 31u) * 8u + 4u);
        uint32_t best = 0u, bestd = 0xFFFFFFFFu;
        for (k = 0; k < c->colours; k++) {
            int32_t dr = r - (int32_t)((c->palette[k] >> 16) & 255u), dg = g - (int32_t)((c->palette[k] >> 8) & 255u),
                    db = b - (int32_t)(c->palette[k] & 255u);
            uint32_t d = (uint32_t)(dr * dr + dg * dg + db * db);
            if (d < bestd) {
                bestd = d;
                best = k;
            }
        }
        c->cube[i] = (uint8_t)best;
    }
}

void sel_gui_palette_default(uint32_t* p, uint32_t accent, uint32_t surface, uint32_t text) {
    uint32_t i, r, g, b, n = 0u;
    p[n++] = accent & 0xFFFFFFu;
    p[n++] = surface & 0xFFFFFFu;
    p[n++] = text & 0xFFFFFFu;
    for (i = 0; i < 24u; i++) {
        uint32_t v = (i * 255u) / 23u;
        p[n++] = (v << 16) | (v << 8) | v;
    }
    for (r = 0; r < 6u; r++) {
        for (g = 0; g < 6u; g++) {
            for (b = 0; b < 6u; b++) {
                p[n++] = ((r * 51u) << 16) | ((g * 51u) << 8) | (b * 51u);
            }
        }
    }
    for (i = 0; n < 256u; i++) {
        uint32_t t = (i + 1u) * 255u / 14u;
        uint32_t cr = (((surface >> 16) & 255u) * (255u - t) + ((accent >> 16) & 255u) * t) / 255u;
        uint32_t cg = (((surface >> 8) & 255u) * (255u - t) + ((accent >> 8) & 255u) * t) / 255u;
        uint32_t cb = ((surface & 255u) * (255u - t) + (accent & 255u) * t) / 255u;
        p[n++] = (cr << 16) | (cg << 8) | cb;
    }
}

static uint32_t* px32(const sel_gui_canvas* c, int32_t x, int32_t y) {
    return (uint32_t*)c->pixels + (size_t)y * (size_t)c->w + (size_t)x;
}

static uint8_t* px8(const sel_gui_canvas* c, int32_t x, int32_t y) {
    return (uint8_t*)c->pixels + (size_t)y * (size_t)c->w + (size_t)x;
}

uint32_t sel_gui_pixel(const sel_gui_canvas* c, int32_t x, int32_t y) {
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) {
        return 0u;
    }
    return c->format == SEL_GUI_RGBA32 ? (*px32(c, x, y) & 0xFFFFFFu) : c->palette[*px8(c, x, y)];
}

uint32_t sel_gui_hash(const sel_gui_canvas* c) {
    uint32_t h = 2166136261u;
    size_t i, n = (size_t)c->w * (size_t)c->h;
    for (i = 0; i < n; i++) {
        uint32_t v = c->format == SEL_GUI_RGBA32 ? ((const uint32_t*)c->pixels)[i] : ((const uint8_t*)c->pixels)[i];
        h = (h ^ v) * 16777619u;
    }
    return h;
}

/* ---- clip stack ---- */

int sel_gui_rect_intersects(sel_gui_rect a, sel_gui_rect b) {
    return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 && a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
           b.y < a.y + a.h;
}

sel_gui_rect sel_gui_rect_intersect(sel_gui_rect a, sel_gui_rect b) {
    sel_gui_rect r;
    int32_t x1 = imin(a.x + a.w, b.x + b.w), y1 = imin(a.y + a.h, b.y + b.h);
    r.x = imax(a.x, b.x);
    r.y = imax(a.y, b.y);
    r.w = x1 > r.x ? x1 - r.x : 0;
    r.h = y1 > r.y ? y1 - r.y : 0;
    return r;
}

int sel_gui_clip_push(sel_gui_canvas* c, sel_gui_rect r) {
    if (c->clip_n >= SEL_GUI_CLIP_DEPTH) {
        return 0;
    }
    c->clip[c->clip_n] = sel_gui_rect_intersect(c->clip[c->clip_n - 1u], r);
    c->clip_n++;
    return 1;
}

void sel_gui_clip_pop(sel_gui_canvas* c) {
    if (c->clip_n > 1u) {
        c->clip_n--;
    }
}

/* ---- dirty list ---- */

static sel_gui_rect bbox(sel_gui_rect a, sel_gui_rect b) {
    sel_gui_rect r;
    int32_t x1 = imax(a.x + a.w, b.x + b.w), y1 = imax(a.y + a.h, b.y + b.h);
    r.x = imin(a.x, b.x);
    r.y = imin(a.y, b.y);
    r.w = x1 - r.x;
    r.h = y1 - r.y;
    return r;
}

void sel_gui_dirty_add(sel_gui_canvas* c, sel_gui_rect r) {
    uint32_t i;
    r = sel_gui_rect_intersect(r, c->clip[0]);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    /* Overlapping rectangles merge until the list is disjoint again; a contained one disappears. */
    for (i = 0; i < c->dirty_n;) {
        if (sel_gui_rect_intersects(r, c->dirty[i])) {
            r = bbox(r, c->dirty[i]);
            c->dirty[i] = c->dirty[--c->dirty_n];
            i = 0u;
        } else {
            i++;
        }
    }
    if (c->dirty_n >= SEL_GUI_DIRTY_MAX) { /* full: merge the pair whose bounding box adds the least area */
        sel_gui_rect all[SEL_GUI_DIRTY_MAX + 1u];
        uint32_t a, b, n = c->dirty_n, ba = 0u, bb = 1u, k = 0u;
        int64_t best = -1;
        for (a = 0; a < n; a++) {
            all[a] = c->dirty[a];
        }
        all[n++] = r;
        for (a = 0; a < n; a++) {
            for (b = a + 1u; b < n; b++) {
                sel_gui_rect u = bbox(all[a], all[b]);
                int64_t grow = (int64_t)u.w * u.h - (int64_t)all[a].w * all[a].h - (int64_t)all[b].w * all[b].h;
                if (best < 0 || grow < best) {
                    best = grow;
                    ba = a;
                    bb = b;
                }
            }
        }
        r = bbox(all[ba], all[bb]);
        for (a = 0; a < n; a++) {
            if (a != ba && a != bb) {
                c->dirty[k++] = all[a];
            }
        }
        c->dirty_n = k;
        sel_gui_dirty_add(c, r); /* the union may now overlap others: the normal path merges, then fits */
        return;
    }
    c->dirty[c->dirty_n++] = r;
}

uint32_t sel_gui_dirty_pixels(const sel_gui_canvas* c) {
    uint32_t i, n = 0u;
    for (i = 0; i < c->dirty_n; i++) {
        n += (uint32_t)c->dirty[i].w * (uint32_t)c->dirty[i].h;
    }
    return n;
}

uint32_t sel_gui_present(sel_gui_canvas* c) {
    uint32_t n = sel_gui_dirty_pixels(c);
    c->dirty_n = 0u;
    return n;
}

/* ---- pixel writes ---- */

static const uint8_t g_bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

static uint32_t cube_index(const sel_gui_canvas* c, uint32_t r, uint32_t g, uint32_t b) {
    return c->cube[((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)];
}

static uint32_t clampc(int32_t v) { return v < 0 ? 0u : v > 255 ? 255u : (uint32_t)v; }

/* Writes 0xAARRGGBB at an in-bounds pixel, blending when the alpha is not 255. `dither` is a bias of -8..7 added
 * to each channel before the INDEX8 palette lookup. */
static void put(sel_gui_canvas* c, int32_t x, int32_t y, uint32_t argb, int32_t dither) {
    uint32_t a = argb >> 24, sr = (argb >> 16) & 255u, sg = (argb >> 8) & 255u, sb = argb & 255u;
    if (a == 0u) {
        return;
    }
    if (c->format == SEL_GUI_RGBA32) {
        uint32_t* p = px32(c, x, y);
        if (a < 255u) {
            uint32_t d = *p;
            sr = div255(sr * a + ((d >> 16) & 255u) * (255u - a));
            sg = div255(sg * a + ((d >> 8) & 255u) * (255u - a));
            sb = div255(sb * a + (d & 255u) * (255u - a));
        }
        *p = 0xFF000000u | (sr << 16) | (sg << 8) | sb;
    } else {
        uint8_t* p = px8(c, x, y);
        if (a < 255u) {
            uint32_t d = c->palette[*p];
            sr = div255(sr * a + ((d >> 16) & 255u) * (255u - a));
            sg = div255(sg * a + ((d >> 8) & 255u) * (255u - a));
            sb = div255(sb * a + (d & 255u) * (255u - a));
        }
        *p = (uint8_t)cube_index(c, clampc((int32_t)sr + dither), clampc((int32_t)sg + dither),
                                 clampc((int32_t)sb + dither));
    }
}

static sel_gui_rect cur_clip(const sel_gui_canvas* c, sel_gui_rect r) {
    return sel_gui_rect_intersect(r, c->clip[c->clip_n - 1u]);
}

/* ---- primitives ---- */

void sel_gui_fill_rect(sel_gui_canvas* c, sel_gui_rect r, uint32_t argb) {
    int32_t x, y;
    c->prims++;
    r = cur_clip(c, r);
    for (y = r.y; y < r.y + r.h; y++) {
        for (x = r.x; x < r.x + r.w; x++) {
            put(c, x, y, argb, 0);
        }
    }
}

void sel_gui_scrim(sel_gui_canvas* c, sel_gui_rect r, uint32_t rgb, uint32_t alpha) {
    sel_gui_fill_rect(c, r, ((alpha > 255u ? 255u : alpha) << 24) | (rgb & 0xFFFFFFu));
}

/* Coverage 0..16 of pixel (px, py) by a circle of radius rad centred on pixel-edge coordinates (cx, cy), 4x4
 * subsamples at eighth-pixel positions. */
static uint32_t coverage(int32_t px, int32_t py, int32_t cx, int32_t cy, int32_t rad) {
    uint32_t n = 0u, sx, sy;
    int64_t lim = (int64_t)8 * rad * 8 * rad;
    for (sy = 0; sy < 4u; sy++) {
        for (sx = 0; sx < 4u; sx++) {
            int64_t dx = (int64_t)px * 8 + (int64_t)sx * 2 + 1 - (int64_t)cx * 8;
            int64_t dy = (int64_t)py * 8 + (int64_t)sy * 2 + 1 - (int64_t)cy * 8;
            if (dx * dx + dy * dy <= lim) {
                n++;
            }
        }
    }
    return n;
}

void sel_gui_round_rect(sel_gui_canvas* c, sel_gui_rect r, int32_t radius, uint32_t argb) {
    sel_gui_rect v;
    int32_t x, y, rad = imin(radius, imin(r.w, r.h) / 2);
    c->prims++;
    if (rad < 0) {
        rad = 0;
    }
    v = cur_clip(c, r);
    for (y = v.y; y < v.y + v.h; y++) {
        for (x = v.x; x < v.x + v.w; x++) {
            int32_t cx = x < r.x + rad ? r.x + rad : x >= r.x + r.w - rad ? r.x + r.w - rad : -1;
            int32_t cy = y < r.y + rad ? r.y + rad : y >= r.y + r.h - rad ? r.y + r.h - rad : -1;
            uint32_t a = argb >> 24;
            if (cx >= 0 && cy >= 0) {
                uint32_t cov = coverage(x, y, cx, cy, rad);
                if (c->format == SEL_GUI_INDEX8) {
                    if (cov < 8u) {
                        continue;
                    }
                } else {
                    a = (a * cov + 8u) / 16u;
                }
                put(c, x, y, (a << 24) | (argb & 0xFFFFFFu), 0);
                continue;
            }
            put(c, x, y, argb, 0);
        }
    }
}

void sel_gui_gradient(sel_gui_canvas* c, sel_gui_rect r, int vertical, uint32_t a, uint32_t b) {
    sel_gui_rect v;
    int32_t x, y, n = vertical ? r.h : r.w;
    int32_t step = n > 1 ? (int32_t)(65536 / (n - 1)) : 0; /* Q16 position change per pixel */
    int32_t ch[4], d[4], i;
    c->prims++;
    v = cur_clip(c, r);
    for (i = 0; i < 4; i++) {
        ch[i] = (int32_t)((a >> (24 - 8 * i)) & 255u);
        d[i] = (int32_t)((b >> (24 - 8 * i)) & 255u) - ch[i];
    }
    for (y = v.y; y < v.y + v.h; y++) {
        for (x = v.x; x < v.x + v.w; x++) {
            int32_t t = (vertical ? y - r.y : x - r.x) * step;
            uint32_t col = 0u;
            for (i = 0; i < 4; i++) {
                col = (col << 8) | clampc(ch[i] + (int32_t)(((int64_t)d[i] * t + 32768) >> 16));
            }
            put(c, x, y, col, c->format == SEL_GUI_INDEX8 ? (int32_t)g_bayer[((y & 3) << 2) | (x & 3)] - 8 : 0);
        }
    }
}

void sel_gui_line(sel_gui_canvas* c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t argb) {
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int32_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    const sel_gui_rect k = c->clip[c->clip_n - 1u];
    c->prims++;
    for (;;) {
        int32_t e2;
        if (x0 >= k.x && x0 < k.x + k.w && y0 >= k.y && y0 < k.y + k.h) {
            put(c, x0, y0, argb, 0);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void sel_gui_blit_mask(sel_gui_canvas* c, int32_t x, int32_t y, const uint8_t* mask, int32_t mw, int32_t mh,
                       int32_t mstride, uint32_t tint) {
    sel_gui_rect box = {0, 0, 0, 0}, v;
    int32_t px, py;
    box.x = x;
    box.y = y;
    box.w = mw;
    box.h = mh;
    c->prims++;
    v = cur_clip(c, box);
    for (py = v.y; py < v.y + v.h; py++) {
        for (px = v.x; px < v.x + v.w; px++) {
            uint32_t cov = mask[(size_t)(py - y) * (size_t)mstride + (size_t)(px - x)];
            uint32_t a = div255(cov * (tint >> 24));
            if (c->format == SEL_GUI_INDEX8) {
                a = a >= 128u ? 255u : 0u;
            }
            put(c, px, py, (a << 24) | (tint & 0xFFFFFFu), 0);
        }
    }
}

void sel_gui_blit_image(sel_gui_canvas* c, sel_gui_rect dst, const uint32_t* src, int32_t sw, int32_t sh,
                        int32_t sstride, int box) {
    sel_gui_rect v;
    int32_t x, y;
    c->prims++;
    if (dst.w <= 0 || dst.h <= 0) {
        return;
    }
    v = cur_clip(c, dst);
    for (y = v.y; y < v.y + v.h; y++) {
        int32_t sy0 = (int32_t)((int64_t)(y - dst.y) * sh / dst.h);
        int32_t sy1 = (int32_t)((int64_t)(y - dst.y + 1) * sh / dst.h);
        for (x = v.x; x < v.x + v.w; x++) {
            int32_t sx0 = (int32_t)((int64_t)(x - dst.x) * sw / dst.w);
            int32_t sx1 = (int32_t)((int64_t)(x - dst.x + 1) * sw / dst.w);
            uint32_t col;
            if (!box || (sx1 - sx0 <= 1 && sy1 - sy0 <= 1)) {
                col = src[(size_t)sy0 * (size_t)sstride + (size_t)sx0];
            } else {
                uint32_t sum[4] = {0, 0, 0, 0}, cnt = 0u;
                int32_t ix, iy, i;
                for (iy = sy0; iy < imax(sy1, sy0 + 1); iy++) {
                    for (ix = sx0; ix < imax(sx1, sx0 + 1); ix++) {
                        uint32_t s = src[(size_t)iy * (size_t)sstride + (size_t)ix];
                        for (i = 0; i < 4; i++) {
                            sum[i] += (s >> (24 - 8 * i)) & 255u;
                        }
                        cnt++;
                    }
                }
                col = 0u;
                for (i = 0; i < 4; i++) {
                    col = (col << 8) | ((sum[i] + cnt / 2u) / cnt);
                }
            }
            put(c, x, y, col, 0);
        }
    }
}

void sel_gui_shadow_bake(sel_gui_shadow* s, uint32_t radius, uint32_t rgb, uint32_t peak) {
    uint32_t i, j;
    memset(s, 0, sizeof *s);
    if (radius > SEL_GUI_SHADOW_MAX) {
        radius = SEL_GUI_SHADOW_MAX;
    }
    s->radius = radius;
    s->rgb = rgb & 0xFFFFFFu;
    for (i = 0; i < radius; i++) {
        for (j = 0; j < radius; j++) {
            uint32_t d256 = isqrt((i * i + j * j) * 65536u); /* 256 * distance */
            uint32_t t = d256 >= radius * 256u ? 0u : (radius * 256u - d256) / radius; /* 0..256 */
            s->corner[i * SEL_GUI_SHADOW_MAX + j] = (uint8_t)((peak * ((t * t) >> 8)) >> 8);
        }
    }
}

void sel_gui_shadow_draw(sel_gui_canvas* c, const sel_gui_shadow* s, sel_gui_rect box) {
    sel_gui_rect out, v;
    int32_t x, y, r = (int32_t)s->radius;
    c->prims++;
    out.x = box.x - r;
    out.y = box.y - r;
    out.w = box.w + 2 * r;
    out.h = box.h + 2 * r;
    v = cur_clip(c, out);
    for (y = v.y; y < v.y + v.h; y++) {
        for (x = v.x; x < v.x + v.w; x++) {
            int32_t ox = x < box.x ? box.x - x - 1 : x >= box.x + box.w ? x - box.x - box.w : -1;
            int32_t oy = y < box.y ? box.y - y - 1 : y >= box.y + box.h ? y - box.y - box.h : -1;
            uint32_t a;
            if (ox < 0 && oy < 0) {
                x = box.x + box.w - 1; /* inside the box: skip to its right edge */
                continue;
            }
            a = s->corner[(uint32_t)(oy < 0 ? 0 : oy) * SEL_GUI_SHADOW_MAX + (uint32_t)(ox < 0 ? 0 : ox)];
            if (c->format == SEL_GUI_INDEX8) {
                continue; /* the governor never asks for a shadow on a palette canvas */
            }
            put(c, x, y, (a << 24) | s->rgb, 0);
        }
    }
}
