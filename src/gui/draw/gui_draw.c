/* Integer rasterizer (plan DUI-P2-1) and dirty-rectangle list (DUI-P2-2). See gui_draw.h. */
#include <selecta/gui/gui_draw.h>

#include <selecta/gui/gui_genart.h>
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

/* Nearest of pal[0 .. n-1] for every 5-bit-per-channel colour. */
/* The first palette entry and size of the art, tile, panel and cover-flow regions (the UI region is entries 0-31). */
static const uint32_t g_rgn_base[4] = {32u, 64u, 128u, 192u};
static const uint32_t g_rgn_size[4] = {32u, 64u, 64u, 64u};

/* I574: exact nearest entry for every 5-bit-per-channel colour. The squared channel distances are tabled once per
 * entry (32 levels x 3 channels), so the 32768 x n inner loop is two adds and a compare with no multiply (a 68020
 * spent 118 s on the old five-cube multiply loop). Ties keep the lowest entry, as before. *ops counts the distance
 * evaluations, the host test pins it. */
static void build_cube(uint8_t* cube, const uint32_t* pal, uint32_t n, uint32_t* ops) {
    uint32_t tr[64][32], tg[64][32], tb[64][32];
    uint32_t i, k, r, g, bl;
    if (n > 64u) {
        n = 64u;
    }
    for (k = 0; k < n; k++) {
        for (i = 0; i < 32u; i++) {
            int32_t lvl = (int32_t)(i * 8u + 4u), dr = lvl - (int32_t)((pal[k] >> 16) & 255u),
                    dg = lvl - (int32_t)((pal[k] >> 8) & 255u), db = lvl - (int32_t)(pal[k] & 255u);
            tr[k][i] = (uint32_t)(dr * dr);
            tg[k][i] = (uint32_t)(dg * dg);
            tb[k][i] = (uint32_t)(db * db);
        }
    }
    for (r = 0; r < 32u; r++) {
        for (g = 0; g < 32u; g++) {
            uint32_t part[64];
            for (k = 0; k < n; k++) {
                part[k] = tr[k][r] + tg[k][g];
            }
            for (bl = 0; bl < 32u; bl++) {
                uint32_t best = 0u, bestd = 0xFFFFFFFFu;
                for (k = 0; k < n; k++) {
                    uint32_t d = part[k] + tb[k][bl];
                    if (d < bestd) {
                        bestd = d;
                        best = k;
                    }
                }
                cube[(r << 10) | (g << 5) | bl] = (uint8_t)best;
            }
        }
    }
    *ops += 32768u * n;
}

static void set_palette_raw(sel_gui_canvas* c, const uint32_t* rgb, uint32_t first, uint32_t n) {
    uint32_t i, k, last;
    for (i = 0; i < n && first + i < 256u; i++) {
        c->palette[first + i] = rgb[i] & 0xFFFFFFu;
    }
    if (first + n > c->colours) {
        c->colours = first + n > 256u ? 256u : first + n;
    }
    last = first + n > 256u ? 256u : first + n;
    if (first < 32u) { /* the UI colours */
        build_cube(c->cube, c->palette, c->colours < 32u ? c->colours : 32u, &c->cube_ops);
    }
    for (k = 0; k < 4u; k++) { /* a region sub-palette whose entries moved */
        uint32_t lo = g_rgn_base[k], hi = lo + g_rgn_size[k];
        if (first < hi && last > lo && c->colours > lo) {
            c->rcube_stale |= 1u << k; /* I574: built when the region is first drawn into, not at every palette set */
        }
    }
}

void sel_gui_set_palette(sel_gui_canvas* c, const uint32_t* rgb, uint32_t first, uint32_t n) {
    uint32_t t0 = sel_gui_prof_begin(c);
    set_palette_raw(c, rgb, first, n);
    sel_gui_prof_end(c, SEL_GUI_PROF_PALETTE, t0);
}

void sel_gui_set_region(sel_gui_canvas* c, uint32_t region) {
    uint32_t t0 = sel_gui_prof_begin(c);
    c->region = region < SEL_GUI_RGN_COUNT ? region : SEL_GUI_RGN_UI;
    sel_gui_prof_end(c, SEL_GUI_PROF_CLIP, t0);
}

uint32_t sel_gui_region_colours(const sel_gui_canvas* c, uint32_t region) {
    uint32_t i, n = 0u, v;
    if (region >= SEL_GUI_RGN_COUNT) {
        return 0u;
    }
    for (i = 0; i < 8u; i++) {
        for (v = c->used[region][i]; v != 0u; v &= v - 1u) {
            n++;
        }
    }
    return n;
}

static uint32_t mix(uint32_t a, uint32_t b, uint32_t t, uint32_t of) {
    uint32_t r = (((a >> 16) & 255u) * (of - t) + ((b >> 16) & 255u) * t) / of;
    uint32_t g = (((a >> 8) & 255u) * (of - t) + ((b >> 8) & 255u) * t) / of;
    uint32_t bl = ((a & 255u) * (of - t) + (b & 255u) * t) / of;
    return (r << 16) | (g << 8) | bl;
}

