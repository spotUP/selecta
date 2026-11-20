/* See mui_canvas.h. Pure C: no MUI, no Amiga calls (MUI-P5-1, MUI-P5-2, MUI-P5-4). */
#include "mui_canvas.h"

#include <selecta/gui/gui_genart.h>
#include <selecta/gui/gui_icons.h>
#include <selecta/gui/gui_image.h>

#include <selecta/platform.h>
#include <selecta/uimodel/grid.h>
#include <selecta/uimodel/tokens.h>

#include <stdlib.h>
#include <string.h>

/* One size of the baked icon set, unpacked on first use (I1399a): the largest baked size not above `px`. The kind icons are a
 * handful of masks, so the whole set at one cover-sized edge is small (about 40 KB at 32 px); a later larger edge rebuilds it. */
static const sel_gui_icons_packed* mui_icon_src; /* set by the UI plugin; NULL draws covers without the icon */

void mui_canvas_set_icons(const sel_gui_icons_packed* baked) {
    mui_icon_src = baked;
}

static const sel_gui_icons* mui_cover_icons(int32_t px) {
    static sel_gui_icons ic;
    static int32_t size;
    static const uint8_t* alpha;
    static uint8_t* buf;
    uint32_t s, best = 0u, k;
    size_t sz;
    const sel_gui_icons_packed* b = mui_icon_src;

    if (b == NULL || b->size_n == 0u) {
        return NULL;
    }
    for (s = 0u; s < b->size_n; s++) {
        if (b->sizes[s] <= px) {
            best = s;
        }
    }
    if (buf != NULL && size == b->sizes[best]) {
        return &ic;
    }
    size = b->sizes[best];
    sz = (size_t)size * (size_t)size;
    free(buf);
    buf = malloc(sz * b->count);
    if (buf == NULL) {
        return NULL; /* no icon: the cover keeps its tint */
    }
    for (k = 0u; k < b->count; k++) {
        sel_gui_icon_unpack(b, best, k, buf + k * sz);
    }
    alpha = buf;
    ic.count = b->count;
    ic.size_n = 1u;
    ic.sizes = &size;
    ic.alpha = &alpha;
    return &ic;
}

/* The cover of a tile or art object (I691e, I1399a): the kind's icon on its per-kind tint (SEL_COVER_KIND_FORMAT: the family of the
 * format extension), in the default theme's cover tokens (the MUI canvas carries no theme of its own). The MUI canvas has no text
 * atlas (the MUI side draws text with TrueType), so the kind label and title are the caller's MUI text, not drawn here. */
static void mui_platform_cover(sel_gui_canvas* c, sel_gui_rect r, uint32_t kind, const char* format, const char* title) {
    static uint32_t tokens[SEL_GUI_TOKEN_COUNT];
    static int ready;
    int32_t edge = r.w < r.h ? r.w : r.h;

    if (!ready) {
        int t;
        for (t = 0; t < (int)SEL_GUI_TOKEN_COUNT; t++) {
            tokens[t] = sel_gui_token_default((sel_gui_token)t);
        }
        ready = 1;
    }
    const sel_gui_icons* ic = mui_cover_icons(edge / 2);
    uint32_t family = sel_platform_of_ext(format), hash = sel_gui_str_hash(title != NULL ? title : "");

    if (ic == NULL && kind == 0u) { /* no icon set: the family mark cover, as before I1399a */
        sel_gui_cover_draw(c, r, family, hash, tokens, NULL);
        return;
    }
    sel_gui_cover_draw_kind(c, r, kind, family, hash, title, tokens, NULL, NULL, ic);
}

