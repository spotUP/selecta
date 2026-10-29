/* Viz hosting and frame budget of the MUI UI (plan 2026-10-25-amiga-mui.md, MUI-P5-3, MUI-P5-5). Pure C so host tests
 * reach it. The viz plugins (scope, levels, spectrum) draw into an INDEX8 surface this file owns, with palette indices
 * as colours (include/selecta/viz.h); the canvas class expands it to the object's pixels. The budget decides how often
 * viz and grid may redraw for the screen's tier, from the same governor the desktop UI uses (sel_uim_budget_for). */
#ifndef MUI_VIZ_H
#define MUI_VIZ_H

#include <selecta/plugin.h>
#include <selecta/uimodel.h>
#include <selecta/uimodel/tokens.h>
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

/* Fills the palette from theme tokens (0xRRGGBB, SEL_GUI_TOKEN_COUNT of them) with the desktop strip's mapping: background =
 * surface, grid = raised surface, primary = accent, secondary = muted text, accent = text. */
void mui_viz_palette_from_tokens(mui_viz* v, const uint32_t* tokens);
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

/* ---- Plugin set and tick (MUI-P5-3): what the Now Playing viz area shows ---- */
#define MUI_VIZ_PLUGINS 3u
typedef const sel_tag* (*mui_find_plugin_fn)(uint32_t kind, const char* id); /* sel_ui_open_args.find_plugin */
typedef struct mui_vizset {
    const char* id[MUI_VIZ_PLUGINS];
    const char* name_[MUI_VIZ_PLUGINS + 1u]; /* display names, NULL terminated: the cycle gadget's entries */
    sel_viz_render_fn fn[MUI_VIZ_PLUGINS];
    uint32_t n, sel;     /* plugins found, index of the selected one */
    uint32_t seq;        /* sequence number of the newest frame a tick allowed */
    mui_gate gate;       /* frame budget (MUI-P5-5) */
    uint32_t allowed;    /* ticks that allowed a render */
} mui_vizset;
/* Finds the viz plugins the desktop UI lists (scope, levels, spectrum: the ids of the View/Visualizer actions in the shared
 * action table) with find_plugin(SEL_PLUGIN_KIND_VIZ, id), and selects `pref` (the ui.desktop.viz pref value, NULL or unknown =
 * first). Returns how many were found. */
uint32_t mui_vizset_load(mui_vizset* s, mui_find_plugin_fn find, const char* pref);
/* Selects the plugin with action/plugin id `id`; returns its index, -1 when not found. */
int mui_vizset_select(mui_vizset* s, const char* id);
/* The engine line that persists the selection (the line the View/Visualizer menu sends), NULL when none is loaded. */
const char* mui_vizset_line(const mui_vizset* s);
/* 1 when a render of newest engine frame `f` is due at now_ms: a plugin is loaded, the frame is new (seq not 0, not the one
 * last allowed) and the frame budget lets the tier redraw. Records the frame and time when it returns 1. */
int mui_vizset_tick(mui_vizset* s, const sel_viz_frame* f, const sel_uim_budget* b, uint32_t now_ms);
/* The whole per-frame step the viz area runs (tick, then render and expand): returns 1 when a w x h frame was rendered into
 * `v` (the selected plugin, v->frames counts it) and expanded to `dst` (w * h words). 0 when the tick says wait or the
 * surface does not fit. This is the one entry point the class and the host test both drive. */
int mui_vizset_step(mui_vizset* s, mui_viz* v, const sel_viz_frame* f, const sel_uim_budget* b, uint32_t now_ms, uint32_t w,
                    uint32_t h, uint32_t* dst);

#endif /* MUI_VIZ_H */