void sel_gui_palette_budget(uint32_t* p, const uint32_t* ui, uint32_t n) {
    uint32_t i, k = 0u, left;
    for (i = 0; i < n && i < 30u; i++) { /* entries 30 and 31 stay black and white: the extremes of scrims and gradients */
        p[k++] = ui[i] & 0xFFFFFFu;
    }
    left = 30u - k; /* the rest: steps from the surface toward the accent, then toward the text */
    for (i = 0; i < left; i++) {
        uint32_t toward = i < left / 2u ? ui[0] : ui[2];
        uint32_t j = i < left / 2u ? i : i - left / 2u, of = i < left / 2u ? left / 2u + 1u : left - left / 2u + 1u;
        p[k++] = mix(ui[1], toward, j + 1u, of);
    }
    p[30] = 0x000000u;
    p[31] = 0xFFFFFFu;
    sel_gui_genart_palette(p + 32);
    for (i = 0; i < 192u; i++) { /* three artwork sub-palettes, grey ramps until a quantiser fills them */
        uint32_t v = ((i & 63u) * 255u) / 63u;
        p[64u + i] = (v << 16) | (v << 8) | v;
    }
}

void sel_gui_palette_headline_shades(uint32_t* p, uint32_t fg0, uint32_t bg0, uint32_t fg1, uint32_t bg1) {
    uint32_t i;
    for (i = 0; i < 3u; i++) { /* quarter, half, three quarters from the ground to the ink */
        p[SEL_GUI_HEADLINE_SHADE0 + i] = mix(bg0 & 0xFFFFFFu, fg0 & 0xFFFFFFu, i + 1u, 4u);
        p[SEL_GUI_HEADLINE_SHADE0 + 3u + i] = mix(bg1 & 0xFFFFFFu, fg1 & 0xFFFFFFu, i + 1u, 4u);
    }
}