int mui_canvas_fit(mui_canvas* m, sel_gui_rect obj) {
    if (obj.w <= 0 || obj.h <= 0) {
        mui_canvas_release(m);
        m->obj = obj;
        return 0;
    }
    if (m->pix == NULL || m->c.w != obj.w || m->c.h != obj.h) {
        free(m->pix);
        m->pix = malloc(sel_gui_pixel_bytes(SEL_GUI_RGBA32, obj.w, obj.h));
        if (m->pix == NULL) {
            m->c.w = m->c.h = 0;
            return 0;
        }
    }
    sel_gui_canvas_init(&m->c, SEL_GUI_RGBA32, obj.w, obj.h, m->pix);
    m->obj = obj;
    return 1;
}

void mui_canvas_release(mui_canvas* m) {
    free(m->pix);
    m->pix = NULL;
    m->c.w = m->c.h = 0;
}

sel_gui_rect mui_canvas_window_clip(const mui_canvas* m) {
    return m->obj;
}

uint32_t mui_grid_tiles(const sel_view_state* v, mui_tile* out, uint32_t cap) {
    uint32_t k, n = 0;
    if (v == NULL || v->rows == NULL) {
        return 0u;
    }
    for (k = 0; k < v->row_n && n < cap; k++) {
        const sel_view_row* r = &v->rows[k];
        if (r->kind == (uint32_t)SEL_ROW_HEADING) {
            continue;
        }
        out[n].title = r->text;
        out[n].artist = r->artist;
        out[n].format = r->format;
        out[n].art_id = (int64_t)r->art_id;
        out[n].kind = sel_cover_kind_of_item(r->item_kind, r->album != 0u, r->composer != 0u, r->key);
        n++;
    }
    return n;
}

mui_grid_style mui_grid_style_for(int32_t screen_w, uint32_t background, uint32_t tile, uint32_t selected) {
    mui_grid_style s = {background, tile, selected, screen_w >= 1024 ? 128 : 96, 8, 8, 0};
    return s;
}

uint32_t mui_grid_cols(const mui_grid_style* s, int32_t width) {
    int32_t tw = s->edge + 2 * s->pad;
    return sel_uim_grid_cols(width, tw, s->gap);
}

sel_gui_rect mui_grid_tile_rect(const mui_grid_style* s, int32_t width, uint32_t i) {
    uint32_t cols = mui_grid_cols(s, width);
    int32_t tw = s->edge + 2 * s->pad, th = tw + s->caption;
    sel_gui_rect r;
    r.x = s->gap + (int32_t)(i % cols) * (tw + s->gap);
    r.y = s->gap + (int32_t)(i / cols) * (th + s->gap);
    r.w = tw;
    r.h = th;
    return r;
}

uint32_t mui_grid_visible_rows(const mui_grid_style* s, int32_t height) {
    int32_t th = s->edge + 2 * s->pad + s->caption, v = th + s->gap > 0 ? (height - s->gap) / (th + s->gap) : 1;
    return v > 1 ? (uint32_t)v : 1u;
}

uint32_t mui_grid_first_row(const mui_grid_style* s, int32_t width, int32_t height, uint32_t cursor) {
    return sel_uim_grid_first_row(cursor, mui_grid_cols(s, width), (int32_t)mui_grid_visible_rows(s, height));
}

uint32_t mui_grid_signature(const mui_tile* tiles, uint32_t n, uint32_t cursor) {
    uint32_t h = 2166136261u ^ n, i;
    const char* p;
    h = (h ^ cursor) * 16777619u;
    for (i = 0; i < n; i++) {
        h = (h ^ (uint32_t)tiles[i].art_id) * 16777619u;
        h = (h ^ tiles[i].kind) * 16777619u;
        for (p = tiles[i].title; p != NULL && *p != '\0'; p++) {
            h = (h ^ (uint8_t)*p) * 16777619u;
        }
        h = (h ^ 0xFFu) * 16777619u;
        for (p = tiles[i].artist; p != NULL && *p != '\0'; p++) {
            h = (h ^ (uint8_t)*p) * 16777619u;
        }
        h = (h ^ 0xFEu) * 16777619u;
    }
    return h;
}

