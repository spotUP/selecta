/* Rasterizer and canvas of the desktop UI (plan DUI-P2-1 and DUI-P2-2, section 2.5). Integer and Q16.16 only: no
 * floating point anywhere under src/gui/ (checked by the dui-draw lint). Two canvas formats: RGBA32 (0xAARRGGBB
 * words) and INDEX8 (palette indices, nearest entry through a 32x32x32 cube built when the palette is set). The
 * clip stack is 16 deep; the dirty list holds 64 disjoint rectangles and merges by bounding box when it is full.
 * No allocation: the caller owns the pixel buffer. */
#ifndef SELECTA_GUI_DRAW_H
#define SELECTA_GUI_DRAW_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sel_gui_rect {
    int32_t x, y, w, h;
} sel_gui_rect;

enum { SEL_GUI_RGBA32 = 0, SEL_GUI_INDEX8 = 1 };
#define SEL_GUI_CLIP_DEPTH 16u
#define SEL_GUI_DIRTY_MAX 64u
#define SEL_GUI_SHADOW_MAX 32u

/* INDEX8 colour budget (DUI-P6-4, AGA): entries 0-31 UI, 32-63 generated art, 64-127 tile row, 128-191 panel,
 * 192-255 cover flow. Draws in the UI and art regions resolve colours in entries 0-63; the three artwork regions
 * each resolve in their own 64-entry sub-palette. */
enum { SEL_GUI_RGN_UI = 0, SEL_GUI_RGN_ART, SEL_GUI_RGN_TILE, SEL_GUI_RGN_PANEL, SEL_GUI_RGN_FLOW, SEL_GUI_RGN_COUNT };

/* I389: paint profile classes, one per kind of primitive. */
enum { SEL_GUI_PROF_FILL = 0, SEL_GUI_PROF_SCRIM, SEL_GUI_PROF_ROUND, SEL_GUI_PROF_GRADIENT, SEL_GUI_PROF_LINE,
       SEL_GUI_PROF_TEXT, SEL_GUI_PROF_MASK, SEL_GUI_PROF_IMAGE, SEL_GUI_PROF_SHADOW, SEL_GUI_PROF_ICON,
       SEL_GUI_PROF_GENART,
       /* I395: the non-primitive paint costs: dirty-rect merge, clip and region set, palette rebuild (cube), tile quantise, art lookup */
       SEL_GUI_PROF_DIRTY, SEL_GUI_PROF_CLIP, SEL_GUI_PROF_PALETTE, SEL_GUI_PROF_QUANT, SEL_GUI_PROF_ART, SEL_GUI_PROF_COUNT };

typedef struct sel_gui_canvas {
    uint32_t format; /* SEL_GUI_RGBA32 or SEL_GUI_INDEX8 */
    int32_t w, h;
    void* pixels;    /* w*h uint32_t (RGBA32) or uint8_t (INDEX8), row stride = w */
    uint32_t palette[256]; /* 0xRRGGBB, INDEX8 only */
    uint32_t colours;      /* palette entries in use */
    uint8_t cube[32768];   /* INDEX8: nearest of entries 0-31 (the UI region) for each 5-bit-per-channel colour */
    uint8_t rcube[4][32768]; /* INDEX8: nearest within each region sub-palette (art 32-63, tile, panel, flow) */
    uint32_t region;         /* SEL_GUI_RGN_*: the budget region the draws below belong to */
    uint32_t used[SEL_GUI_RGN_COUNT][8]; /* INDEX8: bitmap of palette indices written per region */
    sel_gui_rect clip[SEL_GUI_CLIP_DEPTH];
    uint32_t clip_n;
    sel_gui_rect dirty[SEL_GUI_DIRTY_MAX];
    uint32_t dirty_n;
    uint32_t span_px; /* pixels written by run-length spans (fills, rounded bodies, vertical gradient rows): the sentinel for I401 */
    uint32_t prims; /* primitives run since init: the sentinel that the rasterizer was reached */
    /* I389: optional paint profile. With `clock` set, each outermost primitive adds its elapsed ms (clock deltas) and
     * a call to prof_ms / prof_n by class; a primitive nested in another counts for the outer one only. Draw output is
     * not affected. */
    uint32_t (*clock)(void* ctx);
    void* clock_ctx;
    uint32_t prof_depth;
    uint32_t prof_ms[SEL_GUI_PROF_COUNT];
    uint32_t prof_n[SEL_GUI_PROF_COUNT];
} sel_gui_canvas;

