/* Viz hosting and frame budget of the MUI UI (plan 2026-10-25-amiga-mui.md, MUI-P5-3, MUI-P5-5). Pure C so host tests
 * reach it. The viz plugins (scope, levels, spectrum) draw into an INDEX8 surface this file owns, with palette indices
 * as colours (include/selecta/viz.h); the canvas class expands it to the object's pixels. The budget decides how often
 * viz and grid may redraw for the screen's tier, from the same governor the desktop UI uses (sel_uim_budget_for). */
#ifndef MUI_VIZ_H
#define MUI_VIZ_H

#include <selecta/plugin.h>
#include <selecta/uimodel.h>
#include <selecta/viz.h>

#include <stdint.h>

/* Palette indices the plugin receives as sel_viz_colors. */
enum { MUI_VIZ_BACKGROUND = 0, MUI_VIZ_GRID, MUI_VIZ_PRIMARY, MUI_VIZ_SECONDARY, MUI_VIZ_ACCENT, MUI_VIZ_COLORS };

typedef struct mui_viz {
    sel_viz_render_fn fn;
    sel_surface surf;                  /* INDEX8, pitch >= w, 4-byte aligned rows */
    uint8_t* pixels;                   /* owned, pitch * h bytes */
    uint32_t palette[MUI_VIZ_COLORS];  /* 0xAARRGGBB per index, set by the class from the MUI pens */
    uint32_t frames;                   /* VIZ_FRAME calls that drew something: the rig sentinel */
} mui_viz;

/* Finds the VIZ_FRAME hook in a plugin descriptor (what find_plugin(SEL_PLUGIN_KIND_VIZ, id) returns). Returns 0 when
 * the descriptor has none. */
int mui_viz_bind(mui_viz* v, const sel_tag* desc);
/* (Re)creates the INDEX8 surface for w x h; same size keeps the buffer. Returns 0 on empty size or out of memory. */
int mui_viz_fit(mui_viz* v, uint32_t w, uint32_t h);
void mui_viz_release(mui_viz* v);
/* Renders `f` once into the surface; counts the frame when the plugin reports primitives. Returns the primitives. */
uint32_t mui_viz_render(mui_viz* v, const sel_viz_frame* f);
/* Expands the surface through the palette to w x h 0xAARRGGBB words (the canvas pixels). */
void mui_viz_expand(const mui_viz* v, uint32_t* dst);

/* ---- Frame budget (MUI-P5-5) ---- */
typedef enum mui_redraw_kind { MUI_REDRAW_VIZ = 0, MUI_REDRAW_GRID = 1 } mui_redraw_kind;
/* Minimum milliseconds between two redraws of `kind` under budget `b`: true-colour (RTG) 40 ms for both (25 per second);
 * palette (planar) 100 ms for viz (10 per second) and 200 ms for grid (5 per second). */
uint32_t mui_budget_interval_ms(const sel_uim_budget* b, mui_redraw_kind kind);
typedef struct mui_gate {
    uint32_t last_ms;
    int started;
} mui_gate;
/* 1 when a redraw at `now_ms` is allowed (and records it), 0 when it must wait. The first call is always allowed. */
int mui_gate_due(mui_gate* g, uint32_t interval_ms, uint32_t now_ms);

#endif /* MUI_VIZ_H */