void sel_gui_palette_default(uint32_t* p, uint32_t accent, uint32_t surface, uint32_t text) {
    uint32_t ui[3];
    ui[0] = accent;
    ui[1] = surface;
    ui[2] = text;
    sel_gui_palette_budget(p, ui, 3u);
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

uint64_t sel_gui_fnv64(uint64_t h, const void* bytes, size_t n) {
    const uint8_t* p = (const uint8_t*)bytes;
    size_t i;
    for (i = 0; i < n; i++) {
        h = (h ^ p[i]) * SEL_GUI_FNV64_PRIME;
    }
    return h;
}

uint64_t sel_gui_fnv64_word(uint64_t h, uint32_t word) {
    uint8_t b[4];
    b[0] = (uint8_t)word;
    b[1] = (uint8_t)(word >> 8);
    b[2] = (uint8_t)(word >> 16);
    b[3] = (uint8_t)(word >> 24);
    return sel_gui_fnv64(h, b, 4u);
}

uint64_t sel_gui_hash(const sel_gui_canvas* c) {
    uint64_t h = SEL_GUI_FNV64_BASIS;
    size_t i, n = (size_t)c->w * (size_t)c->h;
    for (i = 0; i < n; i++) {
        if (c->format == SEL_GUI_RGBA32) {
            h = sel_gui_fnv64_word(h, ((const uint32_t*)c->pixels)[i]);
        } else {
            h = sel_gui_fnv64(h, (const uint8_t*)c->pixels + i, 1u);
        }
    }
    return h;
}

void sel_gui_hash_hex(uint64_t h, char out[17]) {
    static const char digits[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 16; i++) {
        out[i] = digits[(h >> (60 - 4 * i)) & 0xFu];
    }
    out[16] = '\0';
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

/* I468: a rect given in the square-pixel layout space as canvas pixels (rows halved for ydiv 2; a non-empty rect
 * keeps at least one row). */
static int32_t ydv(const sel_gui_canvas* c) { return c->ydiv > 1 ? c->ydiv : 1; }

static sel_gui_rect to_canvas(const sel_gui_canvas* c, sel_gui_rect r) {
    int32_t k = ydv(c), y0, y1;
    if (k == 1) {
        return r;
    }
    y0 = r.y >= 0 ? r.y / k : -((-r.y + k - 1) / k);
    y1 = r.y + r.h >= 0 ? (r.y + r.h) / k : -((-(r.y + r.h) + k - 1) / k);
    r.y = y0;
    r.h = y1 - y0 < 1 && r.h > 0 ? 1 : y1 - y0;
    return r;
}

static int clip_push_raw(sel_gui_canvas* c, sel_gui_rect r) {
    if (c->clip_n >= SEL_GUI_CLIP_DEPTH) {
        return 0;
    }
    c->clip[c->clip_n] = sel_gui_rect_intersect(c->clip[c->clip_n - 1u], r);
    c->clip_n++;
    return 1;
}

int sel_gui_clip_push(sel_gui_canvas* c, sel_gui_rect r) {
    uint32_t t0 = sel_gui_prof_begin(c);
    int ok = clip_push_raw(c, to_canvas(c, r));
    sel_gui_prof_end(c, SEL_GUI_PROF_CLIP, t0);
    return ok;
}

void sel_gui_clip_pop(sel_gui_canvas* c) {
    uint32_t t0 = sel_gui_prof_begin(c);
    if (c->clip_n > 1u) {
        c->clip_n--;
    }
    sel_gui_prof_end(c, SEL_GUI_PROF_CLIP, t0);
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

static void dirty_add_raw(sel_gui_canvas* c, sel_gui_rect r) {
    uint32_t i;
    r = sel_gui_rect_intersect(r, c->clip[c->clip_n - 1u]); /* I549: pixels outside the active clip are never painted, so they are not dirty */
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    if (r.w == c->w && r.h == c->h) {
        c->copper_n = 0u; /* a whole-canvas repaint redraws every gradient: the pens are handed out again */
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
        dirty_add_raw(c, r); /* the union may now overlap others: the normal path merges, then fits */
        return;
    }
    c->dirty[c->dirty_n++] = r;
}

void sel_gui_dirty_add(sel_gui_canvas* c, sel_gui_rect r) {
    uint32_t t0 = sel_gui_prof_begin(c);
    dirty_add_raw(c, to_canvas(c, r));
    sel_gui_prof_end(c, SEL_GUI_PROF_DIRTY, t0);
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

static uint32_t cube_index(sel_gui_canvas* c, uint32_t r, uint32_t g, uint32_t b) {
    uint32_t at = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3), idx;
    if (c->region >= SEL_GUI_RGN_ART && (c->rcube_stale & (1u << (c->region - SEL_GUI_RGN_ART))) != 0u &&
        c->colours > g_rgn_base[c->region - SEL_GUI_RGN_ART]) {
        uint32_t k = c->region - SEL_GUI_RGN_ART, lo = g_rgn_base[k], sz = g_rgn_size[k] - (k == 3u ? c->copper_pens : 0u);
        build_cube(c->rcube[k], c->palette + lo, c->colours - lo < sz ? c->colours - lo : sz, &c->cube_ops);
        c->rcube_stale &= ~(1u << k);
    }
    idx = c->region >= SEL_GUI_RGN_ART
              ? g_rgn_base[c->region - SEL_GUI_RGN_ART] + c->rcube[c->region - SEL_GUI_RGN_ART][at]
              : c->cube[at];
    c->used[c->region][idx >> 5] |= 1u << (idx & 31u);
    return idx;
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

/* Span fill context: one colour written along row spans. An opaque colour is resolved to its pixel value (palette
 * index on INDEX8) once, lazily on the first span so an empty clip marks no palette entry as used, and stored with
 * memset or a word loop. A translucent colour blends per pixel, but on INDEX8 each distinct destination byte is
 * resolved once through a 256 entry table. The output equals put() pixel for pixel. */
typedef struct {
    sel_gui_canvas* c;
    uint32_t argb;
    int solid, ready;
    uint32_t val;
    uint16_t lut[256];
} fill_ctx;

static void fill_ctx_init(fill_ctx* f, sel_gui_canvas* c, uint32_t argb) {
    f->c = c;
    f->argb = argb;
    f->solid = (argb >> 24) == 255u;
    f->ready = 0;
    f->val = 0u;
    if (!f->solid && (argb >> 24) != 0u && c->format == SEL_GUI_INDEX8) {
        memset(f->lut, 0xFF, sizeof f->lut);
    }
}

/* Fills the in-bounds, already clipped pixels [x0, x1) of row y. */
static void fill_span(fill_ctx* f, int32_t x0, int32_t x1, int32_t y) {
    sel_gui_canvas* c = f->c;
    int32_t x;
    if (x1 <= x0 || (f->argb >> 24) == 0u) {
        return;
    }
    c->span_px += (uint32_t)(x1 - x0);
    if (f->solid) {
        if (!f->ready) {
            f->val = c->format == SEL_GUI_RGBA32
                         ? (f->argb | 0xFF000000u)
                         : cube_index(c, (f->argb >> 16) & 255u, (f->argb >> 8) & 255u, f->argb & 255u);
            f->ready = 1;
        }
        if (c->format == SEL_GUI_RGBA32) {
            uint32_t* p = px32(c, x0, y);
            for (x = x0; x < x1; x++) {
                *p++ = f->val;
            }
        } else {
            memset(px8(c, x0, y), (int)f->val, (size_t)(x1 - x0));
        }
        return;
    }
    if (c->format == SEL_GUI_RGBA32) {
        for (x = x0; x < x1; x++) {
            put(c, x, y, f->argb, 0);
        }
    } else {
        uint8_t* p = px8(c, x0, y);
        for (x = x0; x < x1; x++, p++) {
            uint8_t d = *p;
            if (f->lut[d] == 0xFFFFu) {
                put(c, x, y, f->argb, 0);
                f->lut[d] = *p;
            } else {
                *p = (uint8_t)f->lut[d];
            }
        }
    }
}

static void fill_rect_raw(sel_gui_canvas* c, sel_gui_rect r, uint32_t argb) {
    fill_ctx f;
    int32_t y;
    c->prims++;
    r = cur_clip(c, r);
    fill_ctx_init(&f, c, argb);
    for (y = r.y; y < r.y + r.h; y++) {
        fill_span(&f, r.x, r.x + r.w, y);
    }
}

void sel_gui_scrim(sel_gui_canvas* c, sel_gui_rect r, uint32_t rgb, uint32_t alpha) {
    uint32_t t0 = sel_gui_prof_begin(c);
    sel_gui_fill_rect(c, r, ((alpha > 255u ? 255u : alpha) << 24) | (rgb & 0xFFFFFFu));
    sel_gui_prof_end(c, SEL_GUI_PROF_SCRIM, t0);
}

/* Coverage 0..16 of pixel (px, py) by a circle of radius rad centred on pixel-edge coordinates (cx, cy), 4x4
 * subsamples at eighth-pixel positions. */
static uint32_t coverage(int32_t px, int32_t py, int32_t cx, int32_t cy, int32_t rad, int32_t k) {
    /* 32-bit integer only (the 68020 has no 64-bit multiply): the squared offsets of the four sample columns and rows
     * are formed once, the 16 samples then only add and compare. A pixel wholly inside or outside the circle is
     * decided from the extreme sums. The count equals the 64-bit form for rad up to 1000. */
    int32_t lim = 64 * rad * rad, ax = (px - cx) * 8, ay = (py - cy) * 8 * k, i, j; /* k: row height in columns (I468) */
    int32_t xs[4], ys[4], xmin, xmax, ymin, ymax;
    uint32_t n = 0u;
    for (i = 0; i < 4; i++) {
        int32_t dx = ax + 2 * i + 1, dy = ay + (2 * i + 1) * k;
        xs[i] = dx * dx;
        ys[i] = dy * dy;
    }
    xmin = xmax = xs[0];
    ymin = ymax = ys[0];
    for (i = 1; i < 4; i++) {
        xmin = imin(xmin, xs[i]);
        xmax = imax(xmax, xs[i]);
        ymin = imin(ymin, ys[i]);
        ymax = imax(ymax, ys[i]);
    }
    if (xmax + ymax <= lim) {
        return 16u;
    }
    if (xmin + ymin > lim) {
        return 0u;
    }
    for (j = 0; j < 4; j++) {
        for (i = 0; i < 4; i++) {
            n += (xs[i] + ys[j] <= lim) ? 1u : 0u;
        }
    }
    return n;
}

/* One pixel of a corner row: cy is the corner centre row; the corner centre column follows from x. */
static void corner_px(sel_gui_canvas* c, sel_gui_rect r, int32_t rad, int32_t x, int32_t y, int32_t cy,
                      uint32_t argb, int32_t k) {
    int32_t cx = x < r.x + rad ? r.x + rad : x >= r.x + r.w - rad ? r.x + r.w - rad : -1;
    uint32_t a = argb >> 24;
    if (cx >= 0) {
        uint32_t cov = coverage(x, y, cx, cy, rad, k);
        /* I588: INDEX8 blends too: put() mixes the colour with the destination's palette entry and snaps to the
         * nearest of the reserved in-between steps (sel_gui_palette_budget), so the edge has shades, not two colours */
        a = (a * cov + 8u) / 16u;
        put(c, x, y, (a << 24) | (argb & 0xFFFFFFu), 0);
        return;
    }
    put(c, x, y, argb, 0);
}

static void round_rect_raw(sel_gui_canvas* c, sel_gui_rect r, int32_t radius, uint32_t argb, int32_t k) {
    sel_gui_rect v;
    fill_ctx f;
    /* k = row height in columns: the corners are ellipses rad wide and rad / k rows tall (I468). */
    int32_t x, y, rad = imin(radius, imin(r.w, r.h * k) / 2), ry = (rad + k - 1) / k;
    c->prims++;
    if (rad < 0) {
        rad = 0;
    }
    v = cur_clip(c, r);
    fill_ctx_init(&f, c, argb);
    for (y = v.y; y < v.y + v.h; y++) {
        int32_t cy = y < r.y + ry ? r.y + ry : y >= r.y + r.h - ry ? r.y + r.h - ry : -1;
        int32_t m0 = imax(v.x, r.x + rad), m1 = imin(v.x + v.w, r.x + r.w - rad);
        if (cy < 0) {
            fill_span(&f, v.x, v.x + v.w, y);
            continue;
        }
        /* Corner rows: only the pixels left of m0 and right of m1 sit in a corner square; the middle is solid. */
        if (m1 < m0) {
            m0 = m1 = imin(imax(m0, v.x), v.x + v.w);
        }
        for (x = v.x; x < m0; x++) {
            corner_px(c, r, rad, x, y, cy, argb, k);
        }
        fill_span(&f, m0, m1, y);
        for (x = imax(m1, m0); x < v.x + v.w; x++) {
            corner_px(c, r, rad, x, y, cy, argb, k);
        }
    }
}

/* Coverage 0..16 of pixel (x, y) by the rounded rectangle r with corner radius rad (already clamped). */
static uint32_t round_cov(sel_gui_rect r, int32_t rad, int32_t k, int32_t x, int32_t y) {
    int32_t cx, cy, ry = (rad + k - 1) / k; /* k: row height in columns (I468) */
    if (x < r.x || x >= r.x + r.w || y < r.y || y >= r.y + r.h) {
        return 0u;
    }
    cx = x < r.x + rad ? r.x + rad : x >= r.x + r.w - rad ? r.x + r.w - rad : -1;
    cy = y < r.y + ry ? r.y + ry : y >= r.y + r.h - ry ? r.y + r.h - ry : -1;
    return cx >= 0 && cy >= 0 ? coverage(x, y, cx, cy, rad, k) : 16u;
}

static void round_frame_raw(sel_gui_canvas* c, sel_gui_rect r, int32_t radius, int32_t width, uint32_t argb,
                            int32_t k) {
    sel_gui_rect v, in;
    fill_ctx f;
    /* k = row height in columns (I468): the frame is `width` columns and width / k rows thick (at least one row), the
     * corners are ellipses rad wide and ry rows tall. */
    int32_t x, y, rad = imin(radius, imin(r.w, r.h * k) / 2), irad, wy = imax(1, width / k), ry, iry;
    c->prims++;
    if (rad < 0) {
        rad = 0;
    }
    if (width < 1) {
        return;
    }
    in = (sel_gui_rect){r.x + width, r.y + wy, r.w - 2 * width, r.h - 2 * wy};
    irad = rad > width ? rad - width : 0;
    if (in.w > 0 && in.h > 0) {
        irad = imin(irad, imin(in.w, in.h * k) / 2);
    }
    ry = (rad + k - 1) / k;
    iry = (irad + k - 1) / k;
    v = cur_clip(c, r);
    fill_ctx_init(&f, c, argb);
    for (y = v.y; y < v.y + v.h; y++) {
        int32_t x1 = v.x + v.w;
        int32_t o0 = imin(imax(r.x + rad, v.x), x1), o1 = imax(imin(r.x + r.w - rad, x1), o0);
        int32_t s0 = x1, s1 = x1; /* [s0, s1): inner coverage is 16 here, so the frame has none */
        int in_rows = in.w > 0 && in.h > 0 && y >= in.y && y < in.y + in.h;
        if (in_rows) {
            int band = y >= in.y + iry && y < in.y + in.h - iry;
            s0 = imin(imax(band ? in.x : in.x + irad, v.x), x1);
            s1 = imax(imin(band ? in.x + in.w : in.x + in.w - irad, x1), s0);
        } else if (y >= r.y + ry && y < r.y + r.h - ry) {
            o0 = v.x; /* outer middle band, no inner: the whole row is covered 16 */
            o1 = x1;
        }
        for (x = v.x; x < x1; x++) {
            uint32_t o, i, a, cov;
            if (x == s0 && s1 > s0) {
                x = s1 - 1;
                continue;
            }
            if (x == o0 && o1 > o0 && !in_rows) {
                fill_span(&f, o0, o1, y);
                x = o1 - 1;
                continue;
            }
            o = round_cov(r, rad, k, x, y);
            i = in_rows ? round_cov(in, irad, k, x, y) : 0u;
            a = argb >> 24;
            cov = o > i ? o - i : 0u;
            if (cov == 0u) {
                continue;
            }
            a = (a * cov + 8u) / 16u; /* I588: INDEX8 blends through put() as well */
            put(c, x, y, (a << 24) | (argb & 0xFFFFFFu), 0);
        }
    }
}

static void gradient_raw(sel_gui_canvas* c, sel_gui_rect r, int vertical, uint32_t a, uint32_t b) {
    sel_gui_rect v;
    int32_t n = vertical ? r.h : r.w;
    int32_t step = n > 1 ? (int32_t)(65536 / (n - 1)) : 0; /* Q16 position change per pixel */
    int32_t ch[4], d[4], i;
    c->prims++;
    v = cur_clip(c, r);
    for (i = 0; i < 4; i++) {
        ch[i] = (int32_t)((a >> (24 - 8 * i)) & 255u);
        d[i] = (int32_t)((b >> (24 - 8 * i)) & 255u) - ch[i];
    }
    /* The colour depends on one axis only; the dither on (x & 3, y & 3). Resolve the colour once per line along the
     * gradient axis and its four dithered indices once, then store the line. */
    {
        int32_t lines = vertical ? v.h : v.w, run = vertical ? v.w : v.h, l;
        for (l = 0; l < lines; l++) {
            int32_t pos = (vertical ? v.y : v.x) + l, t = (pos - (vertical ? r.y : r.x)) * step, k;
            uint32_t col = 0u;
            uint32_t idx[4];
            int opaque;
            for (i = 0; i < 4; i++) {
                col = (col << 8) | clampc(ch[i] + (int32_t)(((int64_t)d[i] * t + 32768) >> 16));
            }
            opaque = (col >> 24) == 255u;
            if (opaque && c->format == SEL_GUI_INDEX8) {
                uint32_t rr = (col >> 16) & 255u, gg = (col >> 8) & 255u, bb = col & 255u;
                idx[0] = idx[1] = idx[2] = idx[3] = 256u; /* resolved on first use: unused cells mark nothing */
                for (k = 0; k < run; k++) {
                    int32_t o = (vertical ? v.x : v.y) + k, q = o & 3;
                    int32_t cell = vertical ? (((pos & 3) << 2) | q) : ((q << 2) | (pos & 3));
                    int32_t dv = (int32_t)g_bayer[cell] - 8;
                    if (idx[q] == 256u) {
                        idx[q] = cube_index(c, clampc((int32_t)rr + dv), clampc((int32_t)gg + dv),
                                            clampc((int32_t)bb + dv));
                    }
                    if (vertical) {
                        *px8(c, o, pos) = (uint8_t)idx[q];
                    } else {
                        *px8(c, pos, o) = (uint8_t)idx[q];
                    }
                }
            } else {
                for (k = 0; k < run; k++) {
                    int32_t o = (vertical ? v.x : v.y) + k;
                    int32_t x = vertical ? o : pos, y = vertical ? pos : o;
                    put(c, x, y, col, c->format == SEL_GUI_INDEX8 ? (int32_t)g_bayer[((y & 3) << 2) | (x & 3)] - 8 : 0);
                }
            }
        }
    }
}

static void line_raw(sel_gui_canvas* c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t argb) {
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

static void blit_mask_raw(sel_gui_canvas* c, int32_t x, int32_t y, const uint8_t* mask, int32_t mw, int32_t mh,
                       int32_t mstride, uint32_t tint) {
    sel_gui_rect box = {0, 0, 0, 0}, v;
    int32_t px, py;
    uint32_t ta = tint >> 24, memo_key = 0xFFFFFFFFu;
    uint8_t memo_val = 0u;
    box.x = x;
    box.y = y;
    box.w = mw;
    box.h = mh;
    c->prims++;
    v = cur_clip(c, box);
    for (py = v.y; py < v.y + v.h; py++) {
        const uint8_t* row = mask + (size_t)(py - y) * (size_t)mstride + (size_t)(v.x - x);
        for (px = v.x; px < v.x + v.w; px++) {
            uint32_t cov = *row++, a;
            if (cov == 0u) {
                continue;
            }
            a = div255(cov * ta);
            if (c->format == SEL_GUI_INDEX8) {
                /* The result depends on the alpha and, below full alpha, on the destination byte only: remember the
                 * last pair (text runs repeat the same edge values over the same background). */
                uint8_t* d = px8(c, px, py);
                uint32_t key = (a << 8) | (a < 255u ? *d : 0u);
                if (a == 0u) {
                    continue;
                }
                if (key != memo_key) {
                    put(c, px, py, (a << 24) | (tint & 0xFFFFFFu), 0);
                    memo_val = *d;
                    memo_key = key;
                } else {
                    *d = memo_val;
                }
                continue;
            }
            put(c, px, py, (a << 24) | (tint & 0xFFFFFFu), 0);
        }
    }
}

static void blit_image_raw(sel_gui_canvas* c, sel_gui_rect dst, const uint32_t* src, int32_t sw, int32_t sh,
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

/* I477: nearest image blit whose corners follow a rounded rect of `radius` (the card behind it). Edge pixels blend the
 * source by round_cov coverage over what is already there; INDEX8 keeps or drops a pixel at half coverage. */
static void blit_image_round_raw(sel_gui_canvas* c, sel_gui_rect dst, const uint32_t* src, int32_t sw, int32_t sh,
                                 int32_t sstride, int32_t radius, int32_t k) {
    sel_gui_rect v;
    int32_t x, y, rad = imin(radius, imin(dst.w, dst.h * k) / 2), ry = (rad + k - 1) / k; /* k: I468 */
    c->prims++;
    if (dst.w <= 0 || dst.h <= 0) {
        return;
    }
    if (rad < 0) {
        rad = 0;
    }
    v = cur_clip(c, dst);
    for (y = v.y; y < v.y + v.h; y++) {
        int32_t sy = (int32_t)((int64_t)(y - dst.y) * sh / dst.h);
        int in_band = y >= dst.y + ry && y < dst.y + dst.h - ry;
        int32_t m0 = dst.x + rad, m1 = dst.x + dst.w - rad;
        for (x = v.x; x < v.x + v.w; x++) {
            int32_t sx = (int32_t)((int64_t)(x - dst.x) * sw / dst.w);
            uint32_t col = src[(size_t)sy * (size_t)sstride + (size_t)sx];
            uint32_t cov = (in_band || (x >= m0 && x < m1)) ? 16u : round_cov(dst, rad, k, x, y);
            if (cov == 0u) {
                continue;
            }
            if (cov < 16u) {
                if (c->format == SEL_GUI_INDEX8) {
                    if (cov < 8u) {
                        continue;
                    }
                } else {
                    col = ((((col >> 24) * cov + 8u) / 16u) << 24) | (col & 0xFFFFFFu);
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

static void shadow_draw_raw(sel_gui_canvas* c, const sel_gui_shadow* s, sel_gui_rect box, int32_t k) {
    sel_gui_rect out, v;
    int32_t x, y, r = (int32_t)s->radius, ry = (r + k - 1) / k; /* k: row height in columns (I468) */
    c->prims++;
    out.x = box.x - r;
    out.y = box.y - ry;
    out.w = box.w + 2 * r;
    out.h = box.h + 2 * ry;
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
            if (oy >= 0 && oy * k >= r) {
                continue;
            }
            a = s->corner[(uint32_t)(oy < 0 ? 0 : oy * k) * SEL_GUI_SHADOW_MAX + (uint32_t)(ox < 0 ? 0 : ox)];
            if (c->format == SEL_GUI_INDEX8) {
                continue; /* the governor never asks for a shadow on a palette canvas */
            }
            put(c, x, y, (a << 24) | s->rgb, 0);
        }
    }
}

/* I507: the shadow of a rounded box (a Home cover). The shape the falloff follows is the box inset by the shadow radius r on
 * the top and sides, so the whole falloff there ends at the box edge and hides under the cover (no ring or hairline above or
 * beside it). Its bottom edge sits r - lift above the box bottom, so only the last `lift` pixels of the falloff show under
 * the cover: the visible shadow starts faint and fades out, with no dark step against the cover edge (the first version
 * showed the falloff from its darkest value, which read as a dark border on the dark theme). Distance is measured to that
 * rounded shape with the baked falloff table (corner[0][k], k = pixels outside the edge, as in the square draw); the
 * shape's own pixels are skipped because the cover covers them.
 * I513: the shape is the box inset by `inset` on the top and sides. A cover passes the shadow radius (above); a floating
 * rounded card (menu, tooltip, toast, dialog) passes 0, so the ring runs all around the card and follows its rounded
 * corners instead of leaving a square bracket with a light notch at each corner. */
static void shadow_round_raw(sel_gui_canvas* c, const sel_gui_shadow* s, sel_gui_rect box, int32_t radius, int32_t inset,
                             int32_t lift, int32_t k) {
    sel_gui_rect sh, out, v;
    /* k = row height in columns (I468): the vertical offsets (inset, radius, falloff) are in rows; distances scale by k. */
    int32_t x, y, r = (int32_t)s->radius, ry = (r + k - 1) / k, rad, radx, iy = (inset + k - 1) / k;
    c->prims++;
    sh = (sel_gui_rect){box.x + inset, box.y + iy, box.w - 2 * inset, box.h + lift - 2 * iy};
    if (sh.w <= 0 || sh.h <= 0 || c->format == SEL_GUI_INDEX8) {
        return; /* the governor never asks for a shadow on a palette canvas */
    }
    radx = imax(0, imin(radius - inset, imin(sh.w, sh.h * k) / 2));
    rad = (radx + k - 1) / k; /* corner height in rows */
    out = (sel_gui_rect){sh.x - r, sh.y - ry, sh.w + 2 * r, sh.h + 2 * ry};
    v = cur_clip(c, out);
    for (y = v.y; y < v.y + v.h; y++) {
        int32_t dy = y < sh.y + rad ? sh.y + rad - y : y >= sh.y + sh.h - rad ? y - (sh.y + sh.h - rad - 1) : 0;
        for (x = v.x; x < v.x + v.w; x++) {
            int32_t dx = x < sh.x + radx ? sh.x + radx - x : x >= sh.x + sh.w - radx ? x - (sh.x + sh.w - radx - 1) : 0;
            uint32_t d256, kd;
            if (dx == 0 && dy == 0) {
                continue;
            }
            d256 = isqrt((uint32_t)(dx * dx + dy * k * dy * k) * 65536u); /* 256 * distance to the inner (radius-shrunk) rectangle */
            if (d256 <= (uint32_t)radx * 256u) {
                continue; /* inside the rounded shape */
            }
            kd = d256 >= ((uint32_t)radx + 1u) * 256u ? (d256 - ((uint32_t)radx + 1u) * 256u) / 256u : 0u;
            if (kd >= (uint32_t)r) {
                continue;
            }
            put(c, x, y, ((uint32_t)s->corner[kd] << 24) | s->rgb, 0);
        }
    }
}

/* I389: profiled entry points; the raw functions above hold the drawing. */
void sel_gui_fill_rect(sel_gui_canvas* c, sel_gui_rect r, uint32_t argb) {
    uint32_t t0 = sel_gui_prof_begin(c);
    fill_rect_raw(c, to_canvas(c, r), argb);
    sel_gui_prof_end(c, SEL_GUI_PROF_FILL, t0);
}

void sel_gui_round_rect(sel_gui_canvas* c, sel_gui_rect r, int32_t radius, uint32_t argb) {
    uint32_t t0 = sel_gui_prof_begin(c);
    round_rect_raw(c, to_canvas(c, r), radius, argb, ydv(c));
    sel_gui_prof_end(c, SEL_GUI_PROF_ROUND, t0);
}

void sel_gui_round_frame(sel_gui_canvas* c, sel_gui_rect r, int32_t radius, int32_t width, uint32_t argb) {
    uint32_t t0 = sel_gui_prof_begin(c);
    round_frame_raw(c, to_canvas(c, r), radius, width, argb, ydv(c));
    sel_gui_prof_end(c, SEL_GUI_PROF_ROUND, t0);
}

void sel_gui_copper_enable(sel_gui_canvas* c, uint32_t pens) {
    c->copper_pens = pens > 4u ? 4u : pens;
    c->copper_n = 0u;
}

static uint32_t copper_rgb(const struct sel_gui_copper_key* k, int32_t y) {
    int32_t step = k->rh > 1 ? (int32_t)(65536 / (k->rh - 1)) : 0, t = (y - k->ry) * step, ch;
    uint32_t rgb = 0u;
    for (ch = 16; ch >= 0; ch -= 8) {
        int32_t from = (int32_t)((k->a >> ch) & 255u), to = (int32_t)((k->b >> ch) & 255u);
        rgb |= clampc(from + (int32_t)(((int64_t)(to - from) * t + 32768) >> 16)) << ch;
    }
    return rgb;
}

/* I441: fills the visible rows of an opaque vertical gradient with its copper pen. 0 when no pen is free (the caller
 * draws the dithered gradient). */
static int copper_gradient(sel_gui_canvas* c, sel_gui_rect r, uint32_t a, uint32_t b) {
    sel_gui_rect v = cur_clip(c, r);
    uint32_t i;
    int32_t y;
    if (v.w <= 0 || v.h <= 0) {
        return 1;
    }
    for (i = 0; i < c->copper_n; i++) {
        const struct sel_gui_copper_key* k = &c->copper_key[i];
        if (k->ry == r.y && k->rh == r.h && k->a == (a & 0xFFFFFFu) && k->b == (b & 0xFFFFFFu)) {
            break;
        }
    }
    if (i == c->copper_n) {
        if (i >= c->copper_pens) {
            return 0;
        }
        c->copper_key[i].ry = r.y;
        c->copper_key[i].rh = r.h;
        c->copper_key[i].a = a & 0xFFFFFFu;
        c->copper_key[i].b = b & 0xFFFFFFu;
        c->copper_key[i].y0 = v.y;
        c->copper_key[i].y1 = v.y + v.h;
        c->copper_n++;
    } else {
        if (v.y < c->copper_key[i].y0) c->copper_key[i].y0 = v.y;
        if (v.y + v.h > c->copper_key[i].y1) c->copper_key[i].y1 = v.y + v.h;
    }
    for (y = v.y; y < v.y + v.h; y++) {
        memset(px8(c, v.x, y), (int)(255u - i), (size_t)v.w);
    }
    c->used[c->region][(255u - i) >> 5] |= 1u << ((255u - i) & 31u);
    c->copper_filled += (uint32_t)v.h;
    c->span_px += (uint32_t)(v.w * v.h);
    return 1;
}

uint32_t sel_gui_copper_changes(const sel_gui_canvas* c, sel_gui_copper_change* out, uint32_t max) {
    uint32_t n = 0u, i;
    int32_t y, y0 = c->h, y1 = 0;
    for (i = 0; i < c->copper_n; i++) {
        if (c->copper_key[i].y0 < y0) y0 = c->copper_key[i].y0;
        if (c->copper_key[i].y1 > y1) y1 = c->copper_key[i].y1;
    }
    for (y = y0; y < y1; y++) {
        for (i = 0; i < c->copper_n && n < max; i++) {
            const struct sel_gui_copper_key* k = &c->copper_key[i];
            uint32_t rgb;
            if (y < k->y0 || y >= k->y1) {
                continue;
            }
            rgb = copper_rgb(k, y);
            if (y > k->y0 && copper_rgb(k, y - 1) == rgb) {
                continue; /* the copper holds the colour */
            }
            out[n].y = y;
            out[n].pen = 255u - i;
            out[n].rgb = rgb;
            n++;
        }
    }
    return n;
}

void sel_gui_gradient(sel_gui_canvas* c, sel_gui_rect r, int vertical, uint32_t a, uint32_t b) {
    uint32_t t0 = sel_gui_prof_begin(c);
    sel_gui_rect cr = to_canvas(c, r);
    if (!(vertical && c->copper_pens > 0u && c->format == SEL_GUI_INDEX8 && (a >> 24) == 255u && (b >> 24) == 255u &&
          copper_gradient(c, cr, a, b))) {
        gradient_raw(c, cr, vertical, a, b);
    }
    sel_gui_prof_end(c, SEL_GUI_PROF_GRADIENT, t0);
}

void sel_gui_line(sel_gui_canvas* c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t argb) {
    uint32_t t0 = sel_gui_prof_begin(c);
    line_raw(c, x0, y0 / ydv(c), x1, y1 / ydv(c), argb);
    sel_gui_prof_end(c, SEL_GUI_PROF_LINE, t0);
}

void sel_gui_blit_mask(sel_gui_canvas* c, int32_t x, int32_t y, const uint8_t* mask, int32_t mw, int32_t mh, int32_t mstride, uint32_t tint) {
    uint32_t t0 = sel_gui_prof_begin(c);
    blit_mask_raw(c, x, y / ydv(c), mask, mw, mh, mstride, tint);
    sel_gui_prof_end(c, SEL_GUI_PROF_MASK, t0);
}

void sel_gui_blit_image(sel_gui_canvas* c, sel_gui_rect dst, const uint32_t* src, int32_t sw, int32_t sh, int32_t sstride, int box) {
    uint32_t t0 = sel_gui_prof_begin(c);
    blit_image_raw(c, to_canvas(c, dst), src, sw, sh, sstride, box);
    sel_gui_prof_end(c, SEL_GUI_PROF_IMAGE, t0);
}

void sel_gui_blit_image_round(sel_gui_canvas* c, sel_gui_rect dst, const uint32_t* src, int32_t sw, int32_t sh, int32_t sstride, int32_t radius) {
    uint32_t t0 = sel_gui_prof_begin(c);
    blit_image_round_raw(c, to_canvas(c, dst), src, sw, sh, sstride, radius, ydv(c));
    sel_gui_prof_end(c, SEL_GUI_PROF_IMAGE, t0);
}

void sel_gui_shadow_draw_round(sel_gui_canvas* c, const sel_gui_shadow* s, sel_gui_rect box, int32_t radius, int32_t lift) {
    uint32_t t0 = sel_gui_prof_begin(c);
    shadow_round_raw(c, s, to_canvas(c, box), radius, (int32_t)s->radius, lift / ydv(c), ydv(c));
    sel_gui_prof_end(c, SEL_GUI_PROF_SHADOW, t0);
}

void sel_gui_shadow_draw(sel_gui_canvas* c, const sel_gui_shadow* s, sel_gui_rect box, int32_t radius) {
    uint32_t t0 = sel_gui_prof_begin(c);
    if (radius > 0) {
        shadow_round_raw(c, s, to_canvas(c, box), radius, 0, 0, ydv(c));
    } else {
        shadow_draw_raw(c, s, to_canvas(c, box), ydv(c));
    }
    sel_gui_prof_end(c, SEL_GUI_PROF_SHADOW, t0);
}

uint32_t sel_gui_prof_begin(sel_gui_canvas* c) {
    if (c->clock == NULL) {
        return 0u;
    }
    c->prof_depth++;
    return c->clock(c->clock_ctx);
}

void sel_gui_prof_end(sel_gui_canvas* c, uint32_t cls, uint32_t t0) {
    if (c->clock == NULL) {
        return;
    }
    c->prof_depth--;
    if (c->prof_depth == 0u) {
        c->prof_ms[cls] += c->clock(c->clock_ctx) - t0;
        c->prof_n[cls]++;
    }
}
