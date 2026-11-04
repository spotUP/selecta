/* I1079a: the one column splitter for row text widths, shared by the desktop and MUI lists (design
 * thoughts/shared/plans/2026-11-05-I1079-row-text-widths.md section 3.1). Pure C99, no allocation. */
#ifndef SELECTA_UIMODEL_COLS_H
#define SELECTA_UIMODEL_COLS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_UIM_COLS_MAX 8

typedef struct {
    int32_t weight; /* > 0: a flex column, takes this share of the room left after fixed columns */
    int32_t fixed;  /* > 0 (weight 0): a fixed column, exactly this wide */
    int32_t min_w;  /* flex only: never narrower than this while the row allows it */
    int32_t max_w;  /* flex only, 0 = none: the widest content; room past it goes to the other flex columns */
} sel_uim_col;      /* weight 0 and fixed 0: the column is absent: width 0, no gap */

/* Splits row_w into n (at most SEL_UIM_COLS_MAX) columns left to right with `gap` between present columns. Writes x
 * (relative to the row) and w per column; the last present column ends exactly at row_w when a flex column can take
 * the remainder. Returns the number of present columns. */
uint32_t sel_uim_cols_split(int32_t row_w, int32_t gap, const sel_uim_col* c, uint32_t n, int32_t* x, int32_t* w);
uint32_t sel_uim_cols_calls(void); /* sentinel for the reachability test */

#ifdef __cplusplus
}
#endif

#endif
