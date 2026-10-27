/* See mui_canvas.h. Pure C: no MUI, no Amiga calls (MUI-P5-1, MUI-P5-2, MUI-P5-4). */
#include "mui_canvas.h"

#include <selecta/gui/gui_genart.h>
#include <selecta/gui/gui_image.h>

#include <stdlib.h>

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

uint32_t mui_grid_cols(const mui_grid_style* s, int32_t width) {
    int32_t tw = s->edge + 2 * s->pad;
    return width >= tw + s->gap ? (uint32_t)((width + s->gap) / (tw + s->gap)) : 1u;
}

sel_gui_rect mui_grid_tile_rect(const mui_grid_style* s, int32_t width, uint32_t i) {
    uint32_t cols = mui_grid_cols(s, width);
    int32_t tw = s->edge + 2 * s->pad, th = tw;
    sel_gui_rect r;
    r.x = s->gap + (int32_t)(i % cols) * (tw + s->gap);
    r.y = s->gap + (int32_t)(i / cols) * (th + s->gap);
    r.w = tw;
    r.h = th;
    return r;
}

uint32_t mui_canvas_draw_grid(mui_canvas* m, const mui_grid_style* s, const mui_tile* tiles, uint32_t n, uint32_t cursor,
                              mui_art_fn art, void* art_ctx) {
    uint32_t i, drawn = 0u;
    sel_gui_fill_rect(&m->c, (sel_gui_rect){0, 0, m->c.w, m->c.h}, s->background);
    for (i = 0; i < n; i++) {
        sel_gui_rect t = mui_grid_tile_rect(s, m->c.w, i), ar;
        const uint32_t* px = NULL;
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
            sel_gui_blit_image(&m->c, ar, px, s->edge, s->edge, s->edge, 0);
        } else {
            sel_gui_genart_draw(&m->c, ar, tiles[i].format != NULL ? tiles[i].format : "album",
                                sel_gui_str_hash(tiles[i].title != NULL ? tiles[i].title : ""));
        }
        sel_gui_dirty_add(&m->c, t);
        drawn++;
    }
    m->draws++;
    return drawn;
}

mui_art_path mui_art_path_for(int screen_is_rtg) {
    return screen_is_rtg ? MUI_ART_DATATYPE : MUI_ART_GENART;
}

mui_art_path mui_canvas_draw_art(mui_canvas* m, sel_gui_rect r, mui_art_path want, const uint32_t* src, int32_t sw,
                                 int32_t sh, const char* format, const char* title) {
    if (want == MUI_ART_DATATYPE && src != NULL && sw > 0 && sh > 0 && r.w > 0 && r.h > 0) {
        uint32_t* scaled = malloc((size_t)r.w * (size_t)r.h * sizeof *scaled);
        if (scaled != NULL) {
            sel_gui_image_scale(src, sw, sh, scaled, r.w, r.h);
            sel_gui_blit_image(&m->c, r, scaled, r.w, r.h, r.w, 0);
            free(scaled);
            return MUI_ART_DATATYPE;
        }
    }
    sel_gui_genart_draw(&m->c, r, format != NULL ? format : "album", sel_gui_str_hash(title != NULL ? title : ""));
    return MUI_ART_GENART;
}
