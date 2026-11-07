/* I685w (design 2026-11-06-mui-mcc-package.md section 5.2 item 4): the span logic of every MCC fill, pure C99 so the host tests reach
 * it. mui_aa_draw.c binds the three ops to p96RectFill / FillPixelArray (rect_rgb), SetAPen + RectFill (rect_pen) and ObtainBestPen
 * (pen). On a truecolour screen (ops->tc) every span goes out as exact RGB and no pen is asked for; otherwise each span asks the pen
 * for its colour and draws through rect_pen (a pen of -1 skips the span). The rounding maths (mui_aa_round_inset) is used from here. */
#ifndef MUI_AA_PAINT_H
#define MUI_AA_PAINT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mui_aa_paint_ops {
    void* ctx;
    int tc;
    void (*rect_rgb)(void* ctx, int x0, int y0, int x1, int y1, uint32_t rgb);
    void (*rect_pen)(void* ctx, int x0, int y0, int x1, int y1, long pen);
    long (*pen)(void* ctx, uint32_t rgb);
} mui_aa_paint_ops;

/* One inclusive span in rgb; an empty span (x1 < x0 or y1 < y0) draws nothing. */
void mui_aa_paint_rect(const mui_aa_paint_ops* ops, int x0, int y0, int x1, int y1, uint32_t rgb);
/* Rows with the insets of mui_aa_round_inset; radius 0 is one span. */
void mui_aa_paint_round_fill(const mui_aa_paint_ops* ops, int x, int y, int w, int h, int radius, uint32_t rgb);
/* round_fill that leaves the hole rectangle (hx, hy, hw, hh) untouched. */
void mui_aa_paint_round_fill_hole(const mui_aa_paint_ops* ops, int x, int y, int w, int h, int radius, uint32_t rgb, int hx, int hy, int hw, int hh);
#define MUI_AA_PAINT_MAX_HOLES 64
/* round_fill that leaves up to MUI_AA_PAINT_MAX_HOLES rectangles untouched; holes is n quads x, y, w, h. */
void mui_aa_paint_round_fill_holes(const mui_aa_paint_ops* ops, int x, int y, int w, int h, int radius, uint32_t rgb, const int* holes, int n);
/* Only the four corner wedges outside the rounded shape. */
void mui_aa_paint_round_corners(const mui_aa_paint_ops* ops, int x, int y, int w, int h, int radius, uint32_t rgb);
/* One pixel rounded outline. */
void mui_aa_paint_round_frame(const mui_aa_paint_ops* ops, int x, int y, int w, int h, int radius, uint32_t rgb);
/* One row of n pixels at (x, y): slot[i] 0 is transparent, slot k > 0 draws slot_rgb[k]; equal neighbours merge into one span. */
void mui_aa_paint_runs(const mui_aa_paint_ops* ops, int x, int y, const unsigned char* slot, int n, const uint32_t* slot_rgb);

#ifdef __cplusplus
}
#endif

#endif