/* Bytes of pixel storage for a format and size. */
size_t sel_gui_pixel_bytes(uint32_t format, int32_t w, int32_t h);
/* Binds `pixels` (sel_gui_pixel_bytes long), clears it to 0, resets clip and dirty list. INDEX8 starts with a
 * black palette entry 0 only. */
void sel_gui_canvas_init(sel_gui_canvas* c, uint32_t format, int32_t w, int32_t h, void* pixels);
/* INDEX8: sets palette entries first .. first+n-1 (0xRRGGBB) and rebuilds the lookup cube. */
void sel_gui_set_palette(sel_gui_canvas* c, const uint32_t* rgb, uint32_t first, uint32_t n);
/* Selects the budget region for the draws that follow (INDEX8; harmless on RGBA32). */
void sel_gui_set_region(sel_gui_canvas* c, uint32_t region);
/* Distinct palette indices written in a region since init. */
uint32_t sel_gui_region_colours(const sel_gui_canvas* c, uint32_t region);
/* The budget palette: `ui` holds n theme colours (0xRRGGBB; ui[0] accent, ui[1] surface, ui[2] text, n >= 3, n <= 32)
 * for entries 0-31, the rest of those 32 are steps from the surface to the accent and the text; entries 32-63 are
 * the generated-art colours (sel_gui_genart_palette); each artwork sub-palette starts as a grey ramp. */
void sel_gui_palette_budget(uint32_t* rgb256, const uint32_t* ui, uint32_t n);
/* The default 256-colour UI palette from three theme colours: 3 theme entries, 24 greys, a 6x6x6 cube, then
 * surface-to-accent steps (now: sel_gui_palette_budget over the three colours). */
void sel_gui_palette_default(uint32_t* rgb256, uint32_t accent, uint32_t surface, uint32_t text);
/* Colour as 0xRRGGBB of pixel (x, y); 0 outside the canvas. */
uint32_t sel_gui_pixel(const sel_gui_canvas* c, int32_t x, int32_t y);
/* 64-bit FNV-1a, the one hash of every golden render (plan 4.2). */
#define SEL_GUI_FNV64_BASIS 0xcbf29ce484222325ull
#define SEL_GUI_FNV64_PRIME 0x00000100000001b3ull
/* Folds `n` bytes into `h` (start from SEL_GUI_FNV64_BASIS); "a" from the basis gives af63dc4c8601ec8c. */
uint64_t sel_gui_fnv64(uint64_t h, const void* bytes, size_t n);
/* Folds a 32-bit word as its four bytes, least significant first (the same on every host byte order). */
uint64_t sel_gui_fnv64_word(uint64_t h, uint32_t word);
/* Hash of the canvas pixels: every pixel word through sel_gui_fnv64_word (INDEX8: the index byte). */
uint64_t sel_gui_hash(const sel_gui_canvas* c);
/* `h` as 16 lower-case hex digits and a NUL, the form golden files and the stats carry. */
void sel_gui_hash_hex(uint64_t h, char out[17]);

/* Clip stack: push intersects with the current clip; every primitive draws inside it. 0 when full. */
int sel_gui_clip_push(sel_gui_canvas* c, sel_gui_rect r);
void sel_gui_clip_pop(sel_gui_canvas* c);

/* Dirty list. add: contained rectangles vanish, overlapping ones merge, and a full list merges the pair whose
 * bounding box grows least. present: returns the pixel count of the list (exact, the rectangles are disjoint)
 * and empties it. */
void sel_gui_dirty_add(sel_gui_canvas* c, sel_gui_rect r);
uint32_t sel_gui_dirty_pixels(const sel_gui_canvas* c);
uint32_t sel_gui_present(sel_gui_canvas* c);
int sel_gui_rect_intersects(sel_gui_rect a, sel_gui_rect b);
sel_gui_rect sel_gui_rect_intersect(sel_gui_rect a, sel_gui_rect b);

