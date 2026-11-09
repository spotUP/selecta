/* I1222e: the sidebar stack (src/uimodel/sidestack.c). Order and visibility of the sidebar sections, shared by the desktop, MUI and
 * text UIs: brand, doors, library header, chips, tools, list. Units are the caller's (px, cells). Pure C99, no state.
 * Design: thoughts/shared/research/2026-11-09-text-frame-design.md section 4.3. */
#ifndef SELECTA_UIMODEL_SIDESTACK_H
#define SELECTA_UIMODEL_SIDESTACK_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    SEL_UIM_SIDE_BRAND,
    SEL_UIM_SIDE_DOOR,
    SEL_UIM_SIDE_LIBHDR,
    SEL_UIM_SIDE_CHIPS,
    SEL_UIM_SIDE_TOOLS,
    SEL_UIM_SIDE_LIST
} sel_uim_side_kind;

typedef struct {
    sel_uim_side_kind kind;
    int32_t y, h;
    int32_t door; /* the door index (sel_uim_door_at) for SEL_UIM_SIDE_DOOR, else -1 */
} sel_uim_side_item;

typedef struct {
    int32_t brand_h, door_h, libhdr_h, chip_row_h, tools_h, list_row_h;
    int32_t brand_min_w; /* the brand row needs at least this width */
    int rail;            /* icon rail: doors only */
    int top_bar;         /* top bar shown: the doors it owns leave the sidebar (sel_uim_door_shown_in) */
    int32_t chip_rows;   /* rows the chips take (sel_uim_chips_flow); 0 hides the chips */
} sel_uim_side_spec;

/* The minimum list rows the brand row may cost (decision 4 of the design): fewer than this and the brand is dropped. */
#define SEL_UIM_SIDE_MIN_LIST_ROWS 3

/* Fills out[0..max) top to bottom inside width w and height h; LIST takes what is left (h 0 when nothing is). The brand is left out when
 * w < brand_min_w or when with it the list would get fewer than SEL_UIM_SIDE_MIN_LIST_ROWS rows. Returns the item count. */
uint32_t sel_uim_side_stack(const sel_uim_side_spec* s, int32_t w, int32_t h, sel_uim_side_item* out, uint32_t max);

#ifdef __cplusplus
}
#endif
#endif
