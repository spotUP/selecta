/* I1473g8b1: the one Cover Flow geometry, shared by the desktop flow and the MUI canvas flow (design
 * thoughts/shared/designs/2026-11-20-i1473g8b-mui-cover-flow.md section 2). Pure C99, no allocation, no theme. */
#ifndef SELECTA_UIMODEL_FLOW_H
#define SELECTA_UIMODEL_FLOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { SEL_UIM_FLOW_SIDES = 3, SEL_UIM_FLOW_MAX = 2 * SEL_UIM_FLOW_SIDES + 4 };

typedef struct sel_uim_flow_cover {
    uint32_t g;    /* index into the view window (0 .. row_n-1) */
    int32_t x, y;  /* top left of the square, in the caller's coordinates */
    int32_t sz;    /* side of the square */
    int32_t alpha; /* scrim 0..150 */
    int32_t a;     /* |p| in 16.16 slot distance */
} sel_uim_flow_cover;

/* Width of the whole stack at centre size big: the two outermost covers' outer edges. */
int32_t sel_uim_flow_stack_w(int32_t big);

/* Centre size for a free area: the stack fills the width or the height (minus the caption block `capt`), never below `floor`. */
int32_t sel_uim_flow_big(int32_t avail_w, int32_t area_h, int32_t capt, int32_t ref, int32_t floor);

/* One cover at slot distance a (16.16, >= 0): size, offset from the centre line and scrim, continuous in a.
 * Whole slots are 100/80/60/40 % of big, offsets 0/.25/.5/.75 x big, scrim 0/90/150/150. */
void sel_uim_flow_slot(int32_t big, int32_t a_q16, int32_t* sz, int32_t* off, int32_t* alpha);

/* Fractional centre (16.16): the cursor at rest, gliding from `from_q16` to it as glide_q16 runs 0..65536. */
int64_t sel_uim_flow_centre_q16(int64_t from_q16, uint32_t cursor, int32_t glide_q16);

/* The visible covers in draw order (far to near, the front cover last). cx = centre line x, y0 = top of the centre cover.
 * The window holds absolute indices window_start .. window_start + row_n - 1. Returns the count (<= min(cap, SEL_UIM_FLOW_MAX)). */
uint32_t sel_uim_flow_layout(int64_t centre_q16, uint32_t window_start, uint32_t row_n, int32_t big, int32_t cx, int32_t y0,
                             sel_uim_flow_cover* out, uint32_t cap);

/* Window index of the cover under (x, y) among c[0..n) (front = last drawn wins), or -1. front_only: only c[n-1] counts. */
int32_t sel_uim_flow_hit(const sel_uim_flow_cover* c, uint32_t n, int32_t x, int32_t y, int front_only);

#ifdef __cplusplus
}
#endif

#endif