/* Colours passed to primitives are 0xAARRGGBB; alpha 255 is opaque. */
void sel_gui_fill_rect(sel_gui_canvas* c, sel_gui_rect r, uint32_t argb);
/* Rounded rectangle: 4x4 coverage anti-aliasing on RGBA32; INDEX8 keeps the pixels covered by half or more. */
void sel_gui_round_rect(sel_gui_canvas* c, sel_gui_rect r, int32_t radius, uint32_t argb);
/* Outline of a rounded rectangle: the band `width` pixels wide just inside r, same corner radius and anti-aliasing
 * as sel_gui_round_rect, so a ring drawn on the rect a pill was filled with hugs the pill. */
void sel_gui_round_frame(sel_gui_canvas* c, sel_gui_rect r, int32_t radius, int32_t width, uint32_t argb);
/* Gradient from `a` to `b` along x (vertical = 0) or y (vertical = 1); Q16 step. INDEX8 dithers with an ordered
 * 4x4 matrix. */
void sel_gui_gradient(sel_gui_canvas* c, sel_gui_rect r, int vertical, uint32_t a, uint32_t b);
/* One-pixel line, Bresenham. */
void sel_gui_line(sel_gui_canvas* c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t argb);
/* Alpha mask blit: coverage bytes scaled by the tint's alpha (glyphs, icons). */
void sel_gui_blit_mask(sel_gui_canvas* c, int32_t x, int32_t y, const uint8_t* mask, int32_t mw, int32_t mh,
                       int32_t mstride, uint32_t tint_argb);
/* Image blit of 0xAARRGGBB words into `dst`. box = 0 nearest; box = 1 averages the source box of each pixel when
 * shrinking. */
void sel_gui_blit_image(sel_gui_canvas* c, sel_gui_rect dst, const uint32_t* src, int32_t sw, int32_t sh,
                        int32_t sstride, int box);
/* Nearest image blit with the corners rounded to `radius` (I477): edge pixels blend the source over the canvas by the
 * rounded rect's coverage, so a card behind the image shows through at the corners. */
void sel_gui_blit_image_round(sel_gui_canvas* c, sel_gui_rect dst, const uint32_t* src, int32_t sw, int32_t sh,
                              int32_t sstride, int32_t radius);
/* Scrim: a translucent fill (dim behind a modal). */
void sel_gui_scrim(sel_gui_canvas* c, sel_gui_rect r, uint32_t rgb, uint32_t alpha);

/* Drop shadow as a pre-baked 9-slice: the corner table is built once from the token radius. */
typedef struct sel_gui_shadow {
    uint32_t radius;
    uint32_t rgb;
    uint8_t corner[SEL_GUI_SHADOW_MAX * SEL_GUI_SHADOW_MAX]; /* alpha at offset (i, j) from the box corner */
} sel_gui_shadow;
void sel_gui_shadow_bake(sel_gui_shadow* s, uint32_t radius, uint32_t rgb, uint32_t peak_alpha);
/* Draws the shadow ring around `box` (the box itself is left alone). */
void sel_gui_shadow_draw(sel_gui_canvas* c, const sel_gui_shadow* s, sel_gui_rect box);
/* Soft shadow of a rounded box (corner `radius`), I507: follows the rounded corners and sits `lift` px lower, so nothing
 * shows above or at the sides of the box edge and the shadow gathers under it. Nothing on a palette canvas. */
void sel_gui_shadow_draw_round(sel_gui_canvas* c, const sel_gui_shadow* s, sel_gui_rect box, int32_t radius, int32_t lift);

#ifdef __cplusplus
}
#endif
#endif

/* I389: profile bracket for a primitive; pairs with sel_gui_prof_end. Returns the start time (0 when profiling is off). */
uint32_t sel_gui_prof_begin(sel_gui_canvas* c);
void sel_gui_prof_end(sel_gui_canvas* c, uint32_t cls, uint32_t t0);