/* I1473m: the Mac twin cover_radius() (plugins/ui/desktop/desktop_ui.c): the large card radius from 96 px up, else the small one, from the
 * same default tokens. shortcut: the default tokens, not the user theme, because the MUI class has no theme; upgrade with a theme token feed. */
static int32_t mui_cover_radius(int32_t edge) {
    int32_t r = (int32_t)sel_gui_token_default(edge >= 96 ? SEL_GUI_TOKEN_RADIUS_LARGE : SEL_GUI_TOKEN_RADIUS);
    return r > edge / 2 ? edge / 2 : r;
}

/* I1473m: rounds the four corners of `r` after a generated cover drew, by repainting the pixels outside the quarter circle with the
 * card colour `bg`: integer only, no alpha, a few dozen stores per cover (the 68020 cost of the blit stays). */
static void mui_round_corners(sel_gui_canvas* c, sel_gui_rect r, int32_t rad, uint32_t bg) {
    int32_t y, x;
    for (y = 0; y < rad; y++) {
        for (x = 0; x < rad; x++) {
            int32_t dx = rad - x, dy = rad - y; /* pixel centre to the arc centre, doubled below */
            if ((2 * dx - 1) * (2 * dx - 1) + (2 * dy - 1) * (2 * dy - 1) > 4 * rad * rad) {
                sel_gui_fill_rect(c, (sel_gui_rect){r.x + x, r.y + y, 1, 1}, bg);
                sel_gui_fill_rect(c, (sel_gui_rect){r.x + r.w - 1 - x, r.y + y, 1, 1}, bg);
                sel_gui_fill_rect(c, (sel_gui_rect){r.x + x, r.y + r.h - 1 - y, 1, 1}, bg);
                sel_gui_fill_rect(c, (sel_gui_rect){r.x + r.w - 1 - x, r.y + r.h - 1 - y, 1, 1}, bg);
            }
        }
    }
}

uint32_t mui_canvas_draw_grid(mui_canvas* m, const mui_grid_style* s, const mui_tile* tiles, uint32_t n, uint32_t cursor,
                              uint32_t first_row, mui_art_fn art, void* art_ctx) {
    uint32_t i, drawn = 0u, cols = mui_grid_cols(s, m->c.w);
    int32_t shift = (int32_t)first_row * (s->edge + 2 * s->pad + s->caption + s->gap);
    sel_gui_fill_rect(&m->c, (sel_gui_rect){0, 0, m->c.w, m->c.h}, s->background);
    for (i = 0; i < n; i++) {
        sel_gui_rect t = mui_grid_tile_rect(s, m->c.w, i), ar;
        const uint32_t* px = NULL;
        if (i / cols < first_row) {
            continue;
        }
        t.y -= shift;
        if (t.y >= m->c.h) {
            break;
        }
        ar.x = t.x + s->pad;
        ar.y = t.y + s->pad;
        ar.w = ar.h = s->edge;
        if (i == cursor) {
            sel_gui_round_rect(&m->c, (sel_gui_rect){t.x - 2, t.y - 2, t.w + 4, t.h + 4}, s->pad + 2, s->selected);
        }
        sel_gui_round_rect(&m->c, t, s->pad, s->tile);
        if (art != NULL && tiles[i].art_id != 0) {
            px = art(art_ctx, tiles[i].art_id);
        }
        if (px != NULL) {
            sel_gui_blit_image_round(&m->c, ar, px, s->edge, s->edge, s->edge, mui_cover_radius(s->edge));
        } else {
            mui_platform_cover(&m->c, ar, tiles[i].kind, tiles[i].format, tiles[i].title);
            mui_round_corners(&m->c, ar, mui_cover_radius(s->edge), s->tile);
        }
        sel_gui_dirty_add(&m->c, t);
        drawn++;
    }
    m->draws++;
    return drawn;
}

