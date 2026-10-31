/* I669, I670: the Your Library kind filter, one model for every UI. The filter in force is an index 0..SEL_UIM_LFILTERS-1: 0 is
 * All, then one per kind chip (Playlists, Albums, Artists, Composers, Sources, Local Files). The desktop chips and the MUI
 * previous/next control both name, clamp, mask and read the list through these functions. Pure C99. */
#ifndef SELECTA_UIMODEL_LIBFILTER_H
#define SELECTA_UIMODEL_LIBFILTER_H

#include <selecta/engine.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_UIM_LFILTERS 7u /* All plus the six kind chips */

/* "All", "Playlists", "Albums", "Artists", "Composers", "Sources", "Local Files"; NULL past the count. */
const char* sel_uim_lfilter_label(uint32_t filter);
/* The SEL_LIB_ITEM_BIT mask the library call takes for `filter`; All and any index past the count give SEL_LIB_ITEM_ALL. */
uint32_t sel_uim_lfilter_mask(uint32_t filter);
/* One step of the previous (dir < 0) or next (dir > 0) control from `filter`, clamped: it stops at All and at the last chip. */
uint32_t sel_uim_lfilter_step(uint32_t filter, int dir);
/* The filter pass: the rows of the kinds `filter` names, read from the library store (the store filters, not a UI loop). Returns the
 * rows written to out[0..max). *total, when not NULL, is the row count of the whole filtered list. */
uint32_t sel_uim_lfilter_pass(sel_engine* e, uint32_t filter, sel_engine_library_item* out, uint32_t max, uint32_t* total);
/* I669: chips or the single label. The kind chips show side by side only when they fit the sidebar; otherwise (and always on the
 * bitmap tier) one label names the filter in force and the Previous and Next chevrons step it, and the label takes no click. Both UIs
 * measure their chip widths in their own units and ask here. total is the chips' summed width with gaps, band the width they may use. */
int32_t sel_uim_lfilter_chips_total(const int32_t* widths, uint32_t n, int32_t gap);
int sel_uim_lfilter_chips_single(int bitmap, int32_t total, int32_t band);
/* I831: the label mode control is "< label >" (Spotify). With All in force the left button is Previous Filter; while any other filter
 * is in force it is Clear Filter (an x) and returns to All. Both UIs ask here for which one to show, its name, and what a press of the
 * left (dir < 0) or right (dir > 0) button gives; the right button always steps on (clamped). */
int sel_uim_lfilter_left_clears(uint32_t filter);
const char* sel_uim_lfilter_left_name(uint32_t filter);
uint32_t sel_uim_lfilter_single_step(uint32_t filter, int dir);
/* Passes run through sel_uim_lfilter_pass since start: the call-count sentinel the reachability tests read. */
uint32_t sel_uim_lfilter_passes(void);

#ifdef __cplusplus
}
#endif

#endif
