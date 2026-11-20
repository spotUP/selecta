#include "mui_grid_art.h"

#include <selecta/gui/gui_image.h>

#include <stdlib.h>
#include <string.h>

#define GRID_THUMB_KIND "thumbf144"
#define GRID_THUMB_PX ((size_t)MUI_GRID_THUMB_EDGE * MUI_GRID_THUMB_EDGE)
#define GRID_THUMB_MAX (SEL_GUI_THUMB_HEAD + 4u * (uint32_t)GRID_THUMB_PX)
#define GRID_BAD 64u /* albums without a usable cover, a ring: not retried every tick */

struct mui_grid_art {
    const sel_ui_art* art;
    mui_grid_decode_fn decode;
    sel_gui_art_cache* cache;
    int32_t edge;
    uint32_t decodes;
    int64_t bad[GRID_BAD];
    uint32_t bad_n;
};

mui_grid_art* mui_grid_art_open(const sel_ui_art* art, mui_grid_decode_fn decode, int32_t edge, uint8_t slots) {
    uint16_t e[SEL_GUI_ART_SIZES] = {1, 1, 1, 1};
    uint8_t s[SEL_GUI_ART_SIZES] = {0, 0, 0, 0};
    mui_grid_art* g;
    if (art == NULL || art->get == NULL || decode == NULL || edge < 1 || edge > MUI_GRID_THUMB_EDGE || slots == 0u) {
        return NULL;
    }
    g = (mui_grid_art*)calloc(1, sizeof *g);
    if (g == NULL) {
        return NULL;
    }
    e[SEL_GUI_ART_TILE] = (uint16_t)edge;
    s[SEL_GUI_ART_TILE] = slots;
    g->cache = sel_gui_art_cache_open(e, s);
    if (g->cache == NULL) {
        free(g);
        return NULL;
    }
    g->art = art;
    g->decode = decode;
    g->edge = edge;
    return g;
}

void mui_grid_art_close(mui_grid_art* g) {
    if (g != NULL) {
        sel_gui_art_cache_close(g->cache);
        free(g);
    }
}

const uint32_t* mui_grid_art_get(void* g, int64_t art_id) {
    return art_id != 0 ? sel_gui_art_cache_get(((mui_grid_art*)g)->cache, SEL_GUI_ART_TILE, art_id) : NULL;
}

uint32_t mui_grid_art_decodes(const mui_grid_art* g) {
    return g->decodes;
}

static int is_bad(const mui_grid_art* g, int64_t id) {
    uint32_t i;
    for (i = 0; i < g->bad_n && i < GRID_BAD; i++) {
        if (g->bad[i] == id) {
            return 1;
        }
    }
    return 0;
}

/* Fills th (144 squared) from the thumbnail row, else from the decoded cover and writes the row back. 0: no usable cover. */
static int load_thumb(mui_grid_art* g, int64_t album, uint32_t* th) {
    uint8_t* blob = (uint8_t*)malloc(GRID_THUMB_MAX);
    uint32_t len;
    int32_t w = 0, h = 0;
    int ok = 0;
    if (blob == NULL) {
        return 0;
    }
    len = g->art->get(g->art->ctx, album, GRID_THUMB_KIND, blob, GRID_THUMB_MAX);
    if (len > 0u && len <= GRID_THUMB_MAX && sel_gui_thumb_unpack(blob, len, th, (uint32_t)GRID_THUMB_PX, &w, &h) &&
        w == MUI_GRID_THUMB_EDGE && h == MUI_GRID_THUMB_EDGE) {
        ok = 1;
    } else {
        uint32_t* img = g->decode(g->art, album, 0u, &w, &h);
        if (img != NULL) {
            g->decodes++;
            if (sel_gui_image_fit_square(img, w, h, th, MUI_GRID_THUMB_EDGE)) {
                len = sel_gui_thumb_pack(th, MUI_GRID_THUMB_EDGE, MUI_GRID_THUMB_EDGE, blob, GRID_THUMB_MAX);
                if (len > 0u && g->art->put != NULL) {
                    g->art->put(g->art->ctx, album, GRID_THUMB_KIND, blob, len);
                }
                ok = 1;
            }
            free(img);
        }
    }
    free(blob);
    return ok;
}

int64_t mui_grid_art_tick(mui_grid_art* g, const mui_tile* tiles, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        int64_t id = tiles[i].art_id;
        uint32_t* th;
        int ok;
        if (id == 0 || is_bad(g, id) || sel_gui_art_cache_get(g->cache, SEL_GUI_ART_TILE, id) != NULL) {
            continue;
        }
        th = (uint32_t*)malloc(GRID_THUMB_PX * sizeof *th);
        ok = th != NULL && load_thumb(g, id, th);
        if (ok) {
            uint32_t* dst = sel_gui_art_cache_put(g->cache, SEL_GUI_ART_TILE, id);
            if (g->edge == MUI_GRID_THUMB_EDGE) {
                memcpy(dst, th, GRID_THUMB_PX * sizeof *th);
            } else {
                sel_gui_image_scale(th, MUI_GRID_THUMB_EDGE, MUI_GRID_THUMB_EDGE, dst, g->edge, g->edge);
            }
        } else {
            g->bad[g->bad_n++ % GRID_BAD] = id;
        }
        free(th);
        return ok ? id : 0;
    }
    return 0;
}