struct mui_aga_cache {
    int32_t edge;
    uint32_t slots, next;
    int64_t* ids;
    uint8_t* idx; /* slots * edge * edge */
};

mui_aga_cache* mui_aga_cache_open(int32_t edge, uint32_t slots) {
    mui_aga_cache* c;
    if (edge <= 0 || slots == 0u) {
        return NULL;
    }
    c = calloc(1, sizeof *c);
    if (c != NULL) {
        c->edge = edge;
        c->slots = slots;
        c->ids = calloc(slots, sizeof *c->ids); /* id 0 = free slot (art_id 0 means no cover) */
        c->idx = malloc((size_t)slots * (size_t)edge * (size_t)edge);
    }
    if (c == NULL || c->ids == NULL || c->idx == NULL) {
        mui_aga_cache_close(c);
        return NULL;
    }
    return c;
}

void mui_aga_cache_close(mui_aga_cache* c) {
    if (c != NULL) {
        free(c->ids);
        free(c->idx);
        free(c);
    }
}

static const uint8_t* mui_aga_cache_get(mui_aga_cache* c, int64_t id, mui_art_fn art, void* ctx) {
    uint32_t i;
    size_t sz = (size_t)c->edge * (size_t)c->edge;
    const uint32_t* px;
    for (i = 0; i < c->slots; i++) {
        if (c->ids[i] == id) {
            return c->idx + i * sz;
        }
    }
    px = art != NULL ? art(ctx, id) : NULL;
    if (px == NULL) {
        return NULL;
    }
    i = c->next++ % c->slots;
    mui_cover_remap(px, c->edge, c->idx + i * sz);
    c->ids[i] = id;
    return c->idx + i * sz;
}

uint32_t mui_tile_tint(const mui_tile* t) {
    return sel_gui_token_default((sel_gui_token)sel_cover_kind_token(t->kind != 0u ? t->kind : SEL_COVER_KIND_FORMAT)) & 0xFFFFFFu;
}

uint32_t mui_canvas_draw_grid_aga(const mui_grid_style* s, int32_t w, int32_t h, const mui_tile* tiles, uint32_t n, uint32_t cursor,
                                  uint32_t first_row, mui_art_fn art, void* art_ctx, mui_aga_cache* cache, const mui_aga_sink* sink,
                                  void* sink_ctx) {
    uint32_t i, drawn = 0u, cols = mui_grid_cols(s, w);
    int32_t shift = (int32_t)first_row * (s->edge + 2 * s->pad + s->caption + s->gap);
    sink->rect(sink_ctx, (sel_gui_rect){0, 0, w, h}, s->background & 0xFFFFFFu);
    for (i = first_row * cols; i < n; i++) {
        sel_gui_rect t = mui_grid_tile_rect(s, w, i), ar;
        const uint8_t* idx = NULL;
        t.y -= shift;
        if (t.y >= h) {
            break;
        }
        ar = (sel_gui_rect){t.x + s->pad, t.y + s->pad, s->edge, s->edge};
        if (i == cursor) { /* the outline is the larger rect under the card, as the RTG round rect */
            sink->rect(sink_ctx, (sel_gui_rect){t.x - 2, t.y - 2, t.w + 4, t.h + 4}, s->selected & 0xFFFFFFu);
        }
        sink->rect(sink_ctx, t, s->tile & 0xFFFFFFu);
        if (cache != NULL && cache->edge == s->edge && tiles[i].art_id != 0) {
            idx = mui_aga_cache_get(cache, tiles[i].art_id, art, art_ctx);
        }
        sink->cover(sink_ctx, ar, idx, mui_tile_tint(&tiles[i]));
        drawn++;
    }
    return drawn;
}

mui_art_path mui_art_path_for(int screen_is_rtg) {
    return screen_is_rtg ? MUI_ART_DATATYPE : MUI_ART_COVER;
}

mui_art_path mui_canvas_draw_art(mui_canvas* m, sel_gui_rect r, mui_art_path want, const uint32_t* src, int32_t sw,
                                 int32_t sh, uint32_t kind, const char* format, const char* title) {
    if (want == MUI_ART_DATATYPE && src != NULL && sw > 0 && sh > 0 && r.w > 0 && r.h > 0) {
        uint32_t* scaled = malloc((size_t)r.w * (size_t)r.h * sizeof *scaled);
        if (scaled != NULL) {
            sel_gui_image_scale(src, sw, sh, scaled, r.w, r.h);
            sel_gui_blit_image(&m->c, r, scaled, r.w, r.h, r.w, 0);
            free(scaled);
            return MUI_ART_DATATYPE;
        }
    }
    mui_platform_cover(&m->c, r, kind, format, title);
    return MUI_ART_COVER;
}

int mui_image_square(const uint32_t* src, int32_t w, int32_t h, uint32_t* dst, int32_t edge) {
    int32_t side, x0, y0, y;
    uint32_t* sq;
    if (src == NULL || dst == NULL || w <= 0 || h <= 0 || edge <= 0) {
        return 0;
    }
    side = w < h ? w : h;
    x0 = (w - side) / 2;
    y0 = (h - side) / 2;
    sq = malloc((size_t)side * (size_t)side * sizeof *sq);
    if (sq == NULL) {
        return 0;
    }
    for (y = 0; y < side; y++) {
        memcpy(sq + (size_t)y * (size_t)side, src + (size_t)(y0 + y) * (size_t)w + (size_t)x0, (size_t)side * sizeof *sq);
    }
    sel_gui_image_scale(sq, side, side, dst, edge, edge);
    free(sq);
    return 1;
}

/* I1440q4: AGA cover. 3 levels per channel = a 27 colour cube; a 4 x 4 ordered dither picks the level, so the 68020 does three adds,
 * three divides by a constant and one store per pixel (no error buffer, no search over the palette). The pens come from the screen
 * (ObtainBestPen, shared), the index maps through a 27 entry pen table. */
static const uint8_t g_bayer4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

uint32_t mui_cover_cube_rgb(uint32_t index) {
    static const uint32_t lvl[3] = {0x00u, 0x80u, 0xFFu};
    return index >= MUI_COVER_CUBE ? 0u : (lvl[index / 9u] << 16) | (lvl[(index / 3u) % 3u] << 8) | lvl[index % 3u];
}

int mui_cover_remap(const uint32_t* src, int32_t edge, uint8_t* idx) {
    int32_t x, y;
    if (src == NULL || idx == NULL || edge <= 0) {
        return 0;
    }
    for (y = 0; y < edge; y++) {
        for (x = 0; x < edge; x++) {
            uint32_t p = src[(size_t)y * (size_t)edge + (size_t)x];
            uint32_t d = (uint32_t)g_bayer4[((uint32_t)y & 3u) * 4u + ((uint32_t)x & 3u)] * 16u + 8u; /* 8..248 */
            uint32_t r = (((p >> 16) & 0xFFu) * 2u + d) / 255u;
            uint32_t g = (((p >> 8) & 0xFFu) * 2u + d) / 255u;
            uint32_t b = ((p & 0xFFu) * 2u + d) / 255u;
            idx[(size_t)y * (size_t)edge + (size_t)x] = (uint8_t)(r * 9u + g * 3u + b);
        }
    }
    return 1;
}

void mui_cover_scale8(const uint8_t* src, int32_t sedge, uint8_t* dst, int32_t dw, int32_t dh) {
    int32_t x, y;
    for (y = 0; y < dh; y++) {
        const uint8_t* row = src + (size_t)(((int64_t)y * sedge) / dh) * (size_t)sedge;
        for (x = 0; x < dw; x++) {
            dst[(size_t)y * (size_t)dw + (size_t)x] = row[((int64_t)x * sedge) / dw];
        }
    }
}
